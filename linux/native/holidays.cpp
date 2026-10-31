#include "holidays.h"
#include "calendar-sources.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <libical/ical.h>
#include <algorithm>
#include <memory>

namespace {
constexpr qsizetype MaximumCalendar=256*1024;
const QUrl Feed("https://calendars.icloud.com/holidays/ca_en.ics/");
struct Province {QString code,name;};
const QList<Province> Provinces{{"AB","Alberta"},{"BC","British Columbia"},{"MB","Manitoba"},{"NB","New Brunswick"},{"NL","Newfoundland and Labrador"},{"NS","Nova Scotia"},{"NT","Northwest Territories"},{"NU","Nunavut"},{"ON","Ontario"},{"PE","Prince Edward Island"},{"QC","Quebec"},{"SK","Saskatchewan"},{"YT","Yukon"}};
QString province(const HouseholdProfile &profile){
    if(!profile.weather())return {};
    const auto region=profile.weather()->label().split(',').last().trimmed();
    for(const auto &p:Provinces)if(region.compare(p.code,Qt::CaseInsensitive)==0||region.compare(p.name,Qt::CaseInsensitive)==0)return p.code;
    return {};
}
bool applies(const QString &title,const QString &region){
    static const QRegularExpression annotation("\\(([A-Z]{2}(?:,\\s*[A-Z]{2})*)\\)");
    auto matches=annotation.globalMatch(title);
    while(matches.hasNext()){
        const auto codes=matches.next().captured(1).split(QRegularExpression(",\\s*"));
        if(std::all_of(codes.begin(),codes.end(),[](const QString &code){return std::any_of(Provinces.begin(),Provinces.end(),[&](const Province &p){return p.code==code;});}))return codes.contains(region);
    }
    return true;
}
std::optional<QDate> day(icaltimetype value){
    const QDate result(value.year,value.month,value.day);
    if(!icaltime_is_valid_time(value)||!value.is_date||!result.isValid())return std::nullopt;
    return result;
}
QString titleKey(QString value){return value.normalized(QString::NormalizationForm_KC).toCaseFolded().replace(QChar(0x2019),'\'').simplified();}
QByteArray calendarWithoutTimestamps(const QByteArray &bytes){
    // The provider uses date-only DTSTAMP metadata, which libical rejects.
    // We use our fetch time for freshness; event dates still receive full validation.
    QList<QByteArray> unfolded;
    for(auto line:bytes.split('\n')){
        if(line.endsWith('\r'))line.chop(1);
        if(!unfolded.isEmpty()&&(line.startsWith(' ')||line.startsWith('\t')))unfolded.last()+=line.mid(1);
        else unfolded.append(line);
    }
    QByteArray result;
    for(const auto &line:unfolded){
        const auto name=line.split(':').first().split(';').first().toUpper();
        if(name!="DTSTAMP")result+=line+"\r\n";
    }
    return result;
}
}

