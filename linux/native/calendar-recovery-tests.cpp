#define PORTHOLE_CALENDAR_SERVICE_NO_MAIN
#include "calendar-service.cpp"
#include "calendar-test-transport.h"
#include <QEventLoop>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QJsonObject command(const QString &path,const QJsonObject &request){
    QLocalSocket socket;QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);
    QByteArray bytes;QJsonObject result;
    QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
    QObject::connect(&socket,&QLocalSocket::connected,&loop,[&]{socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');});
    QObject::connect(&socket,&QLocalSocket::readyRead,&loop,[&]{bytes+=socket.readAll();if(bytes.contains('\n')){result=QJsonDocument::fromJson(bytes).object();loop.quit();}});
    socket.connectToServer(path);timeout.start(1000);loop.exec();return result;
}
struct Fixture {
    QTemporaryDir directory;
    FakeCalendar agenda,upcoming;
    qint64 elapsed=0;
    bool write(const QString &name,const QByteArray &bytes){QFile file(directory.filePath(name));return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();}
    bool seed(bool ai=true){
        const QJsonObject account{{"account","fixture@example.invalid"},{"password","abcd-efgh-ijkl-mnop"},{"calendars",QJsonArray{QJsonObject{{"url","https://caldav.icloud.com/family/"},{"name","Fixture"}}}}};
        const QJsonObject briefing{{"version",1},{"cacheVersion",4},{"enabled",ai},{"spent",12345},{"attempts",2},{"budgetCents",100},
            {"billingMonth",QDate::currentDate().toString("yyyy-MM")},{"attemptDay",QDate::currentDate().toString(Qt::ISODate)},
            {"generated",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"text","Cached useful briefing."},
            {"segments",QJsonArray{QJsonObject{{"text","Cached useful briefing."},{"calendar_ids",QJsonArray{}}}}},{"calendarColours",QJsonArray{}},
            {"profileRevision",profileRevision(householdProfile())}};
        return directory.isValid()&&write("porthole-icloud",QJsonDocument(account).toJson())&&
            write("porthole-openai","sk-"+QByteArray(40,'f'))&&write("briefing.json",QJsonDocument(briefing).toJson());
    }
    CalendarServiceOptions options(){
        CalendarServiceOptions result;result.stateDirectory=result.credentialDirectory=directory.path();
        result.socketPath=directory.filePath("calendar.sock");result.vaultSocket=directory.filePath("vault.sock");
        result.calendarTransport=&agenda;result.upcomingTransport=&upcoming;result.elapsedMs=[this]{return elapsed;};return result;
    }
};
}

