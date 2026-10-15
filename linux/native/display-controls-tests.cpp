#include "display-controls.h"
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>
#include <limits>
#include <sys/stat.h>

namespace {
void put(const QString &path,const QByteArray &bytes){QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));QCOMPARE(file.write(bytes),bytes.size());}
}
class DisplayTests:public QObject {
    Q_OBJECT
private slots:
    void legacySolarPreferenceAndNewModesSurviveRestart(){
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("preferences.ini");
        {QSettings settings(path,QSettings::IniFormat);QCOMPARE(appearanceMode(settings),AppearanceMode::Manual);settings.setValue("display/followSun",true);settings.sync();}
        {QSettings settings(path,QSettings::IniFormat);QCOMPARE(appearanceMode(settings),AppearanceMode::SunSchedule);saveAppearanceMode(settings,AppearanceMode::AmbientLight,true);QVERIFY(!settings.value("display/followSun").toBool());}
        {QSettings settings(path,QSettings::IniFormat);QCOMPARE(appearanceMode(settings),AppearanceMode::AmbientLight);QVERIFY(settings.value("display/darkMode").toBool());saveAppearanceMode(settings,AppearanceMode::SunSchedule,false);QVERIFY(settings.value("display/followSun").toBool());}
        {QSettings settings(path,QSettings::IniFormat);QCOMPARE(appearanceMode(settings),AppearanceMode::SunSchedule);settings.setValue("display/appearanceMode","invalid");QCOMPARE(appearanceMode(settings),AppearanceMode::Manual);saveAppearanceMode(settings,AppearanceMode::Manual,false);}
        QSettings settings(path,QSettings::IniFormat);QCOMPARE(appearanceMode(settings),AppearanceMode::Manual);
    }
    void sensorNeedsContinuousStableReadingsAndHysteresis(){
        AmbientTheme theme;
        for(qint64 time:{0,5000,10000})QVERIFY(!theme.observe(10,time,false));QCOMPARE(theme.observe(10,15000,false),std::optional<bool>(true));
        for(qint64 time:{20000,25000,30000,35000})QVERIFY(!theme.observe(25,time,true));
        for(qint64 time:{40000,45000,50000})QVERIFY(!theme.observe(50,time,true));QCOMPARE(theme.observe(50,55000,true),std::optional<bool>(false));
        QVERIFY(!theme.observe(10,60000,false));QVERIFY(!theme.observe({},65000,false));QVERIFY(!theme.observe(10,70000,false));QVERIFY(!theme.observe(10,80000,false));QVERIFY(!theme.observe(10,85000,false));
        QVERIFY(!theme.observe(10,90000,false));QCOMPARE(theme.observe(10,95000,false),std::optional<bool>(true));
        for(double value:{-1.0,200001.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})QVERIFY(!theme.observe(value,100000,false));
        QVERIFY(!theme.observe(10,100000,false));QVERIFY(!theme.observe(10,99000,false));theme.reset();QVERIFY(!theme.observe(10,120000,false));
    }
    void backlightRejectsInvalidWritesAndConfirmsAppliedSteps(){
        QTemporaryDir dir;QVERIFY(dir.isValid());Backlight backlight(dir.path());QVERIFY(!backlight.state());
        put(dir.filePath("brightness"),"8\n");put(dir.filePath("max_brightness"),"10\n");const auto state=backlight.state();QVERIFY(state);QCOMPARE(state->percent(),80);
        for(int invalid:{-10,0,11,100000})QVERIFY(!backlight.setLevel(invalid));QCOMPARE(backlight.state()->level,8);
        const auto dim=backlight.setPercent(1);QVERIFY(dim);QCOMPARE(dim->level,1);QCOMPARE(dim->percent(),10);
        QVERIFY(backlight.setLevel(6));QCOMPARE(backlight.state()->percent(),60);QVERIFY(backlight.setPercent(100));QCOMPARE(backlight.state()->level,10);
        for(int invalid:{-1,0,101})QVERIFY(!backlight.setPercent(invalid));QCOMPARE(backlight.state()->level,10);
        for(const auto invalid:{QByteArray("bad"),QByteArray("0"),QByteArray("10001"),QByteArray(65,'1')}){put(dir.filePath("max_brightness"),invalid);QVERIFY(!backlight.state());QVERIFY(!backlight.setLevel(1));}
        put(dir.filePath("max_brightness"),"10");put(dir.filePath("brightness"),"11");QVERIFY(!backlight.state());
    }
    void sensorReadsLuxAndRejectsMissingOrMalformedData(){
        QTemporaryDir dir;QVERIFY(dir.isValid());AmbientSensor sensor(nullptr,dir.path());QSignalSpy readings(&sensor,&AmbientSensor::measured),missing(&sensor,&AmbientSensor::unavailable);
        sensor.sample();QCOMPARE(missing.count(),1);QVERIFY(QDir(dir.path()).mkpath("iio:device0"));const auto input=dir.filePath("iio:device0/in_illuminance_input");
        put(input,"42.5\n");sensor.sample();QTRY_COMPARE(readings.count(),1);QCOMPARE(readings.first().first().toDouble(),42.5);
        for(const auto invalid:{QByteArray("nan"),QByteArray("-1"),QByteArray("200001"),QByteArray("40 garbage"),QByteArray(65,'1')}){const auto before=missing.count();put(input,invalid);sensor.sample();QTRY_COMPARE(missing.count(),before+1);QCOMPARE(readings.count(),1);}
        QVERIFY(QFile::remove(input));put(dir.filePath("iio:device0/in_intensity_both_raw"),"42");sensor.sample();QCOMPARE(missing.count(),7);
    }
    void blockedSensorTimesOutWithoutBlockingTheEventLoop(){
        QTemporaryDir dir;QVERIFY(dir.isValid());QVERIFY(QDir(dir.path()).mkpath("iio:device0"));const auto input=dir.filePath("iio:device0/in_illuminance_input");QCOMPARE(mkfifo(QFile::encodeName(input).constData(),0600),0);
        AmbientSensor sensor(nullptr,dir.path());QSignalSpy missing(&sensor,&AmbientSensor::unavailable);int beats=0;QTimer heartbeat;connect(&heartbeat,&QTimer::timeout,this,[&]{++beats;});heartbeat.start(50);
        sensor.sample();sensor.sample();QTRY_COMPARE_WITH_TIMEOUT(missing.count(),1,4000);QVERIFY(beats>=10);
    }
};
QTEST_GUILESS_MAIN(DisplayTests)
#include "display-controls-tests.moc"
