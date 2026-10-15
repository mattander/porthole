#include "sun-theme.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace {
QDateTime solarTime(const QString &value,const QTimeZone &zone){
    if(value.size()!=16||value[10]!=QChar('T'))return {};
    const auto date=QDate::fromString(value.left(10),Qt::ISODate);const auto time=QTime::fromString(value.mid(11),"HH:mm");
    return date.isValid()&&time.isValid()?QDateTime(date,time,zone):QDateTime();
}
}
SunThemeSchedule SunThemeSchedule::fromWeather(const QJsonObject &daily,const QTimeZone &zone){
    SunThemeSchedule result;if(!zone.isValid())return result;
    const auto dates=daily["time"].toArray(),rises=daily["sunrise"].toArray(),sets=daily["sunset"].toArray();QSet<QDate> seen;
    for(int i=0;i<qMin(4,dates.size());++i){
        if(i>=rises.size()||i>=sets.size())break;
        const auto date=QDate::fromString(dates[i].toString(),Qt::ISODate);
        const auto sunrise=solarTime(rises[i].toString(),zone),sunset=solarTime(sets[i].toString(),zone);
        if(!date.isValid()||!sunrise.isValid()||!sunset.isValid()||sunrise.date()!=date||sunset.date()!=date||sunrise>=sunset)continue;
        if(seen.contains(date))return {};
        seen.insert(date);result.days.append({sunrise,sunset});
    }
    std::sort(result.days.begin(),result.days.end(),[](const Day &a,const Day &b){return a.sunrise<b.sunrise;});return result;
}
SunThemeSchedule SunThemeSchedule::fromCache(const QByteArray &bytes,const HouseholdProfile &profile){
    if(bytes.size()>4096)return {};
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())return {};
    const auto value=document.object();
    if(value["profileRevision"].toString()!=profileRevision(profile))return {};
    return fromWeather(value["daily"].toObject(),profile.zone());
}
QByteArray SunThemeSchedule::cache(const HouseholdProfile &profile)const{
    QJsonArray dates,rises,sets;
    for(const auto &day:days){dates.append(day.sunrise.date().toString(Qt::ISODate));rises.append(day.sunrise.toString("yyyy-MM-dd'T'HH:mm"));sets.append(day.sunset.toString("yyyy-MM-dd'T'HH:mm"));}
    return QJsonDocument(QJsonObject{{"profileRevision",profileRevision(profile)},{"daily",QJsonObject{{"time",dates},{"sunrise",rises},{"sunset",sets}}}}).toJson(QJsonDocument::Compact);
}
std::optional<bool> SunThemeSchedule::darkAt(const QDateTime &now)const{
    if(!now.isValid())return std::nullopt;
    for(const auto &day:days)if(now.toTimeZone(day.sunrise.timeZone()).date()==day.sunrise.date())return now<day.sunrise||now>=day.sunset;
    return std::nullopt;
}
QString SunThemeSchedule::descriptionAt(const QDateTime &now)const{
    const auto dark=darkAt(now);if(!dark)return "Waiting for today's sun times. Keeping the current theme.";
    for(const auto &day:days){
        const auto date=now.toTimeZone(day.sunrise.timeZone()).date();if(date!=day.sunrise.date())continue;
        if(now<day.sunrise)return "Dark until sunrise at "+day.sunrise.toString("h:mm AP");
        if(now<day.sunset)return "Light until sunset at "+day.sunset.toString("h:mm AP");
        for(const auto &next:days)if(next.sunrise.date()==date.addDays(1))return "Dark until sunrise at "+next.sunrise.toString("h:mm AP");
        return "Dark after sunset";
    }
    return {};
}
