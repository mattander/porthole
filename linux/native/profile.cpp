#include "profile.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrlQuery>
#include <cmath>
#include <unistd.h>

namespace {
HouseholdProfile currentProfile;
bool controls(const QString &value){for(const auto ch:value)if(ch.category()==QChar::Other_Control||ch.category()==QChar::Other_Format)return true;return false;}
bool keys(const QJsonObject &value,const QStringList &allowed){for(auto i=value.begin();i!=value.end();++i)if(!allowed.contains(i.key()))return false;return true;}
}
QString profileErrorMessage(ProfileError error){switch(error){
case ProfileError::Schema:return "The profile format is not supported.";
case ProfileError::Timezone:return "Enter a recognized timezone such as UTC or an IANA region/city name.";
case ProfileError::Context:return "Household context must contain at most 1600 characters, without control characters.";
case ProfileError::Weather:return "Enter a location name and valid latitude (-90 to 90) and longitude (-180 to 180).";
case ProfileError::Read:return "The private profile could not be read.";
}return {};}
std::variant<HouseholdProfile,ProfileError> HouseholdProfile::parse(const QJsonObject &value){
    if(!keys(value,{"version","timezone","familyContext","weather"})||value["version"]!=QJsonValue(1)||!value["timezone"].isString()||!value["familyContext"].isString())return ProfileError::Schema;
    const auto id=value["timezone"].toString().toUtf8();
    if(id.size()>100||!QRegularExpression("^[A-Za-z0-9_+/-]+$").match(QString::fromUtf8(id)).hasMatch()||!QTimeZone::isTimeZoneIdAvailable(id))return ProfileError::Timezone;
    const auto context=value["familyContext"].toString();
    if(context.toUcs4().size()>1600||controls(context))return ProfileError::Context;
    HouseholdProfile profile;profile.zoneValue=QTimeZone(id);profile.contextValue=context;
    const auto weather=value["weather"];
    if(weather.isNull())return profile;
    if(!weather.isObject())return ProfileError::Weather;
    const auto fields=weather.toObject();const auto label=fields["label"].toString().simplified();
    if(!keys(fields,{"label","latitude","longitude"})||!fields["label"].isString()||label.isEmpty()||label.size()>120||controls(label)||!fields["latitude"].isDouble()||!fields["longitude"].isDouble())return ProfileError::Weather;
    const auto lat=fields["latitude"].toDouble(),lon=fields["longitude"].toDouble();
    if(!std::isfinite(lat)||!std::isfinite(lon)||lat < -90||lat>90||lon < -180||lon>180)return ProfileError::Weather;
    profile.weatherValue=WeatherLocation(label,lat,lon);return profile;
}
QJsonObject HouseholdProfile::json()const{
    QJsonValue weather=QJsonValue::Null;if(weatherValue)weather=QJsonObject{{"label",weatherValue->label()},{"latitude",weatherValue->latitude()},{"longitude",weatherValue->longitude()}};
    return {{"version",1},{"timezone",QString::fromUtf8(zoneValue.id())},{"familyContext",contextValue},{"weather",weather}};
}
const HouseholdProfile &householdProfile(){return currentProfile;}
void setHouseholdProfile(const HouseholdProfile &profile){currentProfile=profile;}
QTimeZone configuredZone(){return currentProfile.zone();}
QString profileRevision(const HouseholdProfile &profile){return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(profile.json()).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
std::optional<QUrl> weatherForecastUrl(const HouseholdProfile &profile){
    if(!profile.weather())return std::nullopt;
    QUrl url("https://api.open-meteo.com/v1/forecast");QUrlQuery query;
    query.addQueryItem("latitude",QString::number(profile.weather()->latitude(),'g',12));query.addQueryItem("longitude",QString::number(profile.weather()->longitude(),'g',12));
    query.addQueryItem("current","temperature_2m,apparent_temperature,weather_code");query.addQueryItem("daily","weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset");
    query.addQueryItem("timezone",QString::fromUtf8(profile.zone().id()));query.addQueryItem("forecast_days","4");url.setQuery(query);return url;
}
std::variant<HouseholdProfile,ProfileError> loadProfile(const QString &path){
    QFile file(path);if(!file.exists())return HouseholdProfile();if(!file.open(QIODevice::ReadOnly)||file.size()>16384)return ProfileError::Read;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);if(error.error!=QJsonParseError::NoError||!doc.isObject())return ProfileError::Schema;return HouseholdProfile::parse(doc.object());
}
bool saveProfile(const QString &path,const HouseholdProfile &profile){
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))return false;file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    const auto bytes=QJsonDocument(profile.json()).toJson(QJsonDocument::Compact);
    return file.write(bytes)==bytes.size()&&file.flush()&&::fsync(file.handle())==0&&file.commit();
}
