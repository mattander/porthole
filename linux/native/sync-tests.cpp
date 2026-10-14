#include "caldav.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>
#include <cstring>

class Reply:public QNetworkReply {
    QByteArray body;qint64 offset=0;
public:
    Reply(const QNetworkRequest &request,QByteArray value,int code,QObject *parent):QNetworkReply(parent),body(value){
        setRequest(request);setUrl(request.url());setAttribute(QNetworkRequest::HttpStatusCodeAttribute,code);open(QIODevice::ReadOnly|QIODevice::Unbuffered);
        if(code>=400)setError(QNetworkReply::ContentAccessDenied,"Fixture error");
        QTimer::singleShot(0,this,[this]{setFinished(true);emit readyRead();emit finished();});
    }
    void abort()override{}
    qint64 bytesAvailable()const override{return body.size()-offset+QIODevice::bytesAvailable();}
    bool isSequential()const override{return true;}
protected:
    qint64 readData(char *data,qint64 max)override{const auto n=qMin(max,body.size()-offset);if(n<=0)return -1;std::memcpy(data,body.constData()+offset,n);offset+=n;return n;}
};
class FakeCalendar:public QNetworkAccessManager {
public:
    QString token="one",colour="#1a2b3c80";int reports=0,probes=0,probeCode=207;bool omitColour=false;
protected:
    QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *output)override{
        const auto query=output?output->readAll():QByteArray();QByteArray props,colourProperty;int code=207;
        if(!omitColour)colourProperty="<a:calendar-color>"+colour.toUtf8()+"</a:calendar-color>";
        if(query.contains("current-user-principal"))props="<d:current-user-principal><d:href>/principal/</d:href></d:current-user-principal>";
        else if(query.contains("calendar-home-set"))props="<c:calendar-home-set><d:href>/calendars/</d:href></c:calendar-home-set>";
        else if(query.contains("supported-calendar-component-set"))props="<d:displayname>Fixture</d:displayname><d:resourcetype><c:calendar/></d:resourcetype><c:supported-calendar-component-set><c:comp name='VEVENT'/></c:supported-calendar-component-set>"+colourProperty;
        else if(query.contains("getctag")){++probes;code=probeCode;props="<s:getctag>"+token.toUtf8()+"</s:getctag>"+colourProperty;}
        else {++reports;props="<c:calendar-data>BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nUID:fixture\nDTSTART:20261014T160000Z\nDTEND:20261014T170000Z\nSUMMARY:Fixture "+token.toUtf8()+"\nEND:VEVENT\nEND:VCALENDAR\n</c:calendar-data>";}
        const QByteArray body="<d:multistatus xmlns:d='DAV:' xmlns:c='urn:ietf:params:xml:ns:caldav' xmlns:s='http://calendarserver.org/ns/' xmlns:a='http://apple.com/ns/ical/'><d:response><d:href>/family/</d:href><d:propstat><d:prop>"+props+"</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response></d:multistatus>";
        return new Reply(request,body,code,this);
    }
};
class SyncTests:public QObject {
    Q_OBJECT
private slots:
    void changesOnlyAndMonthCacheBoundaries(){
        FakeCalendar network;Caldav client(nullptr,&network);QSignalSpy discovered(&client,&Caldav::calendarsReady),events(&client,&Caldav::eventsReady);
        QList<Calendar> available;QList<Event> loaded;
        connect(&client,&Caldav::calendarsReady,this,[&](const QList<Calendar> &items){available=items;});
        connect(&client,&Caldav::eventsReady,this,[&](const QList<Event> &items,QDate){loaded=items;});
        client.connectAccount("fixture@example.invalid","abcd-efgh-ijkl-mnop");QTRY_COMPARE(discovered.count(),1);
        QCOMPARE(available.first().colour,QString("#1A2B3C80"));
        const QList<Calendar> selected=available;
        client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),1);QCOMPARE(network.reports,1);QCOMPARE(loaded.first().colour,QString("#1A2B3C80"));QCOMPARE(loaded.first().calendarId,calendarIdentity(selected.first()));
        network.colour="#fedcba";client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),2);QCOMPARE(network.reports,1);QCOMPARE(loaded.first().colour,QString("#FEDCBA"));
        network.omitColour=true;client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),3);QCOMPARE(network.reports,1);QCOMPARE(loaded.first().colour,QString("#1A2B3C80"));
        network.omitColour=false;network.colour="invalid";client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),4);QCOMPARE(network.reports,1);QVERIFY(loaded.first().colour.isEmpty());
        network.token="two";client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),5);QCOMPARE(network.reports,2);
        client.fetch(selected,QDate(2026,11,1));QTRY_COMPARE(events.count(),6);QCOMPARE(network.reports,3);
        QCOMPARE(network.probes,6);
    }
    void unsupportedProbeFallsBackAndUnauthorizedRequiresSignIn(){
        FakeCalendar network;Caldav client(nullptr,&network);QSignalSpy discovered(&client,&Caldav::calendarsReady),events(&client,&Caldav::eventsReady),rejected(&client,&Caldav::authorizationRejected);
        client.connectAccount("fixture@example.invalid","abcd-efgh-ijkl-mnop");QTRY_COMPARE(discovered.count(),1);
        const QList<Calendar> selected{{QUrl("https://caldav.icloud.com/family/"),"Fixture"}};
        network.probeCode=501;client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),1);QCOMPARE(network.reports,1);
        client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),2);QCOMPARE(network.reports,2);
        network.probeCode=401;client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(rejected.count(),1);QCOMPARE(events.count(),2);
        client.fetch(selected,QDate(2026,10,1));QTest::qWait(50);QCOMPARE(network.probes,3);
    }
};
QTEST_GUILESS_MAIN(SyncTests)
#include "sync-tests.moc"
