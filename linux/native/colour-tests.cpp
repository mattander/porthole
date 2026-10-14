#include "calendar-style.h"
#include "briefing-widget.h"
#include <QJsonObject>
#include <QLabel>
#include <QPixmap>
#include <QToolTip>
#include <QScrollArea>
#include <QScrollBar>
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
    void calendarKeyUsesOnlyValidTintedSourcesInPhraseOrder(){
        auto sources=calendars();sources.append(QJsonObject{{"id","unused"},{"name","Not referenced"},{"colour","#112233"}});
        auto parts=segments();parts.append(QJsonObject{{"text"," Photo day again."},{"calendar_ids",QJsonArray{"school"}}});
        const auto keys=briefingCalendarKey(content()+" Photo day again.",parts,sources);
        QCOMPARE(keys.size(),2);QCOMPARE(keys[0].name,QString("School"));QCOMPARE(keys[1].name,QString("Family"));QCOMPARE(keys[0].colour,QColor("#E7A82F"));
        auto first=parts[0].toObject();first["calendar_ids"]=QJsonArray{"school","family"};parts[0]=first;
        const auto mixed=briefingCalendarKey(content()+" Photo day again.",parts,sources);QCOMPARE(mixed.size(),2);QCOMPARE(mixed[0].name,QString("Family"));
        first["calendar_ids"]=QJsonArray{"unknown"};parts[0]=first;QVERIFY(briefingCalendarKey(content()+" Photo day again.",parts,sources).isEmpty());
        QVERIFY(briefingCalendarKey(content()+"changed",segments(),sources).isEmpty());
        auto transparent=sources[0].toObject();transparent["colour"]="#11223300";sources[0]=transparent;
        const auto remaining=briefingCalendarKey(content(),segments(),sources);QCOMPARE(remaining.size(),1);QCOMPARE(remaining[0].name,QString("Family"));
    }
    void calendarKeySitsBelowTextAndClearsWithPlainBriefings(){
        BriefingWidget widget;widget.show();
        auto sources=calendars();auto named=sources[0].toObject();named["name"]="<b>School & lunch</b>";sources[0]=named;
        for(bool dark:{false,true})for(int width:{620,744,300,150}){
            QPalette palette;palette.setColor(QPalette::Window,QColor(dark?"#171d25":"#f6f5ef"));palette.setColor(QPalette::WindowText,QColor(dark?"#e6ebe9":"#253c3c"));widget.setPalette(palette);
            widget.resize(width,166);widget.updateBriefing({{"enabled",true},{"text",content()},{"segments",segments()},{"calendar_colours",sources}});QTest::qWait(10);
            auto *key=widget.findChild<QLabel*>("briefingCalendarKey");auto *body=widget.findChild<QLabel*>("briefingText");auto *area=widget.findChild<QWidget*>("briefingKeyArea");QVERIFY(key);QVERIFY(body);QVERIFY(area);
            QVERIFY(area->isVisible());QCOMPARE(key->textFormat(),Qt::PlainText);QVERIFY(key->text().contains("<b>School & lunch</b>"));QVERIFY(key->accessibleDescription().contains("Family"));
            const auto keyRect=QRect(area->mapTo(&widget,QPoint()),area->size());const auto bodyRect=QRect(body->mapTo(&widget,QPoint()),body->size());
            QVERIFY(widget.rect().contains(keyRect));QVERIFY(widget.rect().contains(bodyRect));QVERIFY(keyRect.top()>bodyRect.bottom());QVERIFY(keyRect.top()-bodyRect.bottom()<=9);
            widget.updateBriefing({{"enabled",true},{"text",content()}});QVERIFY(!area->isVisible());QVERIFY(key->text().isEmpty());QVERIFY(key->accessibleDescription().isEmpty());
        }
    }
    void manyLongCalendarNamesRemainReachableWithoutGrowingThePanel(){
        QJsonArray parts,sources;QString copy;
        for(int i=0;i<12;++i){
            const auto id=QString::number(i),phrase=QString("Plan %1. ").arg(i);
            copy+=phrase;parts.append(QJsonObject{{"text",phrase},{"calendar_ids",QJsonArray{id}}});
            sources.append(QJsonObject{{"id",id},{"name",QString("A long household calendar name %1").arg(i)},{"colour","#AF52DE"}});
        }
        BriefingWidget widget;widget.resize(300,166);widget.show();widget.updateBriefing({{"enabled",true},{"text",copy},{"segments",parts},{"calendar_colours",sources}});QTest::qWait(10);
        auto *area=widget.findChild<QScrollArea*>("briefingKeyArea");auto *key=widget.findChild<QLabel*>("briefingCalendarKey");QVERIFY(area);QVERIFY(key);
        QCOMPARE(widget.height(),166);QVERIFY(area->height()<=48);QVERIFY(key->height()>area->height());QVERIFY(key->accessibleDescription().contains("calendar name 11"));
        QVERIFY(area->verticalScrollBar()->maximum()>0);area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());QVERIFY(area->verticalScrollBar()->value()>0);QCOMPARE(widget.visibleText(),copy);
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
