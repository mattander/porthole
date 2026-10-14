#pragma once
#include "data.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <functional>

constexpr int BriefingCharacterLimit=400;
constexpr int BriefingDailyAttemptLimit=31;
enum class BriefingPeriod { Morning, Midday, Evening };
BriefingPeriod briefingPeriod(QDateTime time);
QString briefingPeriodName(BriefingPeriod period);
bool briefingWakingHours(QDateTime time);
QDateTime nextBriefingCheck(QDateTime time);
QString boundedBriefing(const QString &text,int maximum=BriefingCharacterLimit);
QJsonObject briefingContext(const QList<Event> &events,const QJsonObject &weather,QDateTime current);
QJsonObject briefingRequest(const QJsonObject &context,const QString &priorities);

class BriefingEngine:public QObject {
    Q_OBJECT
public:
    BriefingEngine(QString statePath,QByteArray apiKey,QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr,std::function<QDateTime()> clock={});
    void calendarData(const QList<Event> &events,QDateTime checked);
    void calendarUnavailable(bool forget=false);
    void weatherData(const QJsonObject &data);
    void configure(bool enabled,const QString &priorities,int budgetCents);
    void refresh(bool manual=false);
    void forgetKey();
    void profileChanged();
    QJsonObject snapshot() const;
signals:
    void changed();
private:
    QString path,keyIdentity,text,status="AI briefing is off",priorities="";
    QByteArray key,lastFingerprint;
    QNetworkAccessManager ownedNetwork;
    QNetworkAccessManager *network;
    std::function<QDateTime()> clock;
    QPointer<QNetworkReply> active;
    QList<Event> events;
    QJsonObject weather;
    QDateTime calendarChecked,weatherChecked,generated,lastAttempt,lastCheck,validatedAt;
    QString billingMonth,attemptDay,checkedSlot,generatedPeriod,cachedProfileRevision;
    qint64 spent=0;
    int attempts=0,budgetCents=100;
    bool enabled=false,storageOk=true,authBlocked=false,bootPending=true;
    quint64 generation=0;
    QDateTime timeNow()const;
    bool persist();
    void invalidate(bool clearText);
    void complete(QNetworkReply *reply,QByteArray fingerprint,QString periodKey,quint64 token);
};
