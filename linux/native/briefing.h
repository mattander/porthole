#pragma once
#include "data.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QStringList>
#include <functional>

constexpr int BriefingCharacterLimit=400;
constexpr int BriefingGenerationCharacterLimit=240;
constexpr int BriefingGenerationReferencesPerSegmentLimit=1;
constexpr int BriefingDailyAttemptLimit=31;
constexpr int BriefingSegmentLimit=32;
constexpr int BriefingReferencesPerSegmentLimit=4;
constexpr qint64 BriefingStateByteLimit=65536; // Covers worst-case accepted UTF-8 metadata plus settings and the durable ledger.
enum class BriefingPeriod { Morning, Midday, Evening };
BriefingPeriod briefingPeriod(QDateTime time);
QString briefingPeriodName(BriefingPeriod period);
bool briefingWakingHours(QDateTime time);
QDateTime nextBriefingCheck(QDateTime time);
QString boundedBriefing(const QString &text,int maximum=BriefingCharacterLimit);
QJsonObject briefingContext(const QList<Event> &events,const QJsonObject &weather,QDateTime current);
QJsonObject briefingRequest(const QJsonObject &context,const QString &priorities);

struct BriefingEventSource {
    QString calendarId;
    QString colour;
    QString name;
};

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
    QJsonArray segments,calendarColours;
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
    bool refreshCalendarColours();
    void invalidate(bool clearText);
    void complete(QNetworkReply *reply,QByteArray fingerprint,QString periodKey,quint64 token,QMap<QString,BriefingEventSource> sources);
};
