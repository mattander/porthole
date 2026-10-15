#include "typography.h"
#include "dashboard-widgets.h"
#include "calendar-style.h"
#include "diagnostics.h"
#include <QImage>
#include <QFontInfo>
#include <QPainter>
#include <QTextLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>
#include <cmath>

namespace {
int inkBottom(const QImage &image,int first,int last){int bottom=-1;for(int y=0;y<image.height();++y)for(int x=qMax(0,first);x<qMin(image.width(),last);++x){const auto pixel=image.pixelColor(x,y);if(pixel.red()<100&&pixel.green()<100&&pixel.blue()<100)bottom=y;}return bottom;}
}
class LayoutTests:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QVERIFY(configureInterfaceTypography());}
    void bundledFontsResolveWithoutSystemInstallation(){
        for(const auto weight:{QFont::Normal,QFont::DemiBold,QFont::Bold}){
            const QFontInfo actual(interfaceFont(18,weight));QCOMPARE(actual.family(),QString("Poppins"));QCOMPARE(actual.weight(),weight);
        }
        QVERIFY(interfaceFontLicence().contains("SIL OPEN FONT LICENSE Version 1.1"));
    }
    void clockGlyphsShareBottomBaseline(){
        ClockFace clock;QPalette palette;palette.setColor(QPalette::Window,Qt::white);palette.setColor(QPalette::WindowText,Qt::black);clock.setPalette(palette);
        for(bool portrait:{false,true})for(QTime time:{QTime(0,5),QTime(12,59),QTime(23,11)}){
            clock.setPortrait(portrait);clock.resize(portrait?744:620,clock.height());clock.setTime(time);QVERIFY(clock.accessibleName().startsWith(QString::number(time.hour()%12?time.hour()%12:12)+":"));clock.show();QTest::qWait(10);
            QImage image(clock.size(),QImage::Format_ARGB32);image.fill(Qt::white);clock.render(&image);
            const int split=qRound(clock.splitPosition());const int digits=inkBottom(image,0,split-12),period=inkBottom(image,split,image.width());
            QVERIFY(digits>=0);QVERIFY(period>=0);QVERIFY2(std::abs(digits-period)<=2,"The visible AM/PM ink must share the time's baseline");
        }
    }
    void colourEmojiAndJoinedSequencesResolveAndPaint(){
        for(const auto &emoji:QStringList{"📚","🥘","⚽","❤️","👍🏽","👩🏽‍⚕️","👨‍👩‍👧‍👦","🇨🇦"}){
            QTextLayout layout(emoji,interfaceFont(32));layout.beginLayout();auto line=layout.createLine();line.setLineWidth(200);layout.endLayout();
            const auto runs=layout.glyphRuns();QCOMPARE(runs.size(),1);QCOMPARE(runs[0].rawFont().familyName(),QString("Noto Color Emoji"));
            QCOMPARE(runs[0].glyphIndexes().size(),1);QVERIFY(runs[0].glyphIndexes()[0]!=0);
            QImage image(200,80,QImage::Format_ARGB32);image.fill(Qt::white);QPainter painter(&image);layout.draw(&painter,QPointF(0,0));painter.end();
            int coloured=0,ink=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto pixel=image.pixelColor(x,y);if(pixel!=QColor(Qt::white))++ink;if(qMax(pixel.red(),qMax(pixel.green(),pixel.blue()))-qMin(pixel.red(),qMin(pixel.green(),pixel.blue()))>30)++coloured;}
            QVERIFY2(ink>20,qPrintable(emoji));if(emoji!="⚽"&&emoji!="👨‍👩‍👧‍👦")QVERIFY2(coloured>20,qPrintable(emoji));
        }
    }
    void footerControlsShareRowAndUpdateTextIsBottomAligned(){
        DashboardFooter footer;QSignalSpy views(&footer,&DashboardFooter::viewRequested),settings(&footer,&DashboardFooter::settingsRequested);
        const auto checked=QDateTime::currentDateTimeUtc();footer.setUpdateTimes(checked,checked,checked,true);footer.show();
        for(int width:{1216,748,576}){
            footer.resize(width,63);QTest::qWait(20);auto *updated=footer.findChild<QLabel*>("lastUpdated");QVERIFY(updated);
            auto *gear=footer.findChild<QPushButton*>("settingsButton");QVERIFY(gear);
            const int bottom=gear->geometry().bottom();QCOMPARE(updated->geometry().bottom(),bottom);
            for(auto *button:footer.findChildren<QPushButton*>()){QCOMPARE(button->geometry().bottom(),bottom);QVERIFY(footer.rect().contains(button->geometry()));QCOMPARE(button->height(),48);}
        }
        QCOMPARE(footer.findChildren<QPushButton*>().size(),2);auto *calendar=footer.findChild<QPushButton*>("calendarButton");QVERIFY(calendar);QTest::mouseClick(calendar,Qt::LeftButton);QCOMPARE(views.count(),1);QCOMPARE(views.first().first().toInt(),2);
        footer.setView(2);QVERIFY(calendar->isChecked());footer.setView(0);QVERIFY(!calendar->isChecked());
        auto *gear=footer.findChild<QPushButton*>("settingsButton");QTest::mouseClick(gear,Qt::LeftButton);QCOMPARE(settings.count(),1);
        footer.setIssueCount(2);QVERIFY(gear->accessibleName().contains("2"));
    }
    void weatherSymbolsHandleSupportedAndMissingConditions(){
        WeatherIcon icon;icon.show();for(int code:{0,1,2,3,45,48,51,61,71,80,85,95,99,-1,123}){
            icon.setCode(code);QTest::qWait(2);QImage image(icon.size(),QImage::Format_ARGB32);image.fill(Qt::transparent);icon.render(&image);
            QVERIFY(!image.isNull());QVERIFY(!icon.accessibleName().isEmpty());QCOMPARE(validWeatherCode(code),code>=0&&code!=123);
        }
        QCOMPARE(weatherCondition(3),QString("Cloudy"));QCOMPARE(weatherCondition(-1),QString("Unavailable"));
        ForecastStrip forecast;forecast.resize(620,85);forecast.setForecast({{QDate(2026,10,15),61,13,7},{QDate(2026,10,16),3,12,5},{QDate(2026,10,17),0,15,6}});forecast.show();QTest::qWait(10);QCOMPARE(forecast.findChildren<WeatherIcon*>().size(),3);
    }
    void weatherColoursRemainVisibleInBothThemesAndSizes(){
        for(bool dark:{false,true})for(int size:{40,64})for(int code:{0,2,3,45,61,71,95,-1}){
            WeatherIcon icon(size);QPalette palette;
            const QColor background=dark?QColor("#171d25"):QColor("#f6f5ef");
            palette.setColor(QPalette::Window,background);icon.setPalette(palette);icon.setCode(code);
            QImage image(icon.size(),QImage::Format_ARGB32);image.fill(background);icon.render(&image);
            int warm=0,cool=0,neutral=0;
            const int precipitationTop=qRound(size*40.0/64);
            for(int y=0;y<size;++y)for(int x=0;x<size;++x){
                const auto pixel=image.pixelColor(x,y);
                if(calendarContrast(pixel,background)<3.0)continue;
                const bool warmPixel=pixel.red()>pixel.blue()+35&&pixel.green()>pixel.blue()+20;
                const bool coolPixel=pixel.blue()>pixel.red()+25;
                if(warmPixel)++warm;
                if(coolPixel&&(!(code==61||code==71)||y>=precipitationTop))++cool;
                if(!warmPixel&&!coolPixel)++neutral;
            }
            QCOMPARE(icon.accessibleName(),weatherCondition(code));
            if(code==0||code==2||code==95)QVERIFY2(warm>=8,"Sun and lightning need visible warm strokes at 3:1 contrast");
            if(code==2||code==3||code==45||code==61||code==71)QVERIFY2(cool>=8,"Clouds, rain and snow need visible blue strokes at 3:1 contrast");
            if(code==-1){QCOMPARE(warm,0);QCOMPARE(cool,0);QVERIFY(neutral>=8);}
        }
    }
    void diagnosticsKeepActiveIssuesWhenHistoryIsTrimmed(){
        Diagnostics diagnostics;diagnostics.fail(DataSource::Calendar,IssueCode::CalendarSignIn);
        for(int i=0;i<200;++i){diagnostics.fail(DataSource::Weather,IssueCode::WeatherUnavailable);diagnostics.recover(DataSource::Weather);}
        QCOMPARE(diagnostics.activeCount(),1);QVERIFY(diagnostics.hasActive(DataSource::Calendar));QVERIFY(diagnostics.snapshot()["issues"].toArray().size()<=50);
        diagnostics.clearResolved();QCOMPARE(diagnostics.snapshot()["issues"].toArray().size(),1);
        diagnostics.recover(DataSource::Calendar);diagnostics.clearResolved();QCOMPARE(diagnostics.activeCount(),0);QVERIFY(diagnostics.snapshot()["issues"].toArray().isEmpty());
    }
    void diagnosticExportDropsUnexpectedPrivateFields(){
        Diagnostics diagnostics;diagnostics.fail(DataSource::Calendar,IssueCode::CalendarUnavailable);auto state=diagnostics.snapshot();
        state["password"]="private-password";state["account"]="private-account";state["events"]=QJsonArray{"private-event"};
        state["sources"]=QJsonArray{QJsonObject{{"name","iCloud"},{"status","private-token"},{"checked","private-time"},{"account","private-account"}}};
        state["device"]=QJsonObject{{"availableMemoryMB",810},{"hostname","private-host"}};
        const auto safe=safeDiagnostics(state);const auto json=QJsonDocument(safe).toJson();QVERIFY(!json.contains("private-"));QVERIFY(!json.contains("password"));QVERIFY(!json.contains("events"));QCOMPARE(safe["active"].toInt(),1);
    }
    void repeatedFailuresDoNotFloodHistoryOrExposeRawData(){
        Diagnostics diagnostics;for(int i=0;i<100;++i)diagnostics.fail(DataSource::Summary,IssueCode::SummaryKey);
        const auto state=diagnostics.snapshot();QCOMPARE(state["issues"].toArray().size(),1);QCOMPARE(state["active"].toInt(),1);
        const auto issue=state["issues"].toArray().first().toObject();QCOMPARE(issue["message"].toString(),QString("OpenAI key was rejected"));
        const auto json=QJsonDocument(state).toJson();QVERIFY(!json.contains("password"));QVERIFY(!json.contains("sk-"));QVERIFY(!json.contains("account"));
    }
};
QTEST_MAIN(LayoutTests)
#include "layout-tests.moc"