std::optional<QList<Event>> parsePublicHolidays(const QByteArray &bytes,QDate first,QDate last,const HouseholdProfile &profile){
    if(bytes.isEmpty()||bytes.size()>MaximumCalendar||bytes.contains('\0')||!first.isValid()||!last.isValid()||last<first||first.daysTo(last)>370)return std::nullopt;
    using Component=std::unique_ptr<icalcomponent,decltype(&icalcomponent_free)>;
    const auto normalized=calendarWithoutTimestamps(bytes);
    Component root(icalparser_parse_string(normalized.constData()),icalcomponent_free);
    if(!root||icalcomponent_isa(root.get())!=ICAL_VCALENDAR_COMPONENT||icalcomponent_count_errors(root.get()))return std::nullopt;
    QList<Event> result;QSet<QString> added;int components=0;const auto region=province(profile);
    for(auto *item=icalcomponent_get_first_component(root.get(),ICAL_VEVENT_COMPONENT);item;item=icalcomponent_get_next_component(root.get(),ICAL_VEVENT_COMPONENT)){
        if(++components>1000)return std::nullopt;
        if(icalcomponent_get_status(item)==ICAL_STATUS_CANCELLED)continue;
        if(icalcomponent_get_first_property(item,ICAL_RECURRENCEID_PROPERTY))return std::nullopt;
        const auto start=icalcomponent_get_dtstart(item);const auto startDay=day(start);
        const auto *summary=icalcomponent_get_summary(item),*uid=icalcomponent_get_uid(item);
        if(!startDay||!summary||!uid)return std::nullopt;
        const auto title=QString::fromUtf8(summary).simplified(),identity=QString::fromUtf8(uid);
        if(title.isEmpty()||title.size()>240||identity.isEmpty()||identity.size()>500)return std::nullopt;
        int duration=1;
        if(const auto *end=icalcomponent_get_first_property(item,ICAL_DTEND_PROPERTY)){
            const auto endDay=day(icalproperty_get_dtend(end));if(!endDay)return std::nullopt;
            const auto days=startDay->daysTo(*endDay);if(days<1||days>7)return std::nullopt;duration=days;
        }else if(icalcomponent_get_first_property(item,ICAL_DURATION_PROPERTY)){
            const auto seconds=icaldurationtype_as_seconds(icalcomponent_get_duration(item));
            if(seconds<86400||seconds>7*86400||seconds%86400)return std::nullopt;
            duration=seconds/86400;
        }
        QSet<QDate> excluded;
        for(auto *p=icalcomponent_get_first_property(item,ICAL_EXDATE_PROPERTY);p;p=icalcomponent_get_next_property(item,ICAL_EXDATE_PROPERTY)){
            const auto date=day(icalproperty_get_exdate(p));if(!date||excluded.size()>=1000)return std::nullopt;excluded.insert(*date);
        }
        QList<QDate> dates{*startDay};
        if(icalcomponent_count_properties(item,ICAL_RRULE_PROPERTY)>1)return std::nullopt;
        if(auto *p=icalcomponent_get_first_property(item,ICAL_RRULE_PROPERTY)){
            auto *rule=icalproperty_get_rrule(p);if(!rule||rule->freq!=ICAL_YEARLY_RECURRENCE)return std::nullopt;
            using Iterator=std::unique_ptr<icalrecur_iterator,decltype(&icalrecur_iterator_free)>;
            Iterator iterator(icalrecur_iterator_new(rule,start),icalrecur_iterator_free);if(!iterator)return std::nullopt;
            bool complete=false;
            for(int n=0;n<500;++n){
                const auto next=icalrecur_iterator_next(iterator.get());if(icaltime_is_null_time(next)){complete=true;break;}
                const auto date=day(next);if(!date)return std::nullopt;
                if(*date>last){complete=true;break;}dates.append(*date);
            }
            if(!complete)return std::nullopt;
        }
        for(auto *p=icalcomponent_get_first_property(item,ICAL_RDATE_PROPERTY);p;p=icalcomponent_get_next_property(item,ICAL_RDATE_PROPERTY)){
            const auto date=day(icalproperty_get_rdate(p).time);if(!date||dates.size()>=1000)return std::nullopt;dates.append(*date);
        }
        for(const auto date:dates){
            if(excluded.contains(date)||date>last||date.addDays(duration)<=first||!applies(title,region))continue;
            const auto reference="holiday:"+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex())+":"+date.toString(Qt::ISODate);
            if(added.contains(reference))continue;
            added.insert(reference);
            QString description="Holiday or observance from Apple's Canadian Holidays calendar.\nSource: "+Feed.toString();
            if(const auto *notes=icalcomponent_get_description(item))description+='\n'+boundedEventDescription(QString::fromUtf8(notes));
            result.append(Event{reference,title,{},"Canadian holidays",QDateTime(date,QTime(0,0),profile.zone()),QDateTime(date.addDays(duration),QTime(0,0),profile.zone()),true,"holidays:CA","#6B8E7D",description});
            if(result.size()>1000)return std::nullopt;
        }
    }
    if(!components)return std::nullopt;
    return mergeCalendarEvents(result,{});
}

QList<Event> mergePublicHolidays(QList<Event> personal,const QList<Event> &holidays){
    for(const auto &event:holidays)if(!std::any_of(personal.begin(),personal.end(),[&](const Event &existing){return existing.allDay&&existing.start.date()==event.start.date()&&existing.end.date()==event.end.date()&&titleKey(existing.title)==titleKey(event.title);}))personal.append(event);
    return mergeCalendarEvents(personal,{});
}

