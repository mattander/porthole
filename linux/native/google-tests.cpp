#include "google-calendar.h"
#include "profile.h"
#include "calendar-sources.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>
#include <cstring>
class GoogleReply:public QNetworkReply{
    QByteArray body;qint64 offset=0;
public:
    GoogleReply(const QNetworkRequest &r,QByteArray b,int code,QObject *parent):QNetworkReply(parent),body(b){setRequest(r);setUrl(r.url());setAttribute(QNetworkRequest::HttpStatusCodeAttribute,code);open(QIODevice::ReadOnly|QIODevice::Unbuffered);if(code>=400)setError(ContentAccessDenied,"Fixture");QTimer::singleShot(0,this,[this]{setFinished(true);emit readyRead();emit finished();});}
    void abort()override{}qint64 bytesAvailable()const override{return body.size()-offset+QIODevice::bytesAvailable();}bool isSequential()const override{return true;}
    qint64 readData(char *dest,qint64 max)override{const auto n=qMin(max,body.size()-offset);if(n<=0)return -1;std::memcpy(dest,body.constData()+offset,n);offset+=n;return n;}
};
class GoogleTransport:public QNetworkAccessManager{
public:
    int refreshes=0,eventPages=0;bool expired=false,revoke=false,redirect=false;QList<QNetworkRequest> requests;QList<Operation> operations;QByteArray refreshBody;
    QByteArray calendarListBody=R"({"items":[{"id":"work@example.invalid","summary":"Work","primary":true,"backgroundColor":"#1a73e8"}]})";
    QNetworkReply *createRequest(Operation op,const QNetworkRequest &r,QIODevice *out)override{
        requests<<r;operations<<op;int code=200;QByteArray body;
        if(r.url().host()=="oauth2.googleapis.com"){++refreshes;refreshBody=out->readAll();if(revoke){code=400;body=R"({"error":"invalid_grant"})";}else body=R"({"access_token":"fixture-access","token_type":"Bearer","expires_in":3600,"scope":"https://www.googleapis.com/auth/calendar.readonly"})";}
        else if(redirect){code=302;body="{}";}
        else if(r.url().path().endsWith("calendarList"))body=calendarListBody;
        else {++eventPages;QUrlQuery q(r.url());if(expired){expired=false;code=401;body="{}";}else if(q.queryItemValue("pageToken").isEmpty())body=R"({"items":[{"id":"one","summary":"School lunch","start":{"date":"2026-10-16"},"end":{"date":"2026-10-17"}}],"nextPageToken":"next"})";else body=R"({"items":[{"id":"two","summary":"Meeting","start":{"dateTime":"2026-10-16T15:00:00Z"},"end":{"dateTime":"2026-10-16T16:00:00Z"}},{"id":"gone","status":"cancelled"}]})";}
        return new GoogleReply(r,body,code,this);
    }
};
QJsonObject credentials(){return {{"client_id","fixture.apps.googleusercontent.com"},{"client_secret","fixture-secret"},{"refresh_token","fixture-refresh"}};}
class GoogleTests:public QObject{
    Q_OBJECT
private slots:
    void cleanup(){setHouseholdProfile(HouseholdProfile());}
    void credentialsAndCalendarIdentity(){
        QVERIFY(GoogleCredentials::parse(credentials()));auto value=credentials();value["refresh_token"]="line\nheader";QVERIFY(!GoogleCredentials::parse(value));value=credentials();value["path"]="/tmp/secret";QVERIFY(!GoogleCredentials::parse(value));
        const auto url=googleCalendarUrl("work@example.invalid/x+&");QVERIFY(isGoogleCalendarUrl(url));QVERIFY(!permittedCalendarUrl(url));QVERIFY(!isGoogleCalendarUrl(QUrl("googlecalendar://evil/work")));QVERIFY(!isGoogleCalendarUrl(QUrl("googlecalendar:/work?token=secret")));
        value=credentials();value["refresh_token"]="a+b&c=d";const auto parsed=GoogleCredentials::parse(value);QVERIFY(parsed);QUrlQuery query(QString::fromUtf8(parsed->refreshBody()));QCOMPARE(query.queryItemValue("refresh_token",QUrl::FullyDecoded),QString("a+b&c=d"));
    }
    void datesAndExclusiveEnd(){
        const Calendar c{googleCalendarUrl("fixture"),"Fixture","#aBcDeF"};const auto page=QJsonDocument::fromJson(R"({"items":[{"id":"a","start":{"date":"2026-10-16"},"end":{"date":"2026-10-17"}}]})").object();const auto events=parseGoogleEvents(page,c);QVERIFY(events);QCOMPARE(events->size(),1);QVERIFY(events->first().onDate(QDate(2026,10,16)));QVERIFY(!events->first().onDate(QDate(2026,10,15)));QVERIFY(!events->first().onDate(QDate(2026,10,17)));QCOMPARE(events->first().colour,QString("#ABCDEF"));QCOMPARE(events->first().calendarId,calendarIdentity(c));
        QVERIFY(!parseGoogleEvents(QJsonDocument::fromJson(R"({"items":[{"id":"a","start":{"dateTime":"2026-10-16T10:00:00"},"end":{"dateTime":"2026-10-16T11:00:00"}}]})").object(),c));
    }
    void calendarListColoursAreValidated(){
        GoogleTransport network;network.calendarListBody=R"({"items":[{"id":"one@example.invalid","summary":"Same","backgroundColor":"#a1b2c3"},{"id":"two@example.invalid","summary":"Same"},{"id":"three@example.invalid","summary":"Other","backgroundColor":"not-a-colour"}]})";
        GoogleCalendar client(nullptr,&network);QList<Calendar> calendars;QSignalSpy found(&client,&GoogleCalendar::calendarsReady);
        connect(&client,&GoogleCalendar::calendarsReady,this,[&](const QList<Calendar> &items){calendars=items;});client.connectAccount(*GoogleCredentials::parse(credentials()));QTRY_COMPARE(found.count(),1);
        QCOMPARE(calendars.size(),3);QCOMPARE(calendars[0].colour,QString("#A1B2C3"));QVERIFY(calendars[1].colour.isEmpty());QVERIFY(calendars[2].colour.isEmpty());
        QCOMPARE(calendars[0].name,calendars[1].name);QVERIFY(calendarIdentity(calendars[0])!=calendarIdentity(calendars[1]));
    }
    void briefingRequiresEveryConnectedSource(){
        const auto time=QDateTime::fromString("2026-10-14T16:00:00Z",Qt::ISODate);Event apple;apple.uid="apple";Event google;google.uid="google";
        const CheckedCalendarData a{{apple},time},g{{google},time.addSecs(2)};
        QVERIFY(!combineCalendarSources(a,CalendarUnavailable{},time));QVERIFY(!combineCalendarSources(CalendarUnavailable{},g,time));
        QVERIFY(!combineCalendarSources(a,g,time.addSecs(1)));QVERIFY(!combineCalendarSources(CalendarNotConfigured{},CalendarNotConfigured{},time));
        const auto both=combineCalendarSources(a,g,time);QVERIFY(both);QCOMPARE(both->events.size(),2);QCOMPARE(both->events[0].uid,QString("apple"));QCOMPARE(both->events[1].uid,QString("google"));QCOMPARE(both->checked,time);
        const auto only=combineCalendarSources(CalendarNotConfigured{},g,time);QVERIFY(only);QCOMPARE(only->events.size(),1);
    }
    void mergedAgendaKeepsAllDayThenChronologicalOrder(){
        Event late;late.uid="late";late.start=QDateTime::fromString("2026-10-14T16:00:00Z",Qt::ISODate);Event early=late;early.uid="early";early.start=late.start.addSecs(-3600);Event allDay=early;allDay.uid="all-day";allDay.allDay=true;
        const auto result=mergeCalendarEvents({late},{early,allDay});QCOMPARE(result[0].uid,QString("all-day"));QCOMPARE(result[1].uid,QString("early"));QCOMPARE(result[2].uid,QString("late"));
    }
    void timezoneAndDstPreserveLocalDates(){
        auto json=HouseholdProfile().json();json["timezone"]="America/New_York";const auto parsed=HouseholdProfile::parse(json);const auto *profile=std::get_if<HouseholdProfile>(&parsed);QVERIFY(profile);setHouseholdProfile(*profile);
        const auto page=QJsonDocument::fromJson(R"({"items":[{"id":"dst","start":{"date":"2026-11-01"},"end":{"date":"2026-11-02"}},{"id":"local","start":{"dateTime":"2026-10-16T02:00:00Z"},"end":{"dateTime":"2026-10-16T03:00:00Z"}}]})").object();
        const auto result=parseGoogleEvents(page,{googleCalendarUrl("fixture"),"Fixture"});QVERIFY(result);QCOMPARE(result->first().start.secsTo(result->first().end),qint64(25*3600));QCOMPARE(result->last().start.date(),QDate(2026,10,15));
    }
    void paginationTokenRenewalAndReadOnlyRequests(){
        GoogleTransport network;GoogleCalendar client(nullptr,&network);QSignalSpy found(&client,&GoogleCalendar::calendarsReady),loaded(&client,&GoogleCalendar::eventsReady);const auto bundle=GoogleCredentials::parse(credentials());QVERIFY(bundle);client.connectAccount(*bundle);QTRY_COMPARE(found.count(),1);QList<Calendar> calendars;
        connect(&client,&GoogleCalendar::calendarsReady,this,[&](const QList<Calendar> &v){calendars=v;});client.discover();QTRY_COMPARE(found.count(),2);QCOMPARE(calendars.size(),1);network.expired=true;client.fetch(calendars,QDate(2026,10,1));QTRY_COMPARE(loaded.count(),1);QCOMPARE(network.refreshes,2);QList<Event> result;connect(&client,&GoogleCalendar::eventsReady,this,[&](const QList<Event> &v,QDate){result=v;});client.fetch(calendars,QDate(2026,10,1));QTRY_COMPARE(loaded.count(),2);QCOMPARE(result.size(),2);
        QCOMPARE(calendars.first().colour,QString("#1A73E8"));QCOMPARE(result.first().colour,QString("#1A73E8"));QCOMPARE(result.first().calendarId,calendarIdentity(calendars.first()));
        for(qsizetype i=0;i<network.requests.size();++i){const auto &r=network.requests[i];QCOMPARE(network.operations[i],r.url().host()=="oauth2.googleapis.com"?QNetworkAccessManager::PostOperation:QNetworkAccessManager::GetOperation);if(r.url().path().endsWith("calendarList")){QUrlQuery query(r.url());QVERIFY(query.queryItemValue("fields").contains("backgroundColor"));}if(r.url().path().endsWith("/events")){QUrlQuery query(r.url());QCOMPARE(query.queryItemValue("singleEvents"),QString("true"));QVERIFY(!query.queryItemValue("timeMin").isEmpty());QVERIFY(!query.queryItemValue("timeMax").isEmpty());}QVERIFY(!r.url().hasQuery()||!r.url().query().contains("access_token"));if(r.url().host()=="www.googleapis.com")QCOMPARE(r.rawHeader("Authorization"),QByteArray("Bearer fixture-access"));QVERIFY(r.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt()==QNetworkRequest::ManualRedirectPolicy);}
    }
    void redirectsCannotForwardBearerTokens(){GoogleTransport network;network.redirect=true;GoogleCalendar client(nullptr,&network);QSignalSpy failed(&client,&GoogleCalendar::failed),found(&client,&GoogleCalendar::calendarsReady);client.connectAccount(*GoogleCredentials::parse(credentials()));QTRY_COMPARE(failed.count(),1);QCOMPARE(network.requests.size(),2);QCOMPARE(found.count(),0);QVERIFY(!failed.first().first().toString().contains("fixture-access"));}
    void revokedRefreshStopsAccess(){GoogleTransport network;network.revoke=true;GoogleCalendar client(nullptr,&network);QSignalSpy rejected(&client,&GoogleCalendar::authorizationRejected),found(&client,&GoogleCalendar::calendarsReady);client.connectAccount(*GoogleCredentials::parse(credentials()));QTRY_COMPARE(rejected.count(),1);client.discover();QTest::qWait(30);QCOMPARE(network.refreshes,1);QCOMPARE(found.count(),0);}
};
QTEST_GUILESS_MAIN(GoogleTests)
#include "google-tests.moc"
