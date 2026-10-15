#pragma once
#include "profile.h"
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <optional>

class SunThemeSchedule {
    struct Day {QDateTime sunrise,sunset;};
    QList<Day> days;
public:
    static SunThemeSchedule fromWeather(const QJsonObject &daily,const QTimeZone &zone);
    static SunThemeSchedule fromCache(const QByteArray &bytes,const HouseholdProfile &profile);
    QByteArray cache(const HouseholdProfile &profile)const;
    bool isEmpty()const{return days.isEmpty();}
    std::optional<bool> darkAt(const QDateTime &now)const;
    QString descriptionAt(const QDateTime &now)const;
};
