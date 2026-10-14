#pragma once
#include "data.h"
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

class Caldav : public QObject {
    Q_OBJECT
public:
    explicit Caldav(QObject *parent = nullptr,QNetworkAccessManager *transport=nullptr);
    ~Caldav() override;
    void connectAccount(const QString &account, const QByteArray &appPassword);
    void fetch(const QList<Calendar> &calendars, QDate month);
    void disconnectAccount();
    void cancelFetch();
signals:
    void calendarsReady(QList<Calendar> calendars);
    void eventsReady(QList<Event> events, QDate month);
    void failed(QString message);
    void authorizationRejected();
private:
    QNetworkAccessManager ownedNetwork;
    QNetworkAccessManager *network;
    QByteArray authorization;
    quint64 generation = 0;
    bool fetching = false;
    QDate activeMonth, pendingMonth;
    QList<Calendar> pendingCalendars;
    struct CachedCalendar {QString token;QList<Event> events;};
    QMap<QString,CachedCalendar> cache;
    QDate cacheMonth;
    using Callback = std::function<void(const QByteArray &, const QUrl &)>;
    void request(QUrl url, const QByteArray &method, const QByteArray &body,
                 const QByteArray &depth, Callback done, int redirects = 0,bool optionalProbe=false);
    void readCalendar(QList<Calendar> calendars,int index,QDate first,QDate last,QList<Event> events,quint64 token,QString changeToken);
    void reportNext(QList<Calendar> calendars, int index, QDate first, QDate last,
                    QList<Event> events, quint64 requestGeneration);
};
