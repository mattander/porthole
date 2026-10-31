#pragma once
#include "data.h"
#include "profile.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <functional>
#include <optional>

// Dates are inclusive; malformed or unsupported source data is rejected.
std::optional<QList<Event>> parsePublicHolidays(const QByteArray &calendar,QDate first,QDate last,const HouseholdProfile &profile);
QList<Event> mergePublicHolidays(QList<Event> personal,const QList<Event> &holidays);

class HolidayCalendar:public QObject {
    Q_OBJECT
public:
    HolidayCalendar(QString cachePath,QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr,std::function<QDateTime()> clock={});
    void refresh(const HouseholdProfile &profile);
    QList<Event> events(QDate first,QDate last)const;
    QString status()const;
signals:
    void changed();
private:
    struct Cache {QByteArray calendar;QDateTime fetched;};
    enum class Failure {Refresh,Storage};
    QString path;
    HouseholdProfile profile;
    std::optional<Cache> cache;
    QDateTime lastAttempt;
    std::optional<Failure> failure;
    QNetworkAccessManager ownedNetwork;
    QNetworkAccessManager *network;
    QPointer<QNetworkReply> active;
    std::function<QDateTime()> clock;
    quint64 generation=0;
    QDateTime now()const;
    bool enabled()const;
    bool save()const;
};