HolidayCalendar::HolidayCalendar(QString cachePath,QObject *parent,QNetworkAccessManager *transport,std::function<QDateTime()> time):
    QObject(parent),path(std::move(cachePath)),ownedNetwork(this),network(transport?transport:&ownedNetwork),clock(std::move(time)){
    QFile file(path);
    if(file.open(QIODevice::ReadOnly)&&file.size()<=2*MaximumCalendar){
        QJsonParseError error;const auto value=QJsonDocument::fromJson(file.readAll(),&error).object();
        const auto fetched=QDateTime::fromString(value["fetched"].toString(),Qt::ISODate);const auto data=value["calendar"].toString().toUtf8();
        if(error.error==QJsonParseError::NoError&&value.size()==3&&value["version"]==QJsonValue(1)&&value["calendar"].isString()&&fetched.isValid()&&!data.isEmpty()&&data.size()<=MaximumCalendar)cache=Cache{data,fetched};
    }
}
QDateTime HolidayCalendar::now()const{return clock?clock():QDateTime::currentDateTimeUtc();}
bool HolidayCalendar::enabled()const{return profile.zone().territory()==QLocale::Canada;}
QList<Event> HolidayCalendar::events(QDate first,QDate last)const{
    if(!enabled()||!cache)return {};
    return parsePublicHolidays(cache->calendar,first,last,profile).value_or(QList<Event>{});
}
QString HolidayCalendar::status()const{
    if(!enabled())return {};
    const auto location=province(profile);const auto label="Canadian holidays"+(location.isEmpty()?QString():" ("+location+")");
    if(!cache)return label+" · unavailable";
    if(failure==Failure::Storage)return label+" · available, cache could not be saved";
    return label+(failure==Failure::Refresh?" · saved dates, refresh unavailable":" · available");
}
bool HolidayCalendar::save()const{
    if(!cache)return false;
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))return false;
    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    const auto data=QJsonDocument(QJsonObject{{"version",1},{"fetched",cache->fetched.toString(Qt::ISODate)},{"calendar",QString::fromUtf8(cache->calendar)}}).toJson(QJsonDocument::Compact);
    return data.size()<=2*MaximumCalendar&&file.write(data)==data.size()&&file.commit();
}
void HolidayCalendar::refresh(const HouseholdProfile &next){
    const bool changedProfile=profile.zone()!=next.zone()||province(profile)!=province(next);
    profile=next;
    if(changedProfile){++generation;if(active){active->abort();active=nullptr;}emit changed();}
    if(!enabled())return;
    const auto time=now();const auto year=time.toTimeZone(profile.zone()).date().year();
    if(cache){const auto parsed=parsePublicHolidays(cache->calendar,QDate(year,1,1),QDate(year,12,31),profile);if(!parsed||parsed->isEmpty()){cache.reset();failure=Failure::Refresh;emit changed();}}
    if(cache&&cache->fetched.secsTo(time)>=0&&cache->fetched.secsTo(time)<86400)return;
    if(active||(lastAttempt.isValid()&&lastAttempt.secsTo(time)>=0&&lastAttempt.secsTo(time)<300))return;
    lastAttempt=time;
    QNetworkRequest request(Feed);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(30000);
    auto *reply=network->get(request);active=reply;reply->setReadBufferSize(MaximumCalendar+1);const auto token=generation;
    connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>MaximumCalendar)reply->abort();});
    auto *deadline=new QTimer(reply);deadline->setSingleShot(true);connect(deadline,&QTimer::timeout,reply,&QNetworkReply::abort);deadline->start(35000);
    connect(reply,&QNetworkReply::finished,this,[this,reply,token,year]{
        reply->deleteLater();if(active==reply)active=nullptr;if(token!=generation)return;
        const auto bytes=reply->readAll();
        const auto parsed=reply->error()==QNetworkReply::NoError&&reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200?parsePublicHolidays(bytes,QDate(year,1,1),QDate(year,12,31),profile):std::nullopt;
        if(parsed&&!parsed->isEmpty()){cache=Cache{bytes,now()};if(save())failure.reset();else failure=Failure::Storage;}else failure=Failure::Refresh;
        emit changed();
    });
}
