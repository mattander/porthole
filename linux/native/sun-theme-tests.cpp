#include "sun-theme.h"
#include <QJsonArray>
#include <QtTest>

namespace {
QJsonObject daily(){return {{"time",QJsonArray{"2026-10-14","2026-10-15"}},{"sunrise",QJsonArray{"2026-10-14T07:35","2026-10-15T07:36"}},{"sunset",QJsonArray{"2026-10-14T18:35","2026-10-15T18:33"}}};}
}
class SunThemeTests:public QObject {
    Q_OBJECT
private slots:
    void flipsExactlyAtSunriseAndSunsetInTheConfiguredZone(){
        const QTimeZone zone("America/Toronto");const auto schedule=SunThemeSchedule::fromWeather(daily(),zone);
        for(const auto time:{QTime(0,0),QTime(7,34,59),QTime(7,35),QTime(12,0),QTime(18,34,59),QTime(18,35),QTime(23,59)}){
            const QDateTime now(QDate(2026,10,14),time,zone);const auto dark=schedule.darkAt(now.toUTC());QVERIFY(dark);QCOMPARE(*dark,time<QTime(7,35)||time>=QTime(18,35));
        }
        QVERIFY(schedule.descriptionAt(QDateTime(QDate(2026,10,14),QTime(20,0),zone)).contains("7:36 AM"));
        QVERIFY(!schedule.darkAt(QDateTime(QDate(2026,10,16),QTime(12,0),zone)));
    }
    void missingAndInvalidTimesNeverGuessATheme(){
        auto invalid=daily();invalid["sunrise"]=QJsonArray{"2026-10-14T19:00","bad"};
        const auto schedule=SunThemeSchedule::fromWeather(invalid,QTimeZone::utc());QVERIFY(schedule.isEmpty());QVERIFY(!schedule.darkAt(QDateTime::currentDateTimeUtc()));
        QVERIFY(SunThemeSchedule::fromWeather({},QTimeZone::utc()).isEmpty());
        invalid=daily();invalid["sunrise"]=QJsonArray{"2026-10-13T07:35",QJsonValue::Null};QVERIFY(SunThemeSchedule::fromWeather(invalid,QTimeZone::utc()).isEmpty());
    }
    void daylightSavingChangesUseTheLocationsWallTimes(){
        const QJsonObject values{{"time",QJsonArray{"2026-11-01"}},{"sunrise",QJsonArray{"2026-11-01T07:00"}},{"sunset",QJsonArray{"2026-11-01T17:00"}}};
        const auto schedule=SunThemeSchedule::fromWeather(values,QTimeZone("America/Toronto"));
        QCOMPARE(schedule.darkAt(QDateTime(QDate(2026,11,1),QTime(11,59),QTimeZone::utc())),std::optional<bool>(true));
        QCOMPARE(schedule.darkAt(QDateTime(QDate(2026,11,1),QTime(12,0),QTimeZone::utc())),std::optional<bool>(false));
        QCOMPARE(schedule.darkAt(QDateTime(QDate(2026,11,1),QTime(22,0),QTimeZone::utc())),std::optional<bool>(true));
    }
    void cachedTimesSurviveRestartButCannotFollowAnotherProfile(){
        const HouseholdProfile profile;const auto schedule=SunThemeSchedule::fromWeather(daily(),profile.zone());
        const auto restored=SunThemeSchedule::fromCache(schedule.cache(profile),profile);QVERIFY(!restored.isEmpty());
        QCOMPARE(restored.darkAt(QDateTime(QDate(2026,10,15),QTime(12,0),QTimeZone::utc())),std::optional<bool>(false));
        auto changed=profile.json();changed["timezone"]="America/Toronto";const auto parsed=HouseholdProfile::parse(changed);const auto *other=std::get_if<HouseholdProfile>(&parsed);QVERIFY(other);
        QVERIFY(SunThemeSchedule::fromCache(schedule.cache(profile),*other).isEmpty());QVERIFY(SunThemeSchedule::fromCache("broken",profile).isEmpty());
        QVERIFY(SunThemeSchedule::fromCache(QByteArray(4097,'x'),profile).isEmpty());
    }
};
QTEST_GUILESS_MAIN(SunThemeTests)
#include "sun-theme-tests.moc"
