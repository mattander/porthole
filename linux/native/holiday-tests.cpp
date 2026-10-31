#include "holidays.h"
#include "briefing.h"
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <cstring>

namespace {
const QDateTime Time(QDate(2026,10,31),QTime(9,0),QTimeZone("America/Toronto"));
QByteArray fixture(){return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Porthole//Test//EN\r\n"
    "BEGIN:VEVENT\r\nUID:autumn\r\nDTSTART;VALUE=DATE:20251031\r\nSUMMARY:Halloween\r\nRRULE:FREQ=YEARLY;COUNT=5\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nUID:family\r\nDTSTART;VALUE=DATE:20250217\r\nSUMMARY:Family Day (AB\\, ON)\r\nRRULE:FREQ=YEARLY;COUNT=5;BYDAY=3MO;BYMONTH=2\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nUID:thanks\r\nDTSTART;VALUE=DATE:20251013\r\nSUMMARY:Thanksgiving\r\nRRULE:FREQ=YEARLY;COUNT=5;BYDAY=2MO;BYMONTH=10\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nUID:qc\r\nDTSTART;VALUE=DATE:20250624\r\nSUMMARY:Regional day (QC)\r\nRRULE:FREQ=YEARLY;COUNT=5\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";}
HouseholdProfile profile(QString label="Example City, Ontario",QString zone="America/Toronto"){
    const auto parsed=HouseholdProfile::parse({{"version",1},{"timezone",zone},{"familyContext",""},{"weather",QJsonObject{{"label",label},{"latitude",43.6},{"longitude",-80.0}}}});
    if(const auto *value=std::get_if<HouseholdProfile>(&parsed))return *value;
    qFatal("Invalid fixture profile");
}
class Reply:public QNetworkReply {
    QByteArray body;qint64 offset=0;
public:
    Reply(QNetworkRequest request,QByteArray bytes,int status,QObject *parent):QNetworkReply(parent),body(std::move(bytes)){
        setRequest(request);setUrl(request.url());setOperation(QNetworkAccessManager::GetOperation);setAttribute(QNetworkRequest::HttpStatusCodeAttribute,status);open(QIODevice::ReadOnly);
        QTimer::singleShot(0,this,[this]{setFinished(true);emit readyRead();emit finished();});
    }
    void abort()override{setError(OperationCanceledError,"Cancelled fixture");}
    qint64 bytesAvailable()const override{return body.size()-offset+QNetworkReply::bytesAvailable();}
protected:
    qint64 readData(char *data,qint64 maximum)override{
        const qint64 remaining=body.size()-offset;if(remaining<=0)return -1;
        const auto length=qMin(maximum,remaining);std::memcpy(data,body.constData()+offset,length);offset+=length;return length;
    }
};
class Network:public QNetworkAccessManager {
public:
    int requests=0,code=200;QByteArray body=fixture();QNetworkRequest last;
protected:
    QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *)override{++requests;last=request;return new Reply(request,body,code,this);}
};
}
class HolidayTests:public QObject {
    Q_OBJECT
private slots:
    void recurringDatesAndOntarioFiltering(){
        const auto data=parsePublicHolidays(fixture(),QDate(2026,1,1),QDate(2026,12,31),profile());QVERIFY(data);QCOMPARE(data->size(),3);
        QCOMPARE(data->at(0).start.date(),QDate(2026,2,16));QCOMPARE(data->at(1).start.date(),QDate(2026,10,12));QCOMPARE(data->at(2).title,QString("Halloween"));
        QCOMPARE(data->at(2).start.date(),QDate(2026,10,31));QCOMPARE(data->at(2).end.date(),QDate(2026,11,1));QVERIFY(data->at(2).allDay);QVERIFY(!data->at(2).onDate(QDate(2026,11,1)));
        const auto next=parsePublicHolidays(fixture(),QDate(2027,2,1),QDate(2027,2,28),profile());QVERIFY(next);QCOMPARE(next->first().start.date(),QDate(2027,2,15));
        const auto unknown=parsePublicHolidays(fixture(),QDate(2026,1,1),QDate(2026,12,31),profile("Example City"));QVERIFY(unknown);QCOMPARE(unknown->size(),2);
        const auto quebec=parsePublicHolidays(fixture(),QDate(2026,6,1),QDate(2026,6,30),profile("Example City, QC"));QVERIFY(quebec);QCOMPARE(quebec->size(),1);
    }
    void exceptionsAndUntrustedData(){
        auto metadata=fixture();metadata.replace("BEGIN:VEVENT\r\n","BEGIN:VEVENT\r\nDTSTAMP;VALUE=DATE:19760401\r\n");
        QVERIFY(parsePublicHolidays(metadata,Time.date(),Time.date(),profile()));
        auto bytes=fixture();bytes.replace("RRULE:FREQ=YEARLY;COUNT=5\r\nEND:VEVENT","RRULE:FREQ=YEARLY;COUNT=5\r\nEXDATE;VALUE=DATE:20261031\r\nRDATE;VALUE=DATE:20261101\r\nEND:VEVENT");
        const auto dates=parsePublicHolidays(bytes,QDate(2026,10,31),QDate(2026,11,2),profile());QVERIFY(dates);QCOMPARE(dates->size(),1);QCOMPARE(dates->first().start.date(),QDate(2026,11,1));QCOMPARE(dates->first().end.time(),QTime(0,0));
        QVERIFY(dates->first().start.secsTo(dates->first().end)==25*3600);
        bytes=fixture();bytes.replace("FREQ=YEARLY","FREQ=DAILY");QVERIFY(!parsePublicHolidays(bytes,QDate(2026,1,1),QDate(2026,12,31),profile()));
        bytes=fixture();bytes.replace("DTSTART;VALUE=DATE:20251031","DTSTART:20251031T090000Z");QVERIFY(!parsePublicHolidays(bytes,QDate(2026,1,1),QDate(2026,12,31),profile()));
        QVERIFY(!parsePublicHolidays(QByteArray(300000,'x'),Time.date(),Time.date(),profile()));QVERIFY(!parsePublicHolidays("not a calendar",Time.date(),Time.date(),profile()));
    }
    void cachedPublicDatesAndFailuresPreservePersonalData(){
        QTemporaryDir dir;Network network;auto time=Time;const auto path=dir.filePath("holidays.json");
        HolidayCalendar calendar(path,nullptr,&network,[&]{return time;});calendar.refresh(profile());QTRY_COMPARE(network.requests,1);QTRY_COMPARE(calendar.events(Time.date(),Time.date()).size(),1);
        QCOMPARE(network.last.url(),QUrl("https://calendars.icloud.com/holidays/ca_en.ics/"));QVERIFY(!network.last.hasRawHeader("Authorization"));QVERIFY(!network.last.hasRawHeader("Cookie"));
        QCOMPARE(network.last.attribute(QNetworkRequest::RedirectPolicyAttribute).value<QNetworkRequest::RedirectPolicy>(),QNetworkRequest::ManualRedirectPolicy);
        HolidayCalendar restored(path,nullptr,&network,[&]{return time;});restored.refresh(profile());QCOMPARE(restored.events(Time.date(),Time.date()).size(),1);QCOMPARE(network.requests,1);
        time=time.addDays(1);network.code=302;restored.refresh(profile());QTRY_COMPARE(network.requests,2);QTRY_VERIFY(restored.status().contains("saved dates"));QCOMPARE(restored.events(Time.date(),Time.date()).size(),1);
        restored.refresh(profile());QCOMPARE(network.requests,2);restored.refresh(profile("Example City","Europe/London"));QVERIFY(restored.events(Time.date(),Time.date()).isEmpty());QVERIFY(restored.status().isEmpty());QCOMPARE(network.requests,2);
    }
    void duplicateHolidayKeepsPersonalSourceAndSeparateActivities(){
        const auto data=parsePublicHolidays(fixture(),Time.date(),Time.date(),profile());QVERIFY(data);auto personal=data->first();personal.uid="personal";personal.calendar="Family";personal.calendarId="family";personal.title="HALLOWEEN";
        auto party=personal;party.uid="party";party.title="Halloween party";party.allDay=false;party.start=Time.addSecs(3600);party.end=party.start.addSecs(3600);
        const auto merged=mergePublicHolidays({personal,party},*data);QCOMPARE(merged.size(),2);QCOMPARE(merged.first().calendar,QString("Family"));QCOMPARE(merged.last().title,QString("Halloween party"));
        const auto details=calendarEventDetailsJson(merged,calendarEventReference(merged.first()));QVERIFY(details["ok"].toBool());
        const auto context=briefingContext(mergePublicHolidays({},*data),{},Time);
        QCOMPARE(context["events"].toArray().first().toObject()["title"].toString(),QString("Halloween"));
        QCOMPARE(context["events"].toArray().first().toObject()["days_from_today"].toInt(-1),0);
    }
    void storageFailureKeepsFreshDatesVisible(){
        QTemporaryDir dir;Network network;HolidayCalendar calendar(dir.filePath("missing/cache"),nullptr,&network,[]{return Time;});calendar.refresh(profile());
        QTRY_COMPARE(calendar.events(Time.date(),Time.date()).size(),1);QVERIFY(calendar.status().contains("cache could not be saved"));
    }
    void realProviderFixture(){
        const auto path=qEnvironmentVariable("PORTHOLE_HOLIDAY_FIXTURE");if(path.isEmpty())QSKIP("Optional live-provider fixture not supplied");
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.size()<=256*1024);
        const auto result=parsePublicHolidays(file.readAll(),QDate(2026,10,31),QDate(2026,10,31),profile());QVERIFY(result);QCOMPARE(result->size(),1);QCOMPARE(result->first().title,QString("Halloween"));QCOMPARE(result->first().start.date(),QDate(2026,10,31));
    }
    void realProviderNetwork(){
        if(qEnvironmentVariable("PORTHOLE_HOLIDAY_NETWORK")!="1")QSKIP("Optional public-network probe not enabled");
        QTemporaryDir dir;HolidayCalendar calendar(dir.filePath("calendar"),nullptr,nullptr,[]{return Time;});calendar.refresh(profile());
        QTRY_COMPARE_WITH_TIMEOUT(calendar.events(Time.date(),Time.date()).size(),1,40000);
        QCOMPARE(calendar.events(Time.date(),Time.date()).first().title,QString("Halloween"));QVERIFY(calendar.status().contains("available"));
    }
};
QTEST_GUILESS_MAIN(HolidayTests)
#include "holiday-tests.moc"
