#pragma once
#include "data.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>
#include <optional>

class GoogleCredentials {
public:
    static std::optional<GoogleCredentials> parse(const QJsonObject &value);
    QByteArray refreshBody() const;
private:
    QString clientId,clientSecret,refreshToken;
    GoogleCredentials(QString id,QString secret,QString token):clientId(std::move(id)),clientSecret(std::move(secret)),refreshToken(std::move(token)){}
};
QUrl googleCalendarUrl(const QString &id);
bool isGoogleCalendarUrl(const QUrl &url);
std::optional<QList<Event>> parseGoogleEvents(const QJsonObject &page,const Calendar &calendar);

class GoogleCalendar:public QObject {
    Q_OBJECT
public:
    explicit GoogleCalendar(QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr);
    void connectAccount(const GoogleCredentials &credentials);
    void disconnectAccount();
    void cancelFetch();
    void discover();
    void fetch(const QList<Calendar> &calendars,QDate month);
signals:
    void calendarsReady(QList<Calendar> calendars);
    void eventsReady(QList<Event> events,QDate month);
    void failed(QString message);
    void authorizationRejected();
private:
    QNetworkAccessManager ownedNetwork;
    QNetworkAccessManager *network;
    std::optional<GoogleCredentials> credentials;
    QByteArray token;
    QDateTime expires;
    quint64 generation=0;
    bool busy=false;
    using Callback=std::function<void(QJsonObject)>;
    void request(QUrl url,QByteArray body,Callback done,bool refreshing=false,bool retry=true);
    void ready(std::function<void()> done);
    void listPage(QString page,QList<Calendar> calendars,int pages=0);
    void eventPage(QList<Calendar> calendars,int index,QDate month,QString page,QList<Event> events,int pages=0);
    void fail(QString message,bool rejected=false);
};
