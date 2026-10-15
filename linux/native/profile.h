#pragma once
#include <QJsonObject>
#include <QString>
#include <QTimeZone>
#include <QUrl>
#include <optional>
#include <variant>

class HouseholdProfile;
class WeatherLocation {
    friend class HouseholdProfile;
    QString labelValue;
    double latitudeValue,longitudeValue;
    WeatherLocation(QString label,double latitude,double longitude):labelValue(std::move(label)),latitudeValue(latitude),longitudeValue(longitude){}
public:
    QString label()const{return labelValue;}
    double latitude()const{return latitudeValue;}
    double longitude()const{return longitudeValue;}
};
enum class ProfileError { Schema, Timezone, Context, Weather, Read };
QString profileErrorMessage(ProfileError error);
class HouseholdProfile {
    QTimeZone zoneValue=QTimeZone::utc();
    QString contextValue;
    std::optional<WeatherLocation> weatherValue;
public:
    HouseholdProfile()=default;
    static std::variant<HouseholdProfile,ProfileError> parse(const QJsonObject &value);
    QJsonObject json()const;
    QTimeZone zone()const{return zoneValue;}
    QString familyContext()const{return contextValue;}
    const std::optional<WeatherLocation> &weather()const{return weatherValue;}
    bool operator==(const HouseholdProfile &other)const{return json()==other.json();}
};
const HouseholdProfile &householdProfile();
void setHouseholdProfile(const HouseholdProfile &profile);
QTimeZone configuredZone();
std::optional<QUrl> weatherForecastUrl(const HouseholdProfile &profile);
std::optional<QUrl> weatherRainUrl(const HouseholdProfile &profile);
QString profileRevision(const HouseholdProfile &profile);
std::variant<HouseholdProfile,ProfileError> loadProfile(const QString &path);
bool saveProfile(const QString &path,const HouseholdProfile &profile);
