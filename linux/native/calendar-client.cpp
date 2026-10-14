#include "profile.h"
#include "calendar-client.h"
#include "google-calendar.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>
#include <QTimeZone>
#include <QSharedPointer>

namespace {
struct PendingRead {QByteArray bytes;bool completed=false;};
QList<Calendar> calendars(const QJsonArray &values){
    QList<Calendar> result;
    for(const auto &value:values){const auto item=value.toObject();const QUrl url(item["url"].toString());if(permittedCalendarUrl(url)||isGoogleCalendarUrl(url))result<<Calendar{url,item["name"].toString().left(100),canonicalCalendarColour(item["colour"].toString())};}
    return result;
}
}
CalendarClient::CalendarClient(QObject *parent,bool mock,QString socketPath):QObject(parent),preview(mock),endpoint(std::move(socketPath)){
    if(preview)return;
    auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,&CalendarClient::requestStatus);timer->start(2000);
    QTimer::singleShot(0,this,&CalendarClient::requestStatus);
}
void CalendarClient::send(QJsonObject request,bool status){
    if(preview)return;
    if(status&&polling)return;
    if(status)polling=true;
    auto *socket=new QLocalSocket(this);auto state=QSharedPointer<PendingRead>::create();
    auto *timeout=new QTimer(socket);timeout->setSingleShot(true);timeout->start(5000);
    auto finish=[this,socket,state,status](const QString &error){
        if(state->completed)return;
        state->completed=true;if(status)polling=false;
        state->bytes.fill('\0');state->bytes.clear();socket->abort();socket->deleteLater();
        if(!error.isEmpty()){lastStatus=error;emit statusChanged(error);}
    };
    connect(timeout,&QTimer::timeout,this,[finish]{finish("Calendar service is restarting or unavailable.");});
    connect(socket,&QLocalSocket::errorOccurred,this,[finish](auto){finish("Calendar service is restarting or unavailable.");});
    connect(socket,&QLocalSocket::connected,this,[socket,request]{socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');});
    connect(socket,&QLocalSocket::readyRead,this,[this,socket,state,finish]{
        if(state->completed)return;
        state->bytes.append(socket->readAll());
        if(state->bytes.size()>4*1024*1024){finish("Calendar response exceeded the local limit.");return;}
        const auto end=state->bytes.indexOf('\n');if(end<0)return;
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(state->bytes.left(end),&error);
        if(error.error==QJsonParseError::NoError&&doc.isObject())receive(doc.object());
        else emit failed("Calendar response could not be read.");
        finish({});
    });
    socket->connectToServer(endpoint);
}
void CalendarClient::receive(const QJsonObject &reply){
    if(reply.contains("error")){if(reply["type"].toString()=="profile")emit profileFailed(reply["error"].toString());else emit failed(reply["error"].toString());return;}
    if(!reply.contains("status")){requestStatus();return;}
    if(reply.contains("profile")&&reply["profile"].toObject()!=lastProfile){
        const auto parsed=HouseholdProfile::parse(reply["profile"].toObject());
        if(const auto *profile=std::get_if<HouseholdProfile>(&parsed)){lastProfile=profile->json();setHouseholdProfile(*profile);cachedBriefingWeather={};emit profileChanged(lastProfile);}else {emit profileFailed("The profile returned by the service is invalid.");return;}
    }
    if(instance!=reply["instance"].toString()){instance=reply["instance"].toString();eventRevision=selectionRevision=-1;if(!cachedBriefingWeather.isEmpty())QTimer::singleShot(0,this,[this]{briefingWeather(cachedBriefingWeather);});if(requestedMonth.isValid())QTimer::singleShot(0,this,[this]{fetch({},requestedMonth);});}
    const auto summary=reply["briefing"].toObject();if(summary!=lastBriefing){lastBriefing=summary;emit briefingChanged(summary);}
    google=reply["google"].toObject();appleStatus=reply["icloudStatus"].toString(reply["status"].toString());
    authenticated=reply["icloudAuthenticated"].toBool(reply["authenticated"].toBool());hasSaved=reply["remembered"].toBool();account=reply["account"].toString();
    checked=QDateTime::fromString(reply["lastChecked"].toString(),Qt::ISODate);
    calendarIssue=reply["calendarIssue"].toString();emit healthChanged(checked,calendarIssue,authenticated||google["authenticated"].toBool());
    available=calendars(reply["available"].toArray());emit selectedChanged(calendars(reply["selected"].toArray()));
    const auto status=reply["status"].toString();if(status!=lastStatus){lastStatus=status;emit statusChanged(status);}
    if(reply.contains("events")){
        QList<Event> loaded;
        for(const auto &value:reply["events"].toArray()){
            const auto event=calendarEventFromJson(value.toObject());if(event)loaded<<*event;
        }
        eventRevision=reply["eventsRevision"].toInteger();emit eventsReady(loaded,QDate::fromString(reply["month"].toString(),Qt::ISODate));
    }
    const auto selection=reply["selectionRevision"].toInteger();
    if(reply["needsSelection"].toBool()&&selection!=selectionRevision){selectionRevision=selection;emit calendarsReady(available);}
}
void CalendarClient::requestStatus(){send({{"type","status"},{"eventsRevision",eventRevision}},true);}
void CalendarClient::connectAccount(const QString &account,const QByteArray &password,bool remember){selectionRevision=-1;send({{"type","login"},{"account",account},{"password",QString::fromLatin1(password)},{"remember",remember}});}
void CalendarClient::selectCalendars(const QList<Calendar> &items){QJsonArray urls;for(const auto &c:items)urls.append(c.url.toString());send({{"type","select"},{"urls",urls}});}
void CalendarClient::fetch(const QList<Calendar> &,QDate month){requestedMonth=QDate(month.year(),month.month(),1);send({{"type","fetch"},{"month",QDate(month.year(),month.month(),1).toString(Qt::ISODate)}});}
void CalendarClient::forgetAccount(){send({{"type","forget"}});}

void CalendarClient::briefingWeather(QJsonObject weather){cachedBriefingWeather=weather;send({{"type","briefingWeather"},{"weather",weather}});}
void CalendarClient::briefingSettings(bool enabled,QString priorities,int budgetCents){send({{"type","briefingSettings"},{"enabled",enabled},{"priorities",priorities},{"budgetCents",budgetCents}});}
void CalendarClient::refreshBriefing(){send({{"type","briefingRefresh"}});}

void CalendarClient::forgetBriefingKey(){send({{"type","briefingForget"}});}

void CalendarClient::saveProfile(QJsonObject profile){send({{"type","profileSettings"},{"profile",profile}});}

QList<Calendar> CalendarClient::googleCalendars()const{return calendars(google["available"].toArray());}
void CalendarClient::selectGoogleCalendars(const QList<Calendar> &items){QJsonArray urls;for(const auto &c:items)urls.append(c.url.toString());send({{"type","googleSelect"},{"urls",urls}});}
void CalendarClient::forgetGoogleAccount(){send({{"type","googleForget"}});}
