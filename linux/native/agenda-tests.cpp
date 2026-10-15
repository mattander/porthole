#include "calendar-client.h"
#include "dashboard-widgets.h"
#include "event-widgets.h"
#include "profile.h"
#include "rain-forecast.h"
#include "typography.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSharedPointer>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTimer>
#include <QWindow>
#include <QVBoxLayout>
#include <QtTest>

namespace {
QDateTime instant(){return QDateTime(QDate(2026,10,15),QTime(14,30),QTimeZone::utc());}
QJsonObject forecast(QDateTime at=instant(),double currentRain=.25,double currentShowers=0){
    const auto first=at.toSecsSinceEpoch()/3600*3600;QJsonArray times,amounts,showers;
    for(int index=0;index<8;++index){times.append(first+3600*index);amounts.append(index*.25);showers.append(0);}
    return {{"current_units",QJsonObject{{"time","unixtime"},{"interval","seconds"},{"rain","mm"},{"showers","mm"}}},{"current",QJsonObject{{"time",at.toSecsSinceEpoch()/900*900},{"interval",900},{"rain",currentRain},{"showers",currentShowers}}},{"hourly_units",QJsonObject{{"time","unixtime"},{"rain","mm"},{"showers","mm"}}},{"hourly",QJsonObject{{"time",times},{"rain",amounts},{"showers",showers}}}};
}
Event sample(){return {"sample","Sample: library visit <b>📚</b>","Example community library","Family",instant(),instant().addSecs(3600),false,QString(64,'a'),"#3388FF","Return the books.\nMeet upstairs."};}
}
class AgendaTests:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QVERIFY(configureInterfaceTypography());}
    void init(){
        const auto parsed=HouseholdProfile::parse({{"version",1},{"timezone","America/Toronto"},{"familyContext",""},{"weather",QJsonValue::Null}});
        if(const auto *profile=std::get_if<HouseholdProfile>(&parsed))setHouseholdProfile(*profile);else QFAIL("Invalid fixture");
    }
    void dayHeadingsAreLargeTappableAndKeepTheirSelection(){
        AgendaDays days;days.resize(480,70);days.show();QSignalSpy changes(&days,&AgendaDays::viewRequested);
        const auto buttons=days.findChildren<QPushButton*>();QCOMPARE(buttons.size(),2);
        for(auto *button:buttons){QVERIFY(button->height()>=56);QVERIFY(!button->accessibleName().isEmpty());}
        QCOMPARE(buttons[0]->text(),QString("Today"));QVERIFY(buttons[0]->isChecked());
        QTest::mouseClick(buttons[1],Qt::LeftButton);QCOMPARE(changes.count(),1);QCOMPARE(changes[0][0].toInt(),1);QVERIFY(buttons[1]->isChecked());QVERIFY(!buttons[0]->isChecked());
        QTest::mouseClick(buttons[1],Qt::LeftButton);QVERIFY(buttons[1]->isChecked());
        buttons[0]->setFocus();QTest::keyClick(buttons[0],Qt::Key_Space);QVERIFY(buttons[0]->isChecked());QVERIFY(!buttons[1]->isChecked());
        days.setView(2);QVERIFY(!buttons[0]->isChecked());QVERIFY(!buttons[1]->isChecked());
    }
    void amountsUseThePrecedingHourAndRollForward(){
        const auto parsed=RainForecast::parse(forecast(),instant());QVERIFY(parsed);QVERIFY(parsed->raining(instant()));QCOMPARE(parsed->currentRate(),1.0);
        const auto next=parsed->nextHours(instant());QCOMPARE(next.size(),6);QCOMPARE(next.first().end,instant().addSecs(1800));QCOMPARE(next.first().amountMm,.25);QCOMPARE(next.last().amountMm,1.5);
        QCOMPARE(parsed->nextHours(instant().addSecs(3600)).first().amountMm,.5);
        QVERIFY(parsed->nextHours(instant().addSecs(3601)).isEmpty());QVERIFY(parsed->nextHours(instant().addSecs(-61)).isEmpty());
        QVERIFY(!parsed->raining(instant().addSecs(901)));QVERIFY(!parsed->raining(instant().addSecs(-61)));
        for(const auto at:{QDateTime(QDate(2026,3,8),QTime(6,30),QTimeZone::utc()),QDateTime(QDate(2026,11,1),QTime(5,30),QTimeZone::utc())}){
            const auto transition=RainForecast::parse(forecast(at),at);QVERIFY(transition);const auto hours=transition->nextHours(at);QCOMPARE(hours.size(),6);
            for(int index=1;index<hours.size();++index)QCOMPARE(hours[index-1].end.secsTo(hours[index].end),3600LL);
            RainStrip strip;strip.setForecast(*transition,at);QVERIFY(strip.accessibleDescription().contains("mm per hour"));QVERIFY(!strip.accessibleDescription().contains('%'));
        }
    }
    void invalidAmountsAndCurrentConditionsNeverBecomeARainReport(){
        QVERIFY(!RainForecast::parse({},instant()));
        for(const QJsonValue bad:{QJsonValue(QJsonValue::Null),QJsonValue(-1),QJsonValue(1001),QJsonValue("0.2")}){
            for(const auto name:{"rain","showers"}){
                auto value=forecast();auto hourly=value["hourly"].toObject();auto amounts=hourly[name].toArray();amounts[1]=bad;hourly[name]=amounts;value["hourly"]=hourly;QVERIFY(!RainForecast::parse(value,instant()));
                value=forecast();auto current=value["current"].toObject();current[name]=bad;value["current"]=current;QVERIFY(!RainForecast::parse(value,instant()));
            }
        }
        auto value=forecast();auto hourly=value["hourly"].toObject();auto times=hourly["time"].toArray();times[2]=times[1];hourly["time"]=times;value["hourly"]=hourly;QVERIFY(!RainForecast::parse(value,instant()));
        for(const auto key:{"hourly_units","current_units"}){value=forecast();auto units=value[key].toObject();units["rain"]="inch";value[key]=units;QVERIFY(!RainForecast::parse(value,instant()));}
        value=forecast();auto current=value["current"].toObject();current["interval"]=3600;value["current"]=current;QVERIFY(!RainForecast::parse(value,instant()));
        value=forecast();current=value["current"].toObject();current["time"]=instant().addSecs(-1201).toSecsSinceEpoch();value["current"]=current;QVERIFY(!RainForecast::parse(value,instant()));
    }
    void rainCurveIsContinuousAndOnlyVisibleForFreshCurrentRain(){
        QWidget host;auto *layout=new QVBoxLayout(&host);auto *strip=new RainStrip;layout->addWidget(strip);auto *after=new QLabel("Forecast");layout->addWidget(after);host.resize(680,180);host.show();
        QTest::qWait(10);QVERIFY(strip->isHidden());const auto dryTop=after->y();
        const auto dry=RainForecast::parse(forecast(instant(),0),instant());QVERIFY(dry);QVERIFY(!dry->raining(instant()));strip->setForecast(*dry,instant());QVERIFY(strip->isHidden());
        const auto wet=RainForecast::parse(forecast(),instant());QVERIFY(wet);
        for(bool dark:{false,true}){
            QPalette palette;palette.setColor(QPalette::Window,QColor(dark?"#171d25":"#f6f5ef"));palette.setColor(QPalette::WindowText,QColor(dark?"#e6ebe9":"#253c3c"));strip->setPalette(palette);strip->setAutoFillBackground(true);strip->setForecast(*wet,instant());QTest::qWait(10);
            QVERIFY(strip->isVisible());QVERIFY(after->y()>dryTop);QVERIFY(!strip->accessibleDescription().contains('%'));
            const auto image=strip->grab().toImage();
            for(int x=29;x<image.width()-28;++x){bool ink=false;for(int y=5;y<=52;++y){const auto colour=image.pixelColor(x,y);if(colour.blue()>colour.red()+40){ink=true;break;}}QVERIFY2(ink,"Rain curve has a visible gap");}
        }
        strip->setTime(instant().addSecs(960));QVERIFY(strip->isHidden());
        const auto showers=RainForecast::parse(forecast(instant(),0,.1),instant());QVERIFY(showers);strip->setForecast(*showers,instant());QVERIFY(strip->isVisible());
        strip->clear();QVERIFY(strip->isHidden());QTest::qWait(10);QCOMPARE(after->y(),dryTop);
        strip->setForecast(*wet,instant());strip->setForecast(*dry,instant());QVERIFY(strip->isHidden());
    }
    void descriptionIsBoundedOnDemandAndAbsentFromStatus(){
        auto event=sample();const auto status=calendarEventJson(event);QVERIFY(!status.contains("description"));QVERIFY(!QJsonDocument(status).toJson().contains("Meet upstairs"));
        const auto reference=status["detailsRef"].toString();QCOMPARE(reference.size(),64);const auto details=calendarEventDetailsJson({event},reference);QVERIFY(details["ok"].toBool());
        const auto parsed=calendarEventFromJson(details["event"].toObject());QVERIFY(parsed);QCOMPARE(parsed->description,event.description);
        QVERIFY(!calendarEventDetailsJson({event},QString(64,'b'))["ok"].toBool());QVERIFY(!calendarEventDetailsJson({event},"../secret")["ok"].toBool());
        auto bad=details["event"].toObject();bad["description"]=QString(4097,'x');QVERIFY(!calendarEventFromJson(bad));bad=details["event"].toObject();bad["detailsRef"]="bad";QVERIFY(!calendarEventFromJson(bad));
        const QString emoji="👨‍👩‍👧‍👦";const auto bounded=boundedEventDescription(QString(4092,'x')+emoji);QVERIFY(bounded.size()<=4096);QVERIFY(!bounded.contains(QChar::ReplacementCharacter));QVERIFY(bounded.endsWith(QChar(0x2026)));QVERIFY(!bounded.contains(QChar(0x200d)));
        event.uid=QString(300,'x');auto other=event;other.uid+='y';QVERIFY(calendarEventReference(event)!=calendarEventReference(other));
    }
    void calendarNotesDecodeLineBreaks(){
        const auto events=parseExpandedCalendar("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:notes\r\nDTSTART:20261015T140000Z\r\nDTEND:20261015T150000Z\r\nSUMMARY:Library\r\nDESCRIPTION:Bring books\\nMeet upstairs\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n","Family");
        QCOMPARE(events.size(),1);QCOMPARE(events.first().description,QString("Bring books\nMeet upstairs"));
    }
    void cardTextAndBlankAreaBothOpenDetails(){
        EventCard card(sample());card.resize(600,160);card.show();QTest::qWait(20);QSignalSpy clicks(&card,&QPushButton::clicked);
        auto *title=card.findChild<QLabel*>("eventTitle");QVERIFY(title);QCOMPARE(title->textFormat(),Qt::PlainText);QVERIFY(title->testAttribute(Qt::WA_TransparentForMouseEvents));
        QTest::mouseClick(card.windowHandle(),Qt::LeftButton,Qt::NoModifier,title->mapTo(&card,title->rect().center()));QCOMPARE(clicks.count(),1);
        QTest::mouseClick(&card,Qt::LeftButton,Qt::NoModifier,QPoint(card.width()-4,card.height()/2));QCOMPARE(clicks.count(),2);
        card.setFocus();QTest::keyClick(&card,Qt::Key_Space);QCOMPARE(clicks.count(),3);
        card.setEnabled(false);QTest::mouseClick(&card,Qt::LeftButton);QCOMPARE(clicks.count(),3);
    }
    void detailsPreserveLiteralTextAndScrollLongNotes(){
        auto event=sample();event.description=QString("Sample notes 📚 <b>literal</b>\n").repeated(90);
        EventDetails dialog(event);dialog.setDetails(event);dialog.resize(700,600);dialog.show();QTest::qWait(20);
        auto *title=dialog.findChild<QLabel*>("detailTitle");auto *notes=dialog.findChild<QLabel*>("detailNotes");auto *close=dialog.findChild<QPushButton*>("closeEventDetails");auto *scroll=dialog.findChild<QScrollArea*>();QVERIFY(title&&notes&&close&&scroll);
        QCOMPARE(title->text(),event.title);QCOMPARE(title->textFormat(),Qt::PlainText);QCOMPARE(notes->text(),event.description);QCOMPARE(notes->textFormat(),Qt::PlainText);
        QVERIFY(scroll->verticalScrollBar()->maximum()>0);QVERIFY(close->height()>=56);QVERIFY(dialog.rect().contains(close->geometry()));
        event.description=QString(1000,'W');dialog.setDetails(event);QTest::qWait(20);QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);QVERIFY(notes->heightForWidth(notes->width())<=notes->height());
        dialog.setDetails(std::nullopt);QVERIFY(notes->text().contains("unavailable"));event.description.clear();dialog.setDetails(event);QCOMPARE(notes->text(),QString("No notes provided."));
        QTest::mouseClick(close,Qt::LeftButton);QVERIFY(!dialog.isVisible());
        event.allDay=true;event.start=QDateTime(QDate(2026,10,15),QTime(0,0),configuredZone());event.end=event.start.addDays(3);QVERIFY(eventDateRange(event).contains("Oct 17"));QVERIFY(!eventDateRange(event).contains("Oct 18"));
    }
    void detailRequestIsAsyncAndCompletesOnce(){
        QTemporaryDir directory;QLocalServer server;const auto path=directory.filePath("socket");QVERIFY(server.listen(path));auto event=sample();event.detailsReference=calendarEventReference(event);bool fail=false;
        connect(&server,&QLocalServer::newConnection,this,[&]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();auto bytes=QSharedPointer<QByteArray>::create();connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QLocalSocket::readyRead,socket,[&,socket,bytes]{
                *bytes+=socket->readAll();if(!bytes->contains('\n'))return;const auto request=QJsonDocument::fromJson(*bytes).object();bytes->clear();
                if(request["type"].toString()=="eventDetails"){
                    if(fail){socket->disconnectFromServer();return;}
                    const auto reply=calendarEventDetailsJson({event},request["reference"].toString());QTimer::singleShot(50,socket,[socket,reply]{socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();});
                }else {socket->write("{\"ok\":true,\"status\":\"Fixture\",\"instance\":\"fixture\",\"eventsRevision\":0}\n");socket->disconnectFromServer();}
            });
        }});
        CalendarClient client(nullptr,false,path);int completed=0;bool responsive=false;std::optional<Event> result;
        client.eventDetails(event,[&](std::optional<Event> value){++completed;result=value;});QTimer::singleShot(0,&client,[&]{responsive=true;});QCOMPARE(completed,0);QTRY_COMPARE(completed,1);QVERIFY(responsive);QVERIFY(result);QCOMPARE(result->description,event.description);
        fail=true;client.eventDetails(event,[&](std::optional<Event> value){++completed;result=value;});QTRY_COMPARE(completed,2);QVERIFY(!result);QTest::qWait(30);QCOMPARE(completed,2);
    }
};
QTEST_MAIN(AgendaTests)
#include "agenda-tests.moc"