class CalendarRecoveryTests:public QObject {
    Q_OBJECT
private slots:
    void init(){
        const auto parsed=HouseholdProfile::parse({{"version",1},{"timezone","UTC"},{"familyContext","Fictional family."},{"weather",QJsonValue::Null}});
        const auto *profile=std::get_if<HouseholdProfile>(&parsed);QVERIFY(profile);setHouseholdProfile(*profile);
    }
    void transientRejectionRecoversAndRetainsData_data(){
        QTest::addColumn<int>("code");QTest::addColumn<bool>("briefingReader");
        QTest::newRow("agenda-401")<<401<<false;QTest::newRow("agenda-403")<<403<<false;
        QTest::newRow("briefing-401")<<401<<true;QTest::newRow("briefing-403")<<403<<true;
    }
    void transientRejectionRecoversAndRetainsData(){
        QFETCH(int,code);QFETCH(bool,briefingReader);
        Fixture fixture;QVERIFY(fixture.seed());CalendarService service(getuid(),fixture.options());
        QTRY_VERIFY(service.lastChecked.isValid()&&service.appleUpcomingChecked.isValid());
        const auto selected=service.selected;const auto checked=service.lastChecked;const auto revision=service.eventsRevision;
        QFile saved(fixture.directory.filePath("briefing.json"));QVERIFY(saved.open(QIODevice::ReadOnly));const auto savedBytes=saved.readAll();saved.close();
        const auto cached=service.briefing.snapshot();QCOMPARE(cached["text"].toString(),QString("Cached useful briefing."));
        if(briefingReader){fixture.upcoming.probeCode=code;service.refreshUpcoming();}
        else {fixture.agenda.probeCode=code;service.fetch();}
        QTRY_VERIFY(service.recovery.recovering()&&!service.authenticated);
        QCOMPARE(service.eventsRevision,revision);QCOMPARE(service.lastChecked,checked);QCOMPARE(service.events.size(),1);
        QCOMPARE(service.selected.first().url,selected.first().url);QVERIFY(!service.needsSelection);
        const auto stale=service.briefing.snapshot();QCOMPARE(stale["text"],cached["text"]);QCOMPARE(stale["generated"],cached["generated"]);
        QCOMPARE(stale["spentMicros"],cached["spentMicros"]);QVERIFY(stale["stale"].toBool());
        QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),savedBytes);saved.close();
        QCOMPARE(service.calendarIssue,QString("refresh"));QVERIFY(service.status.contains("retry pending"));
        const int agendaDiscoveries=fixture.agenda.discoveries,upcomingDiscoveries=fixture.upcoming.discoveries;
        fixture.elapsed=IcloudRecovery::FirstDelayMs-1;
        for(int i=0;i<10;++i){service.reconnect();service.refreshUpcoming();}
        QVERIFY(command(service.options.socketPath,{{"type","fetch"},{"month",service.month.toString(Qt::ISODate)}})["ok"].toBool());
        QVERIFY(command(service.options.socketPath,{{"type","retryIcloud"}})["ok"].toBool());
        QCOMPARE(fixture.agenda.discoveries,agendaDiscoveries);QCOMPARE(fixture.upcoming.discoveries,upcomingDiscoveries);
        fixture.agenda.probeCode=fixture.upcoming.probeCode=207;fixture.elapsed=IcloudRecovery::FirstDelayMs;
        service.reconnect();
        QTRY_VERIFY(!service.recovery.recovering()&&service.authenticated&&service.appleUpcomingChecked.isValid());
        QCOMPARE(service.selected.first().url,selected.first().url);QVERIFY(!service.needsSelection);QVERIFY(service.calendarIssue.isEmpty());
        QCOMPARE(service.briefing.snapshot()["text"],cached["text"]);
    }
    void discoveryRejectionDoesNotRequireReboot(){
        Fixture fixture;QVERIFY(fixture.seed(false));fixture.agenda.discoveryCode=401;
        CalendarService service(getuid(),fixture.options());QTRY_VERIFY(service.recovery.recovering());
        QCOMPARE(service.restoreUrls.size(),1);QCOMPARE(fixture.upcoming.discoveries,0);
        fixture.agenda.discoveryCode=207;fixture.elapsed=IcloudRecovery::FirstDelayMs;service.reconnect();
        QTRY_VERIFY(service.lastChecked.isValid());QVERIFY(!service.recovery.recovering());QCOMPARE(service.selected.size(),1);
    }
    void repeatedRejectionPausesAndManualRetryIsBounded_data(){
        QTest::addColumn<bool>("briefingReader");
        QTest::newRow("agenda-rejects")<<false;QTest::newRow("only-briefing-rejects")<<true;
    }
    void repeatedRejectionPausesAndManualRetryIsBounded(){
        QFETCH(bool,briefingReader);
        Fixture fixture;QVERIFY(fixture.seed());
        if(briefingReader)fixture.upcoming.probeCode=403;else fixture.agenda.reportCode=401;
        CalendarService service(getuid(),fixture.options());QTRY_VERIFY(service.recovery.recovering()&&!service.authenticated);
        fixture.elapsed=IcloudRecovery::FirstDelayMs;service.reconnect();
        QTRY_VERIFY(!service.connecting&&!service.authenticated);QVERIFY(!service.recovery.paused());
        fixture.elapsed+=IcloudRecovery::LaterDelayMs;service.reconnect();QTRY_VERIFY(service.recovery.paused());
        QCOMPARE(service.calendarIssue,QString("sign-in"));QVERIFY(service.status.contains("automatic retries paused"));
        const int attempts=fixture.agenda.discoveries;
        fixture.elapsed+=IcloudRecovery::LaterDelayMs-1;
        QVERIFY(command(service.options.socketPath,{{"type","retryIcloud"}})["ok"].toBool());
        // Re-entering the same credential must not renew the budget or erase data.
        QVERIFY(command(service.options.socketPath,{{"type","login"},{"account","fixture@example.invalid"},{"password","abcd-efgh-ijkl-mnop"}})["ok"].toBool());
        QCOMPARE(fixture.agenda.discoveries,attempts);QVERIFY(service.recovery.paused());
        fixture.elapsed+=24*60*60*1000;
        for(int i=0;i<10;++i){service.reconnect();service.refreshUpcoming();}
        QVERIFY(command(service.options.socketPath,{{"type","fetch"},{"month",service.month.toString(Qt::ISODate)}})["ok"].toBool());
        QCOMPARE(fixture.agenda.discoveries,attempts);
        QVERIFY(command(service.options.socketPath,{{"type","retryIcloud"}})["ok"].toBool());
        QTRY_VERIFY(service.recovery.paused()&&!service.authenticated);QCOMPARE(fixture.agenda.discoveries,attempts+1);
        fixture.agenda.probeCode=fixture.agenda.reportCode=fixture.upcoming.probeCode=207;fixture.elapsed+=IcloudRecovery::LaterDelayMs;
        QVERIFY(command(service.options.socketPath,{{"type","retryIcloud"}})["ok"].toBool());
        QTRY_VERIFY(service.authenticated&&!service.recovery.recovering());QCOMPARE(service.selected.size(),1);
        // Full recovery renews the budget: the next isolated rejection uses the
        // first cooldown, rather than inheriting the exhausted thirty-minute one.
        fixture.agenda.probeCode=401;service.fetch();QTRY_VERIFY(!service.authenticated);
        const int before=fixture.agenda.discoveries;fixture.elapsed+=IcloudRecovery::FirstDelayMs-1;
        service.reconnect();QCOMPARE(fixture.agenda.discoveries,before);
        ++fixture.elapsed;fixture.agenda.probeCode=207;service.reconnect();QTRY_VERIFY(!service.recovery.recovering());
    }
    void rejectedConnectionCancelsOtherReaderAndForgetStillClearsData(){
        Fixture fixture;QVERIFY(fixture.seed());CalendarService service(getuid(),fixture.options());
        QTRY_VERIFY(service.lastChecked.isValid()&&service.appleUpcomingChecked.isValid());
        fixture.upcoming.hangNext=true;service.refreshUpcoming();QVERIFY(fixture.upcoming.hanging);
        bool cancelled=false;
        connect(&service.caldav,&Caldav::authorizationRejected,this,[&]{
            if(fixture.upcoming.hanging){cancelled=fixture.upcoming.hanging->aborts>0;fixture.upcoming.hanging->lateCompletion();}
        });
        fixture.agenda.probeCode=403;service.fetch();QTRY_VERIFY(service.recovery.recovering());
        QVERIFY(cancelled);
        QVERIFY(!service.appleUpcomingChecked.isValid());QVERIFY(!service.authenticated);
        QCOMPARE(service.briefing.snapshot()["text"].toString(),QString("Cached useful briefing."));
        QVERIFY(command(service.options.socketPath,{{"type","forget"}})["ok"].toBool());
        QVERIFY(service.events.isEmpty());QVERIFY(service.selected.isEmpty());QVERIFY(service.briefing.snapshot()["text"].toString().isEmpty());
        fixture.elapsed+=IcloudRecovery::LaterDelayMs;const int requests=fixture.agenda.discoveries;service.reconnect();QCOMPARE(fixture.agenda.discoveries,requests);
    }
};
QTEST_GUILESS_MAIN(CalendarRecoveryTests)
#include "calendar-recovery-tests.moc"
