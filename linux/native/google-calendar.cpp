#include "google-calendar.h"
#include "profile.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QTimeZone>
#include <QUrlQuery>
#include <algorithm>
namespace {
bool secretText(const QJsonValue &v,int limit){return v.isString()&&!v.toString().isEmpty()&&v.toString().size()<=limit&&QRegularExpression("^[!-~]+$").match(v.toString()).hasMatch();}
QDateTime googleTime(const QJsonObject &value){
    if(value["date"].isString()&&!value.contains("dateTime")){const auto date=QDate::fromString(value["date"].toString(),Qt::ISODate);return date.isValid()?QDateTime(date,QTime(0,0),configuredZone()):QDateTime();}
    if(!value["dateTime"].isString()||value.contains("date"))return {};
    const auto text=value["dateTime"].toString();if(!QRegularExpression("(?:Z|[+-][0-9]{2}:[0-9]{2})$").match(text).hasMatch())return {};
    return QDateTime::fromString(text,Qt::ISODateWithMs).toTimeZone(configuredZone());
}
}
std::optional<GoogleCredentials> GoogleCredentials::parse(const QJsonObject &v){
    if(v.size()!=3||!secretText(v["client_id"],254)||!v["client_id"].toString().endsWith(".apps.googleusercontent.com")||!secretText(v["client_secret"],512)||!secretText(v["refresh_token"],2048))return std::nullopt;
    return GoogleCredentials(v["client_id"].toString(),v["client_secret"].toString(),v["refresh_token"].toString());
}
QByteArray GoogleCredentials::refreshBody()const{QUrlQuery q;q.addQueryItem("client_id",clientId);q.addQueryItem("client_secret",clientSecret);q.addQueryItem("refresh_token",refreshToken);q.addQueryItem("grant_type","refresh_token");return q.query(QUrl::FullyEncoded).toUtf8().replace("+","%2B");}
QUrl googleCalendarUrl(const QString &id){QUrl url;url.setScheme("googlecalendar");url.setPath('/'+id);return url;}
bool isGoogleCalendarUrl(const QUrl &url){return url.isValid()&&url.scheme()=="googlecalendar"&&url.host().isEmpty()&&url.userInfo().isEmpty()&&!url.hasQuery()&&!url.hasFragment()&&url.path().startsWith('/')&&url.path().size()>1&&url.path().size()<=1025&&!url.path().contains(QChar(0));}
std::optional<QList<Event>> parseGoogleEvents(const QJsonObject &page,const Calendar &calendar){
    if(!page["items"].isArray()||page["items"].toArray().size()>2500)return std::nullopt;
    QList<Event> result;
    for(const auto &v:page["items"].toArray()){
        if(!v.isObject())return std::nullopt;
        const auto e=v.toObject();if(e["status"].toString()=="cancelled")continue;
        const auto start=googleTime(e["start"].toObject()),end=googleTime(e["end"].toObject());
        const bool allDay=e["start"].toObject().contains("date");
        if(!e["id"].isString()||e["id"].toString().isEmpty()||e["id"].toString().size()>1024||!start.isValid()||!end.isValid()||end<=start||allDay!=e["end"].toObject().contains("date"))return std::nullopt;
        result<<Event{"google:"+calendar.url.path()+":"+e["id"].toString(),e["summary"].toString("Untitled event").left(500),e["location"].toString().left(500),calendar.name,start,end,allDay,calendarIdentity(calendar),canonicalCalendarColour(calendar.colour)};
    }
    return result;
}
GoogleCalendar::GoogleCalendar(QObject *parent,QNetworkAccessManager *transport):QObject(parent),network(transport?transport:&ownedNetwork){}
void GoogleCalendar::cancelFetch(){++generation;busy=false;}
void GoogleCalendar::disconnectAccount(){++generation;credentials.reset();token.fill('\0');token.clear();expires={};busy=false;}
void GoogleCalendar::connectAccount(const GoogleCredentials &value){disconnectAccount();credentials=value;discover();}
void GoogleCalendar::fail(QString message,bool rejected){busy=false;if(rejected){token.fill('\0');token.clear();credentials.reset();emit authorizationRejected();}emit failed(message);}
void GoogleCalendar::ready(std::function<void()> done){
    if(!credentials){fail("Connect Google using the Mac setup helper.");return;}
    if(!token.isEmpty()&&expires>QDateTime::currentDateTimeUtc().addSecs(60)){done();return;}
    request(QUrl("https://oauth2.googleapis.com/token"),credentials->refreshBody(),[this,done](const QJsonObject &v){
        if(!secretText(v["access_token"],4096)||v["token_type"].toString().compare("Bearer",Qt::CaseInsensitive)!=0||!v["expires_in"].isDouble()||v["expires_in"].toInt()<=60||v["expires_in"].toInt()>86400){fail("Google token response could not be read.");return;}
        if(v.contains("scope")&&!v["scope"].toString().split(' ').contains("https://www.googleapis.com/auth/calendar.readonly")){fail("Google Calendar read-only permission is missing.",true);return;}
        token=v["access_token"].toString().toUtf8();expires=QDateTime::currentDateTimeUtc().addSecs(v["expires_in"].toInt());done();
    },true);
}
void GoogleCalendar::request(QUrl url,QByteArray body,Callback done,bool refreshing,bool retry){
    const auto current=generation;
    if(url.scheme()!="https"||!url.userInfo().isEmpty()||url.hasFragment()||url.port(443)!=443||(refreshing?url.host()!="oauth2.googleapis.com":url.host()!="www.googleapis.com")){fail("Google request was blocked.");return;}
    QNetworkRequest networkRequest(url);networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);networkRequest.setTransferTimeout(20000);networkRequest.setRawHeader("Accept","application/json");
    if(!refreshing)networkRequest.setRawHeader("Authorization","Bearer "+token);
    else networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    auto *reply=refreshing?network->post(networkRequest,body):network->get(networkRequest);body.fill('\0');auto bytes=QSharedPointer<QByteArray>::create();
    connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{bytes->append(reply->readAll());if(bytes->size()>4*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,current,url,done,refreshing,retry]{
        bytes->append(reply->readAll());const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto error=reply->error();reply->deleteLater();
        if(current!=generation){bytes->fill('\0');return;}
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(*bytes,&parse);bytes->fill('\0');
        if(code==401&&!refreshing&&retry){token.clear();ready([this,url,done]{request(url,{},done,false,false);});return;}
        if((refreshing&&doc.object()["error"].toString()=="invalid_grant")||code==401){fail("Google sign-in expired or was revoked. Reconnect using the Mac helper.",true);return;}
        if(error!=QNetworkReply::NoError||code!=200||bytes->size()>4*1024*1024||parse.error!=QJsonParseError::NoError||!doc.isObject()){fail(code==403?"Google Calendar access was denied. Check Workspace app permissions.":"Google Calendar refresh failed. It will retry automatically.");return;}
        done(doc.object());
    });
}
void GoogleCalendar::discover(){if(busy||!credentials)return;busy=true;ready([this]{listPage({},{});});}
void GoogleCalendar::listPage(QString page,QList<Calendar> calendars,int pages){
    if(pages>=10){fail("Google calendar list exceeded the supported limit.");return;}
    QUrl url("https://www.googleapis.com/calendar/v3/users/me/calendarList");QUrlQuery query;query.addQueryItem("maxResults","250");query.addQueryItem("minAccessRole","reader");query.addQueryItem("fields","items(id,summary,primary,deleted,backgroundColor),nextPageToken");if(!page.isEmpty())query.addQueryItem("pageToken",page);url.setQuery(query);
    request(url,{},[this,calendars,pages](const QJsonObject &value)mutable{
        if(!value["items"].isArray()){fail("Google calendar list could not be read.");return;}
        for(const auto &v:value["items"].toArray()){const auto item=v.toObject();const auto id=item["id"].toString();if(item["deleted"].toBool())continue;if(id.isEmpty()||id.size()>1024||calendars.size()>=250){fail("Google calendar list exceeded the supported limit.");return;}Calendar c{googleCalendarUrl(id),item["summary"].toString("Google calendar").left(100),canonicalCalendarColour(item["backgroundColor"].toString())};if(item["primary"].toBool())calendars.prepend(c);else calendars<<c;}
        const auto next=value["nextPageToken"].toString();if(next.size()>2048){fail("Google calendar list could not be read.");return;}if(!next.isEmpty()){listPage(next,calendars,pages+1);return;}busy=false;emit calendarsReady(calendars);
    });
}
void GoogleCalendar::fetch(const QList<Calendar> &calendars,QDate month){
    if(busy||!credentials||calendars.isEmpty()||calendars.size()>16||!month.isValid())return;
    for(const auto &c:calendars)if(!isGoogleCalendarUrl(c.url))return;
    busy=true;ready([this,calendars,month]{eventPage(calendars,0,QDate(month.year(),month.month(),1),{},{ });});
}
void GoogleCalendar::eventPage(QList<Calendar> calendars,int index,QDate month,QString page,QList<Event> events,int pages){
    if(pages>=10||events.size()>10000){fail("Google events exceeded the supported limit.");return;}
    if(index>=calendars.size()){busy=false;emit eventsReady(events,month);return;}
    const auto c=calendars[index];QUrl url("https://www.googleapis.com/calendar/v3/calendars/"+QString::fromLatin1(QUrl::toPercentEncoding(c.url.path().mid(1)))+"/events");
    QUrlQuery query;query.addQueryItem("timeMin",QDateTime(month,QTime(0,0),configuredZone()).toString(Qt::ISODate));query.addQueryItem("timeMax",QDateTime(month.addMonths(1).addDays(4),QTime(0,0),configuredZone()).toString(Qt::ISODate));query.addQueryItem("timeZone",QString::fromUtf8(configuredZone().id()));query.addQueryItem("singleEvents","true");query.addQueryItem("orderBy","startTime");query.addQueryItem("maxResults","2500");query.addQueryItem("fields","items(id,status,summary,location,start,end),nextPageToken");if(!page.isEmpty())query.addQueryItem("pageToken",page);url.setQuery(query);
    request(url,{},[this,calendars,index,month,events,pages,c](const QJsonObject &value)mutable{const auto parsed=parseGoogleEvents(value,c);if(!parsed){fail("Google event dates could not be read.");return;}events.append(*parsed);const auto next=value["nextPageToken"].toString();if(next.size()>2048){fail("Google events could not be read.");return;}eventPage(calendars,next.isEmpty()?index+1:index,month,next,events,next.isEmpty()?0:pages+1);});
}
