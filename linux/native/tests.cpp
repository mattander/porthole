#include "profile.h"
#include "data.h"
#include "caldav.h"
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <stdexcept>

class Tests : public QObject {
    Q_OBJECT
private slots:
    void init(){
        const auto parsed=HouseholdProfile::parse({{"version",1},{"timezone","America/New_York"},{"familyContext","A student's school lunches and weekday planning are relevant."},{"weather",QJsonValue::Null}});
        if(const auto *profile=std::get_if<HouseholdProfile>(&parsed))setHouseholdProfile(*profile);else QFAIL("Invalid timezone fixture");
    }

    void addressesNeverSendCredentialsOutsideCalendarService() {
        QVERIFY(permittedCalendarUrl(QUrl("https://caldav.icloud.com/")));
        QVERIFY(permittedCalendarUrl(QUrl("https://p42-caldav.icloud.com/123/calendars/")));
        for(const auto &url:QStringList{"http://caldav.icloud.com/","https://caldav.icloud.com.evil.example/","https://evil.icloud.com/","https://user:password@caldav.icloud.com/","https://caldav.icloud.com:444/","https://127.0.0.1/","file:///etc/passwd","https://caldav.icloud.com/#fragment"})
            QVERIFY2(!permittedCalendarUrl(QUrl(url)),qPrintable(url));
        QCOMPARE(calendarHref(QUrl("https://p42-caldav.icloud.com/home/"),"family/"),QUrl("https://p42-caldav.icloud.com/home/family/"));
        QVERIFY_EXCEPTION_THROWN(calendarHref(QUrl("https://caldav.icloud.com/"),"//evil.example/"),std::runtime_error);
    }
    void calendarColoursAndIdentityAreCanonicalAndStable() {
        QCOMPARE(canonicalCalendarColour("#12abEF"),QString("#12ABEF"));
        QCOMPARE(canonicalCalendarColour("#12abEF80"),QString("#12ABEF80"));
        QVERIFY(canonicalCalendarColour({}).isEmpty());
        for(const auto &invalid:QStringList{"12ABEF","#123","#123456789","#GG0000"," #123456","#123456\n"})
            QVERIFY(canonicalCalendarColour(invalid).isEmpty());
        const Calendar family{QUrl("https://caldav.icloud.com/family/"),"Family","#123456"};
        const Calendar renamed{family.url,"Renamed","#ABCDEF"};
        const Calendar second{QUrl("https://caldav.icloud.com/home/"),"Family","#123456"};
        QCOMPARE(calendarIdentity(family),calendarIdentity(renamed));
        QVERIFY(calendarIdentity(family)!=calendarIdentity(second));
        QCOMPARE(calendarIdentity(family).size(),64);
        QVERIFY(QRegularExpression("\\A[0-9a-f]{64}\\z").match(calendarIdentity(family)).hasMatch());
    }
    void calendarEventIpcValidatesMetadata() {
        const Calendar source{QUrl("https://caldav.icloud.com/family/"),"Family","#12ab34ef"};
        Event event{"fixture","Dinner","Home",source.name,QDateTime(QDate(2026,10,14),QTime(18,0),configuredZone()),QDateTime(QDate(2026,10,14),QTime(19,0),configuredZone()),false,calendarIdentity(source),source.colour};
        const auto parsed=calendarEventFromJson(calendarEventJson(event));QVERIFY(parsed);QCOMPARE(parsed->calendarId,event.calendarId);QCOMPARE(parsed->colour,QString("#12AB34EF"));
        auto invalid=calendarEventJson(event);invalid["calendarId"]="private-provider-url";invalid["colour"]="#xyz";
        const auto sanitized=calendarEventFromJson(invalid);QVERIFY(sanitized);QVERIFY(sanitized->calendarId.isEmpty());QVERIFY(sanitized->colour.isEmpty());
        invalid=calendarEventJson(event);invalid["calendarId"]=event.calendarId+'\n';invalid["colour"]="#123456\n";
        const auto newline=calendarEventFromJson(invalid);QVERIFY(newline);QVERIFY(newline->calendarId.isEmpty());QVERIFY(newline->colour.isEmpty());
        invalid["start"]="not-a-date";QVERIFY(!calendarEventFromJson(invalid));
    }
    void selectionReconciliationUsesProviderIdentity() {
        const Calendar first{QUrl("googlecalendar:/one"),"Same","#111111"};
        const Calendar recoloured{first.url,"Renamed","#222222"};
        const Calendar removed{QUrl("googlecalendar:/two"),"Same","#111111"};
        QVERIFY(sameCalendarSelection({first},{recoloured}));
        QVERIFY(sameCalendarSelection({first,removed},{removed,recoloured}));
        QVERIFY(!sameCalendarSelection({first},{removed}));
        QVERIFY(!sameCalendarSelection({first,removed},{first}));
        QVERIFY(!sameCalendarSelection({first,first},{first,removed}));
    }
    void discoveryUsesNamespacesAndSuccessfulProperties() {
        const auto responses=parseDav(R"(<d:multistatus xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav" xmlns:a="http:)"
                                      "//apple.com/ns/ical/"
                                      R"("><d:response><d:href>/family/</d:href><d:propstat><d:prop><d:displayname>Family &amp; friends</d:displayname><d:resourcetype><d:collection/><c:calendar/></d:resourcetype><c:supported-calendar-component-set><c:comp name="VEVENT"/></c:supported-calendar-component-set><a:calendar-color>#1a2B3c80</a:calendar-color></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat><d:propstat><d:prop><c:calendar-home-set><d:href>https://evil.example/</d:href></c:calendar-home-set></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat></d:response></d:multistatus>)");
        QCOMPARE(responses.size(),1);
        QCOMPARE(responses[0].properties.value(davProperty("displayname")),QString("Family & friends"));
        QCOMPARE(responses[0].properties.value(davProperty("resourcetype")),QString("calendar"));
        QCOMPARE(responses[0].properties.value("http://apple.com/ns/ical/|calendar-color"),QString("#1a2B3c80"));
        QVERIFY(!responses[0].properties.contains(calProperty("calendar-home-set")));
        QVERIFY_EXCEPTION_THROWN(parseDav("<multistatus/>"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(parseDav("<d:multistatus xmlns:d='DAV:'></d:multistatus>bad"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(parseDav(QByteArray(4*1024*1024+1,'x')),std::runtime_error);
    }
    void timezoneAndExclusiveAllDayEnd() {
        auto events=parseExpandedCalendar("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:overnight\r\nDTSTART;TZID=America/New_York:20261013T233000\r\nDTEND;TZID=America/New_York:20261014T010000\r\nSUMMARY:Overnight\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:holiday\r\nDTSTART;VALUE=DATE:20261013\r\nDTEND;VALUE=DATE:20261015\r\nSUMMARY:Two-day holiday\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n","Family");
        QCOMPARE(events.size(),2);
        QCOMPARE(events[0].start.toUTC().toString(Qt::ISODate),QString("2026-10-14T03:30:00Z"));
        QVERIFY(events[0].onDate(QDate(2026,10,13)));QVERIFY(events[0].onDate(QDate(2026,10,14)));
        QVERIFY(events[1].onDate(QDate(2026,10,14)));QVERIFY(!events[1].onDate(QDate(2026,10,15)));
        QCOMPARE(events[1].timeLabel(),QString("All day"));
    }
    void expandedRecurrencesAndCancellation() {
        auto events=parseExpandedCalendar("BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nUID:series\nDTSTART:20261013T200000Z\nDTEND:20261013T210000Z\nSUMMARY:First occurrence\nEND:VEVENT\nBEGIN:VEVENT\nUID:series\nRECURRENCE-ID:20261020T200000Z\nDTSTART:20261020T220000Z\nDTEND:20261020T230000Z\nSUMMARY:Moved occurrence\nEND:VEVENT\nBEGIN:VEVENT\nUID:series\nRECURRENCE-ID:20261027T200000Z\nSTATUS:CANCELLED\nEND:VEVENT\nEND:VCALENDAR\n","Family");
        QCOMPARE(events.size(),2);QCOMPARE(events[1].start.time(),QTime(18,0));
        QVERIFY_EXCEPTION_THROWN(parseExpandedCalendar("BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nUID:series\nDTSTART:20261013T200000Z\nRRULE:FREQ=WEEKLY\nEND:VEVENT\nEND:VCALENDAR\n","Family"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(parseExpandedCalendar("not a calendar","Family"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(parseExpandedCalendar("BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nUID:bad-zone\nDTSTART;TZID=Unknown/Test:20261013T200000\nEND:VEVENT\nEND:VCALENDAR\n","Family"),std::runtime_error);
    }
    void utcAcrossDaylightSavingsTransition() {
        const auto events=parseExpandedCalendar("BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nUID:before\nDTSTART:20261101T053000Z\nEND:VEVENT\nBEGIN:VEVENT\nUID:after\nDTSTART:20261101T063000Z\nEND:VEVENT\nEND:VCALENDAR\n","Family");
        QCOMPARE(events[0].start.time(),QTime(1,30));QCOMPARE(events[1].start.time(),QTime(1,30));
        QCOMPARE(events[0].start.offsetFromUtc(),-14400);QCOMPARE(events[1].start.offsetFromUtc(),-18000);
    }
    void primaryPasswordRejectedBeforeAnyRequest() {
        Caldav client;QSignalSpy failure(&client,&Caldav::failed);
        client.connectAccount("test@example.invalid","PrimaryPassword123!");
        QCOMPARE(failure.size(),1);QVERIFY(failure.first().first().toString().contains("app-specific"));
    }
    void unicodeCalendarTitlesSurviveIcalAndIpc(){
        const QString title="Café 🥘 👨‍👩‍👧‍👦 👍🏽 🇨🇦";
        const auto ics=QString("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:unicode\r\nDTSTART:20261014T180000Z\r\nDTEND:20261014T190000Z\r\nSUMMARY:%1\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n").arg(title).toUtf8();
        const auto events=parseExpandedCalendar(ics,"Family");QCOMPARE(events.size(),1);QCOMPARE(events[0].title,title);
        const auto restored=calendarEventFromJson(calendarEventJson(events[0]));QVERIFY(restored);QCOMPARE(restored->title,title);
    }
};
QTEST_GUILESS_MAIN(Tests)
#include "tests.moc"
