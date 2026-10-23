#include "profile.h"
#include "caldav.h"
#include "diagnostic-log.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <algorithm>
#include <stdexcept>

namespace {
const QByteArray Principal = R"(<d:propfind xmlns:d="DAV:"><d:prop><d:current-user-principal/></d:prop></d:propfind>)";
const QByteArray Home = R"(<d:propfind xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav"><d:prop><c:calendar-home-set/></d:prop></d:propfind>)";
const QByteArray Collections = R"(<d:propfind xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav" xmlns:a="http://apple.com/ns/ical/"><d:prop><d:displayname/><d:resourcetype/><c:supported-calendar-component-set/><a:calendar-color/></d:prop></d:propfind>)";
const QString AppleCalendarColour = "http://apple.com/ns/ical/|calendar-color";
QString discoveredProperty(const QByteArray &data, const QString &name) {
    for (const auto &response : parseDav(data)) {
        const auto value = response.properties.value(name);
        if (!value.isEmpty()) return value;
    }
    throw std::runtime_error("Missing calendar discovery property");
}
QString utcBoundary(QDate date) {
    return QDateTime(date,QTime(0,0),configuredZone()).toUTC().toString("yyyyMMdd'T'HHmmss'Z'");
}
void applyCalendarMetadata(QList<Event> &events,const Calendar &calendar){
    const auto id=calendarIdentity(calendar),colour=canonicalCalendarColour(calendar.colour);
    for(auto &event:events){event.calendar=calendar.name;event.calendarId=id;event.colour=colour;}
}
}

Caldav::Caldav(QObject *parent,QNetworkAccessManager *transport,CalendarTimeouts limits) : QObject(parent), ownedNetwork(this),network(transport?transport:&ownedNetwork),timeouts(limits) {
    operationDeadline.setSingleShot(true);
    connect(&operationDeadline,&QTimer::timeout,this,[this]{
        diagnosticEvent(DiagnosticEvent::CalendarOperationTimeout);
        failRequest("Calendar refresh timed out. It will retry automatically.");
    });
}
Caldav::~Caldav() { cancelFetch();authorization.fill('\0'); }
void Caldav::cancelFetch(){
    ++generation;operationDeadline.stop();fetching=false;pendingMonth={};pendingCalendars.clear();
    const auto pending=replies;replies.clear();
    for(const auto &reply:pending)if(reply){reply->abort();reply->deleteLater();}
}
void Caldav::failRequest(const QString &message){
    cancelFetch();network->clearConnectionCache();emit failed(message);
}
void Caldav::disconnectAccount() {
    cancelFetch();cache.clear();cacheMonth={};
    authorization.fill('\0');authorization.clear();network->clearAccessCache();
}

void Caldav::request(QUrl url, const QByteArray &method, const QByteArray &body,
                    const QByteArray &depth, Callback done, int redirects,bool optionalProbe) {
    if (!permittedCalendarUrl(url) || redirects > 4) {
        diagnosticEvent(DiagnosticEvent::CalendarRequestFailed);
        failRequest("The calendar server returned an unsupported address.");
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(20000);
    req.setRawHeader("Authorization", authorization);
    req.setRawHeader("Content-Type", "application/xml; charset=utf-8");
    req.setRawHeader("Depth", depth);
    auto *reply = network->sendCustomRequest(req, method, body);
    const quint64 token = generation;
    replies.append(reply);
    auto *deadline=new QTimer(reply);deadline->setSingleShot(true);
    connect(deadline,&QTimer::timeout,this,[this,token]{
        if(token!=generation)return;
        diagnosticEvent(DiagnosticEvent::CalendarRequestTimeout);
        failRequest("Calendar request timed out. It will retry automatically.");
    });
    deadline->start(timeouts.requestMs);
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 4 * 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [=] {
        deadline->stop();replies.removeIf([reply](const auto &pending){return !pending||pending==reply;});reply->deleteLater();
        if (token != generation) return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 301 || status == 302 || status == 307 || status == 308) {
            const QUrl next = url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            request(next, method, body, depth, done, redirects + 1,optionalProbe);
            return;
        }
        if (status == 401 || status == 403) {
            diagnosticEvent(DiagnosticEvent::CalendarRequestFailed,status,reply->error());
            disconnectAccount();
            emit authorizationRejected();
            emit failed("iCloud did not accept calendar access. Check the dedicated app-specific password.");
            return;
        }
        if(optionalProbe&&(status==404||status==405||status==501)){done({},url);return;}
        if (reply->error() != QNetworkReply::NoError || status != 207 || reply->bytesAvailable() > 4*1024*1024) {
            diagnosticEvent(DiagnosticEvent::CalendarRequestFailed,status,reply->error());
            failRequest("Calendar access failed. Check the connection and the frame's clock, then retry.");
            return;
        }
        try { done(reply->readAll(), url); }
        catch (const std::exception &) {
            diagnosticEvent(DiagnosticEvent::CalendarResponseInvalid);
            failRequest("The calendar response could not be read safely. Previously loaded events are retained.");
        }
    });
}

