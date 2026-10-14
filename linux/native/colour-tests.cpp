#include "calendar-style.h"
#include "briefing-widget.h"
#include <QJsonObject>
#include <QLabel>
#include <QPixmap>
#include <QToolTip>
#include <QtTest>

namespace {
QJsonArray calendars(){return {QJsonObject{{"id","school"},{"colour","#E7A82F"},{"name","School"}},QJsonObject{{"id","family"},{"colour","#AF52DE"},{"name","Family"}}};}
QJsonArray segments(){return {QJsonObject{{"text","Photo day"},{"calendar_ids",QJsonArray{"school"}}},QJsonObject{{"text"," and "},{"calendar_ids",QJsonArray{}}},QJsonObject{{"text","a picnic"},{"calendar_ids",QJsonArray{"family"}}},QJsonObject{{"text",". Rain is forecast."},{"calendar_ids",QJsonArray{}}}};}
QString content(){return "Photo day and a picnic. Rain is forecast.";}
int pixels(const QImage &image,const QColor &colour){int count=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).rgba()==colour.rgba())++count;return count;}
}
class ColourTests:public QObject {
    Q_OBJECT
private slots:
    void appleRgbaAndInvalidColours(){
        const auto source=calendarSourceColour("#12345680");QVERIFY(source);
        QCOMPARE(source->red(),18);QCOMPARE(source->green(),52);QCOMPARE(source->blue(),86);QCOMPARE(source->alpha(),128);
        const auto mixed=compositeCalendarColour(*source,QColor("#ffffff"));QVERIFY(mixed.red()>source->red());QVERIFY(mixed.red()<255);QCOMPARE(mixed.alpha(),255);
        for(const auto &bad:{QString(),QString("red"),QString("#FFF"),QString("#112233; color:red"),QString("#GG2233"),QString("#123456789")})QVERIFY(!calendarSourceColour(bad));
    }
    void allSourceColoursKeepBothThemeContrasts(){
        for(const bool dark:{false,true}){
            const QColor background(dark?"#171d25":"#f6f5ef"),foreground(dark?"#e6ebe9":"#253c3c");
            for(const auto &source:{"#FFFFFF","#000000","#FFFF00","#FF0000","#00FF00","#0000FF","#AF52DE","#E7A82F","#FFFFFF00","#12345680","invalid"}){
                const auto tint=calendarPhraseTint(source,background,foreground);
                QVERIFY2(calendarContrast(foreground,tint)>=4.5,source);QCOMPARE(tint.alpha(),255);
            }
            QCOMPARE(calendarPhraseTint("#FFFFFF00",background,foreground),background);
            QCOMPARE(calendarPhraseTint("invalid",background,foreground),background);
        }
    }
    void sourceRangesKeepWeatherNeutralAndMixedSourcesUncoloured(){
        const auto formats=briefingFormats(content(),segments(),calendars(),QColor("#f6f5ef"),QColor("#253c3c"));
        QCOMPARE(formats.size(),2);QCOMPARE(formats[0].start,0);QCOMPARE(formats[0].length,9);QCOMPARE(formats[1].start,14);QCOMPARE(formats[1].length,8);
        auto mixed=segments();auto phrase=mixed[0].toObject();phrase["calendar_ids"]=QJsonArray{"school","family"};mixed[0]=phrase;
        QCOMPARE(briefingFormats(content(),mixed,calendars(),QColor("#f6f5ef"),QColor("#253c3c")).size(),1);
        const auto labels=briefingSourceDescription(content(),segments(),calendars());QVERIFY(labels.contains("Photo day — School"));QVERIFY(labels.contains("a picnic — Family"));QVERIFY(!labels.contains("Rain"));
    }
    void malformedMetadataAndSplitGraphemesBecomePlainText(){
        auto unknown=segments();auto first=unknown[0].toObject();first["calendar_ids"]=QJsonArray{"missing"};unknown[0]=first;
        QVERIFY(briefingFormats(content(),unknown,calendars(),Qt::white,Qt::black).isEmpty());
        QVERIFY(briefingFormats(content()+"wrong",segments(),calendars(),Qt::white,Qt::black).isEmpty());
        auto duplicate=calendars();duplicate.append(duplicate.first());QVERIFY(briefingFormats(content(),segments(),duplicate,Qt::white,Qt::black).isEmpty());
        const QString joined=QString::fromUtf8("á");const QJsonArray split{QJsonObject{{"text","a"},{"calendar_ids",QJsonArray{"school"}}},QJsonObject{{"text",joined.mid(1)},{"calendar_ids",QJsonArray{}}}};
        QVERIFY(briefingFormats(joined,split,calendars(),Qt::white,Qt::black).isEmpty());
    }
    void nativePaintAndSourceLabelsSurviveThemeAndSizeChanges(){
        BriefingWidget widget;widget.show();
        for(const bool dark:{false,true}){
            const QColor background(dark?"#171d25":"#f6f5ef"),foreground(dark?"#e6ebe9":"#253c3c");
            QPalette palette;palette.setColor(QPalette::Window,background);palette.setColor(QPalette::WindowText,foreground);widget.setPalette(palette);
            for(const int width:{620,744,300}){
                widget.resize(width,166);widget.updateBriefing({{"enabled",true},{"text",content()},{"segments",segments()},{"calendar_colours",calendars()}});QTest::qWait(10);
                QCOMPARE(widget.visibleText(),content());auto *body=widget.findChild<QLabel*>("briefingText");QVERIFY(body);
                QVERIFY(body->accessibleDescription().contains("School"));QVERIFY(body->toolTip().contains("Family"));
                QPixmap pixmap(widget.size());widget.render(&pixmap);const auto image=pixmap.toImage();
                QVERIFY(pixels(image,calendarPhraseTint("#E7A82F",background,foreground))>100);
                QVERIFY(pixels(image,calendarPhraseTint("#AF52DE",background,foreground))>100);
            }
            widget.updateBriefing({{"enabled",true},{"text",content()}});QPixmap plain(widget.size());widget.render(&plain);
            QCOMPARE(pixels(plain.toImage(),calendarPhraseTint("#E7A82F",background,foreground)),0);
            QVERIFY(widget.findChild<QLabel*>("briefingText")->accessibleDescription().isEmpty());
        }
    }
    void tappingBriefingShowsEscapedSourceLabels(){
        BriefingWidget widget;widget.resize(620,166);widget.show();
        const QString literal="School <b>photo</b> day.";
        const QJsonArray parts{QJsonObject{{"text",literal},{"calendar_ids",QJsonArray{"school"}}}};
        widget.updateBriefing({{"enabled",true},{"text",literal},{"segments",parts},{"calendar_colours",calendars()}});
        QTest::qWait(10);auto *body=widget.findChild<QLabel*>("briefingText");QVERIFY(body);
        QCOMPARE(widget.visibleText(),literal);QVERIFY(body->accessibleDescription().contains(literal));
        QVERIFY(body->toolTip().contains("&lt;b&gt;"));QTest::mouseClick(body,Qt::LeftButton);
        QVERIFY(QToolTip::text().contains("School"));QVERIFY(QToolTip::text().contains("&lt;b&gt;"));QToolTip::hideText();
    }
};
QTEST_MAIN(ColourTests)
#include "colour-tests.moc"
