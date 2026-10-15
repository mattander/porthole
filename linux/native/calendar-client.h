#pragma once
#include "data.h"
#include <QObject>
#include <QJsonObject>
#include <functional>

class CalendarClient:public QObject {
    Q_OBJECT
public:
    explicit CalendarClient(QObject *parent=nullptr,bool preview=false,QString socketPath="/run/porthole-calendar/socket");
    void connectAccount(const QString &account,const QByteArray &password,bool remember=true);
    void selectCalendars(const QList<Calendar> &calendars);
    void fetch(const QList<Calendar> &,QDate month);
    void eventDetails(const Event &event,std::function<void(std::optional<Event>)> complete);
    void forgetAccount();
    void selectGoogleCalendars(const QList<Calendar> &calendars);
    void forgetGoogleAccount();
    QJsonObject googleState()const{return google;}
    QString icloudStatus()const{return appleStatus;}
    QList<Calendar> googleCalendars()const;
    void requestStatus();
    void briefingWeather(QJsonObject weather);
    void briefingSettings(bool enabled,QString priorities,int budgetCents);
    void refreshBriefing();
    void forgetBriefingKey();
    void saveProfile(QJsonObject profile);
    QJsonObject briefingState()const{return lastBriefing;}
    QDateTime lastChecked()const{return checked;}
    void reload(){eventRevision=-1;requestStatus();}
    bool hasAccount() const{return hasSaved||authenticated;}
    QString accountEmail()const{return account;}
    QList<Calendar> availableCalendars()const{return available;}
signals:
    void calendarsReady(QList<Calendar> calendars);
    void selectedChanged(QList<Calendar> calendars);
    void eventsReady(QList<Event> events,QDate month);
    void failed(QString message);
    void statusChanged(QString message);
    void briefingChanged(QJsonObject state);
    void profileChanged(QJsonObject profile);
    void profileFailed(QString message);
    void healthChanged(QDateTime checked,QString issue,bool authenticated);
private:
    bool preview=false,polling=false,authenticated=false,hasSaved=false;
    const QString endpoint;
    qint64 eventRevision=-1,selectionRevision=-1;
    QString account,instance,lastStatus,appleStatus;
    QList<Calendar> available;
    QJsonObject lastBriefing,cachedBriefingWeather,lastProfile,google;
    QDate requestedMonth;
    QDateTime checked;
    QString calendarIssue;
    void send(QJsonObject request,bool status=false);
    void receive(const QJsonObject &reply);
};