void Caldav::connectAccount(const QString &account, const QByteArray &appPassword) {
    disconnectAccount();
    static const QRegularExpression password("^[a-z]{4}(-[a-z]{4}){3}$");
    if (!account.contains('@') || account.contains(':') || account.contains('\n') ||
        !password.match(QString::fromLatin1(appPassword)).hasMatch()) {
        emit failed("Use your Apple Account email and a dedicated app-specific password in xxxx-xxxx-xxxx-xxxx format.");
        return;
    }
    operationDeadline.start(timeouts.operationMs);
    QByteArray pair = account.trimmed().toUtf8() + ':' + appPassword;
    authorization = "Basic " + pair.toBase64();
    pair.fill('\0');
    request(QUrl("https://caldav.icloud.com/"), "PROPFIND", Principal, "0", [this](const QByteArray &data,const QUrl &base) {
        const auto principal = calendarHref(base, discoveredProperty(data,davProperty("current-user-principal")));
        request(principal, "PROPFIND", Home, "0", [this](const QByteArray &data,const QUrl &base) {
            const auto home = calendarHref(base, discoveredProperty(data,calProperty("calendar-home-set")));
            request(home, "PROPFIND", Collections, "1", [this](const QByteArray &data,const QUrl &base) {
                QList<Calendar> calendars;
                QSet<QUrl> seen;
                for (const auto &r : parseDav(data)) {
                    if (r.properties.value(davProperty("resourcetype")) != "calendar") continue;
                    const auto kinds = r.properties.value(calProperty("supported-calendar-component-set"));
                    if (!kinds.isEmpty() && !kinds.split(',').contains("VEVENT")) continue;
                    const auto url = calendarHref(base,r.href);
                    if (seen.contains(url)) continue;
                    seen.insert(url);
                    QString name = r.properties.value(davProperty("displayname")).left(100);
                    calendars << Calendar{url,name.isEmpty() ? QString("Calendar") : name,
                                          canonicalCalendarColour(r.properties.value(AppleCalendarColour))};
                    if (calendars.size() > 50) throw std::runtime_error("Too many calendars");
                }
                operationDeadline.stop();diagnosticEvent(DiagnosticEvent::CalendarReady);
                emit calendarsReady(calendars);
            });
        });
    });
}

