#include "calendar-test-transport.h"
#include <QSignalSpy>
#include <QtTest>

class SyncTests:public QObject {
    Q_OBJECT
private slots:
    void stalledDiscoveryRecoversWithoutRestart(){
        FakeCalendar network;Caldav client(nullptr,&network,{30,200});
        QSignalSpy failed(&client,&Caldav::failed),discovered(&client,&Caldav::calendarsReady);
        network.hangNext=true;client.connectAccount("fixture@example.invalid","abcd-efgh-ijkl-mnop");
        QTRY_COMPARE(failed.count(),1);QCOMPARE(discovered.count(),0);
        client.connectAccount("fixture@example.invalid","abcd-efgh-ijkl-mnop");QTRY_COMPARE(discovered.count(),1);
    }
    void stalledFetchAndOperationDeadlineRecover(){
        for(const auto limits:{CalendarTimeouts{30,500},CalendarTimeouts{500,30}}){
            FakeCalendar network;Caldav client(nullptr,&network,limits);
            QSignalSpy discovered(&client,&Caldav::calendarsReady),events(&client,&Caldav::eventsReady),failed(&client,&Caldav::failed);
            client.connectAccount("fixture@example.invalid","abcd-efgh-ijkl-mnop");QTRY_COMPARE(discovered.count(),1);
            const QList<Calendar> selected{{QUrl("https://caldav.icloud.com/family/"),"Fixture"}};
            network.hangNext=true;client.fetch(selected,QDate(2026,10,1));
            // A normal poll while stuck must not keep extending the deadline.
            client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(failed.count(),1);QCOMPARE(events.count(),0);
            client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),1);
            QCOMPARE(network.reports,1);QCOMPARE(failed.count(),1);
        }
    }
    void cancelledFetchCannotPoisonNextCycle(){
        FakeCalendar network;Caldav client(nullptr,&network,{500,1000});
        QSignalSpy discovered(&client,&Caldav::calendarsReady),events(&client,&Caldav::eventsReady),failed(&client,&Caldav::failed);
        client.connectAccount("fixture@example.invalid","abcd-efgh-ijkl-mnop");QTRY_COMPARE(discovered.count(),1);
        const QList<Calendar> selected{{QUrl("https://caldav.icloud.com/family/"),"Fixture"}};
        network.hangNext=true;client.fetch(selected,QDate(2026,10,1));QVERIFY(network.hanging);
        client.cancelFetch();network.hanging->lateCompletion();
        client.fetch(selected,QDate(2026,10,1));QTRY_COMPARE(events.count(),1);QCOMPARE(failed.count(),0);
    }
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