void Caldav::fetch(const QList<Calendar> &calendars, QDate month) {
    if (authorization.isEmpty() || calendars.isEmpty() || calendars.size()>16) return;
    month = QDate(month.year(),month.month(),1);
    if (fetching) {
        pendingMonth=month != activeMonth ? month : QDate();
        pendingCalendars=calendars;
        return;
    }
    if(cacheMonth!=month){cache.clear();cacheMonth=month;}
    fetching = true;operationDeadline.start(timeouts.operationMs);
    activeMonth = month;
    const QDate first(month.year(),month.month(),1);
    reportNext(calendars,0,first.addDays(-7),first.addMonths(1).addDays(7),{},generation);
}
void Caldav::reportNext(QList<Calendar> calendars,int index,QDate first,QDate last,
                       QList<Event> events,quint64 token) {
    if (token != generation) return;
    if (index == calendars.size()) {
        std::sort(events.begin(),events.end(),[](const Event &a,const Event &b) {
            if (a.start.date()==b.start.date() && a.allDay!=b.allDay) return a.allDay;
            return a.start<b.start;
        });
        fetching = false;operationDeadline.stop();diagnosticEvent(DiagnosticEvent::CalendarFetchComplete);
        emit eventsReady(events,activeMonth);
        if(pendingMonth.isValid()){
            const auto next=pendingMonth;const auto nextCalendars=pendingCalendars;
            pendingMonth={};pendingCalendars.clear();fetch(nextCalendars,next);
        }
        return;
    }
    const auto calendar=calendars[index];
    const QByteArray probe=R"(<d:propfind xmlns:d="DAV:" xmlns:s="http://calendarserver.org/ns/" xmlns:a="http://apple.com/ns/ical/"><d:prop><d:sync-token/><s:getctag/><a:calendar-color/></d:prop></d:propfind>)";
    request(calendar.url,"PROPFIND",probe,"0",[=](const QByteArray &data,const QUrl &) mutable {
        QString changeToken;
        QString colour=calendar.colour;
        if(!data.isEmpty())for(const auto &r:parseDav(data)){
            changeToken=r.properties.value(davProperty("sync-token"));
            if(changeToken.isEmpty())changeToken=r.properties.value("http://calendarserver.org/ns/|getctag");
            if(r.properties.contains(AppleCalendarColour))colour=canonicalCalendarColour(r.properties.value(AppleCalendarColour));
            if(!changeToken.isEmpty())break;
        }
        calendars[index].colour=colour;
        const auto key=calendar.url.toString();
        if(!changeToken.isEmpty()&&cache.contains(key)&&cache.value(key).token==changeToken){
            auto cached=cache.value(key).events;applyCalendarMetadata(cached,calendars[index]);
            cache[key].events=cached;events.append(cached);reportNext(calendars,index+1,first,last,events,token);
        }else readCalendar(calendars,index,first,last,events,token,changeToken);
    },0,true);
}
void Caldav::readCalendar(QList<Calendar> calendars,int index,QDate first,QDate last,QList<Event> events,quint64 token,QString changeToken){
    const QString range="start=\""+utcBoundary(first)+"\" end=\""+utcBoundary(last)+"\"";
    const QByteArray body=(QString(R"(<c:calendar-query xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav"><d:prop><c:calendar-data><c:expand )")+range+R"(/></c:calendar-data></d:prop><c:filter><c:comp-filter name="VCALENDAR"><c:comp-filter name="VEVENT"><c:time-range )"+range+R"(/></c:comp-filter></c:comp-filter></c:filter></c:calendar-query>)").toUtf8();
    const auto calendar=calendars[index];
    request(calendar.url,"REPORT",body,"1",[=](const QByteArray &data,const QUrl &) mutable {
        QList<Event> calendarEvents;
        for (const auto &r : parseDav(data)) {
            const auto ics=r.properties.value(calProperty("calendar-data"));
            if (ics.isEmpty()) throw std::runtime_error("Calendar resource could not be read");
            auto parsed=parseExpandedCalendar(ics.toUtf8(),calendar.name);applyCalendarMetadata(parsed,calendar);events.append(parsed);calendarEvents.append(parsed);
            if (events.size()>5000) throw std::runtime_error("Too many events");
        }
        cache[calendar.url.toString()]={changeToken,calendarEvents};
        reportNext(calendars,index+1,first,last,events,token);
    });
}
