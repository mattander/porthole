#include "profile.h"
#include "calendar-client.h"
#include "keyboard.h"
#include "settings.h"
#include "briefing-widget.h"
#include "dashboard-widgets.h"
#include "diagnostics.h"
#include <QTabWidget>
#include "wifi.h"
#include <QApplication>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFrame>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QPushButton>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScroller>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QSslSocket>
#include <QTextStream>
#include <QTextCharFormat>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QDateTime now() { return QDateTime::currentDateTimeUtc().toTimeZone(configuredZone()); }
bool online() {
    for (const auto &i : QNetworkInterface::allInterfaces())
        if (i.flags().testFlag(QNetworkInterface::IsUp) &&
            !i.flags().testFlag(QNetworkInterface::IsLoopBack) && !i.addressEntries().isEmpty()) return true;
    return false;
}
QLabel *label(QString text, int size, QWidget *parent=nullptr) {
    auto *l=new QLabel(text,parent); l->setTextFormat(Qt::PlainText);
    QFont font("DejaVu Sans");font.setPixelSize(size);l->setFont(font);
    l->setWordWrap(true);return l;
}


}

class Dashboard : public QWidget {
    CalendarClient caldav{this,QApplication::arguments().contains("--demo")};
    QNetworkAccessManager weatherNetwork{this};
    WifiController wifi{this,QApplication::arguments().contains("--preview-settings")};
    QPointer<SettingsPane> settings;
    BriefingWidget *briefing=new BriefingWidget;
    QJsonObject latestBriefing;
    QList<Calendar> selected;
    QList<Event> events;
    QDate selectedDate=now().date();
    QDate month=selectedDate;
    bool demo=false;
    bool darkTheme=false;
    int view=0;
    QString calendarStatus="iCloud is not connected";
    QString weatherStatus="Weather is not connected";
    ClockFace *clock=new ClockFace;
    QLabel *date=label("",30);
    WeatherIcon *currentIcon=new WeatherIcon(64);
    DashboardFooter *footer=new DashboardFooter;
    Diagnostics diagnostics;
    bool weatherStale=false,hadWifi=false;
    QDateTime lastCalendar,lastSummary;
    QLabel *weatherLocation=label("Weather location",21);
    QLabel *temperature=label("—",91), *weather=label("Weather is not connected",18),*feels=label("",15),*sunTimes=label("",14);
    ForecastStrip *forecast=new ForecastStrip;
    QLabel *heading=label("Today",40), *caption=label("",17);
    QWidget *conditions=new QWidget,*agenda=new QWidget,*eventList=new QWidget;
    QBoxLayout *board=new QHBoxLayout;
    QVBoxLayout *eventLayout=new QVBoxLayout(eventList);
    QCalendarWidget *calendar=new QCalendarWidget;
    QDateTime lastWeather;
    QDate sunDate;
    quint64 weatherGeneration=0;
public:
    explicit Dashboard(bool sample) {
        setWindowTitle("Porthole");setMinimumSize(640,600);
        caption->setProperty("tone","secondary");feels->setProperty("tone","secondary");sunTimes->setProperty("tone","secondary");
        auto *root=new QVBoxLayout(this);root->setContentsMargins(32,30,32,18);root->setSpacing(20);
        auto *left=new QVBoxLayout(conditions);left->setContentsMargins(0,0,0,0);left->setSpacing(14);
        left->addWidget(clock);left->addWidget(date);left->addSpacing(9);
        auto *divider=new QFrame;divider->setFrameShape(QFrame::HLine);divider->setFixedHeight(1);left->addWidget(divider);left->addSpacing(5);
        auto *current=new QHBoxLayout;current->setSpacing(18);current->addWidget(currentIcon);current->addWidget(temperature);
        auto *weatherCopy=new QVBoxLayout;weatherCopy->setSpacing(5);weatherCopy->addWidget(weatherLocation);weatherCopy->addWidget(weather);weatherCopy->addWidget(feels);weatherCopy->addWidget(sunTimes);current->addLayout(weatherCopy,1);left->addLayout(current);
        left->addSpacing(7);left->addWidget(forecast);left->addSpacing(30);left->addWidget(briefing);left->addStretch();
        auto *right=new QVBoxLayout(agenda);right->setContentsMargins(0,8,0,0);right->setSpacing(14);right->addWidget(heading);right->addWidget(caption);
        auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setWidget(eventList);
        QScroller::grabGesture(scroll->viewport(),QScroller::TouchGesture);right->addWidget(scroll,1);
        eventLayout->setContentsMargins(0,0,8,0);eventLayout->setSpacing(26);eventLayout->setAlignment(Qt::AlignTop);
        calendar->setFirstDayOfWeek(Qt::Sunday);calendar->setGridVisible(false);
        calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        calendar->setMinimumDate(QDate(2000,1,1));calendar->setMaximumDate(QDate(2099,12,31));
        calendar->setSelectedDate(selectedDate);
        calendar->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);calendar->hide();
        connect(calendar,&QCalendarWidget::selectionChanged,this,[this] {
            selectedDate=calendar->selectedDate();renderAgenda();
        });
        connect(calendar,&QCalendarWidget::currentPageChanged,this,[this](int y,int m) {
            month=QDate(y,m,1);if(!demo&&!selected.isEmpty()){calendarStatus="Refreshing calendar…";caldav.fetch(selected,month);renderAgenda();}
        });
        board->setContentsMargins(0,0,0,0);board->setSpacing(48);
        board->addWidget(conditions,55);board->addWidget(calendar,55);board->addWidget(agenda,45);
        root->addLayout(board,1);
        root->addWidget(footer);
        connect(footer,&DashboardFooter::viewRequested,this,&Dashboard::changeView);
        connect(footer,&DashboardFooter::settingsRequested,this,&Dashboard::showSettings);
        connect(&caldav,&CalendarClient::profileChanged,this,[this](const QJsonObject &){
            if(demo)return;
            ++weatherGeneration;for(auto *reply:weatherNetwork.findChildren<QNetworkReply*>())reply->abort();lastWeather={};sunDate={};weatherStale=false;
            weatherLocation->setText(householdProfile().weather()?householdProfile().weather()->label():"Weather location");temperature->setText("—");weather->setText("Updating configured weather…");feels->clear();sunTimes->clear();currentIcon->setCode(-1);forecast->clear();
            events.clear();selectedDate=now().date();month=selectedDate;calendar->setSelectedDate(selectedDate);updateClock();caldav.fetch({},month);refreshWeather();syncSettings();
        });
        connect(&caldav,&CalendarClient::profileFailed,this,[this](const QString &message){QWidget *parent=this;if(settings)parent=settings.data();QMessageBox::warning(parent,"Settings not saved",message);});
        connect(&caldav,&CalendarClient::failed,this,[this](const QString &message){calendarStatus=message;diagnostics.fail(DataSource::Calendar,IssueCode::CalendarUnavailable);renderAgenda();});
        connect(&caldav,&CalendarClient::statusChanged,this,[this](const QString &message){if(!demo){calendarStatus=message;if(message.startsWith("Calendar service")||message.startsWith("Calendar response"))diagnostics.fail(DataSource::Calendar,IssueCode::CalendarUnavailable);renderStatus();}});
        connect(&caldav,&CalendarClient::healthChanged,this,[this](QDateTime checked,const QString &issue,bool authenticated){
            if(demo)return;
            lastCalendar=checked;
            if(!issue.isEmpty())diagnostics.fail(DataSource::Calendar,issue=="sign-in"?IssueCode::CalendarSignIn:IssueCode::CalendarUnavailable);
            else if(authenticated&&checked.isValid())diagnostics.recover(DataSource::Calendar);
            renderStatus();
        });
        connect(&caldav,&CalendarClient::selectedChanged,this,[this](const QList<Calendar> &items){if(demo)return;bool changed=items.size()!=selected.size();if(!changed)for(int i=0;i<items.size();++i)if(items[i].url!=selected[i].url||items[i].name!=selected[i].name)changed=true;if(changed){selected=items;renderAgenda();refreshCalendar();}});
        connect(&caldav,&CalendarClient::calendarsReady,this,[this](const QList<Calendar> &calendars){chooseCalendars(calendars);});
        connect(&caldav,&CalendarClient::eventsReady,this,[this](const QList<Event> &loaded,QDate loadedMonth){
            const auto wanted=view==2?month:now().date().addDays(view);
            if(loadedMonth!=QDate(wanted.year(),wanted.month(),1))return;
            if(demo)return;
            events=loaded;renderAgenda();markDates();
        });
        connect(&caldav,&CalendarClient::briefingChanged,this,[this](const QJsonObject &state){latestBriefing=state;if(!demo){
            const auto generated=QDateTime::fromString(state["generated"].toString(),Qt::ISODate);
            if(generated.isValid()&&generated!=lastSummary){lastSummary=generated;diagnostics.recover(DataSource::Summary);}
            const auto status=state["status"].toString();
            if(status.startsWith("API key rejected"))diagnostics.fail(DataSource::Summary,IssueCode::SummaryKey);
            else if(status.contains("spending record")||status.contains("storage")||status.contains("removal failed")||status.contains("could not be removed"))diagnostics.fail(DataSource::Summary,IssueCode::SummaryStorage);
            else if(status.startsWith("AI unavailable")||status.startsWith("AI reply could not"))diagnostics.fail(DataSource::Summary,IssueCode::SummaryUnavailable);
            if(!state["enabled"].toBool())diagnostics.recover(DataSource::Summary);
            briefing->updateBriefing(state);
        }renderStatus();});
        connect(&wifi,&WifiController::changed,this,[this]{
            if(demo)return;
            if(wifi.status().startsWith("Connection failed"))diagnostics.fail(DataSource::Wifi,IssueCode::WifiConnection);
            else if(wifi.status().startsWith("Wi-Fi scan failed"))diagnostics.fail(DataSource::Wifi,IssueCode::WifiScan);
            else if(wifi.connected())diagnostics.recover(DataSource::Wifi);
            else if(hadWifi)diagnostics.fail(DataSource::Wifi,IssueCode::ConnectionLost);
            hadWifi=wifi.connected();renderStatus();
        });
        connect(&wifi,&WifiController::connectionReady,this,[this]{if(demo)useLiveData();else refreshData();});
        auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{updateClock();});timer->start(1000);
        auto *calendarTimer=new QTimer(this);connect(calendarTimer,&QTimer::timeout,this,[this]{if(!demo)caldav.requestStatus();});calendarTimer->start(60000);
        auto *weatherTimer=new QTimer(this);connect(weatherTimer,&QTimer::timeout,this,[this]{if(!demo)refreshWeather();});weatherTimer->start(15*60*1000);
        QSettings preferences(preferencesPath(),QSettings::IniFormat);
        bool dark=preferences.value("display/darkMode",false).toBool();
        if(qApp->arguments().contains("--dark"))dark=true;
        else if(qApp->arguments().contains("--light"))dark=false;
        applyTheme(dark);
        if(sample){sampleData();if(qApp->arguments().contains("--briefing-stress"))briefing->updateBriefing({{"enabled",true},{"text",QString(400,'W')},{"status","Preview"}});
            if(qApp->arguments().contains("--briefing-long"))briefing->updateBriefing({{"enabled",true},{"text","Sample: There is a special lunch day tomorrow and a school event at 10 AM, so check the calendar for any items to bring. Saturday’s morning plans overlap; check who is handling each commitment. Tomorrow’s forecast is cool with rain, which may affect outdoor plans and clothing choices. Sunday’s family visit starts at noon; consider any preparation listed in the event before leaving."},{"status","Preview"}});}else{updateClock();changeView(0);renderStatus();QTimer::singleShot(0,this,[this]{refreshWeather();});}
    }
protected:
    void resizeEvent(QResizeEvent *e) override {
        QWidget::resizeEvent(e);const bool portrait=width()<960;
        board->setDirection(portrait?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
        clock->setPortrait(portrait);
        auto f=date->font();f.setPixelSize(portrait?27:30);date->setFont(f);
        conditions->layout()->setContentsMargins(0,0,0,0);
        agenda->layout()->setContentsMargins(0,portrait?0:8,0,0);
        board->setStretchFactor(conditions,portrait?0:55);
        board->setStretchFactor(calendar,portrait?1:55);
        board->setStretchFactor(agenda,portrait?1:45);
    }
private:
    QJsonObject diagnosticState()const {
        const auto time=[](QDateTime value){return value.isValid()?value.toString(Qt::ISODate):QString("—");};
        auto value=diagnostics.snapshot();value["device"]=demo?QJsonObject{{"availableMemoryMB",810},{"uptimeSeconds",6240}}:deviceHealth();value["spentMicros"]=latestBriefing["spentMicros"];
        QJsonArray sources;
        sources.append(QJsonObject{{"name","Wi-Fi"},{"status",demo?"Sample":wifi.connected()?"Connected":"Offline"},{"checked",demo?"Sample":"Current state"}});
        sources.append(QJsonObject{{"name","iCloud"},{"status",demo?"Sample":!caldav.hasAccount()?"Not connected":diagnostics.hasActive(DataSource::Calendar)?"Needs attention":lastCalendar.isValid()?"Last calendar available":"Connecting"},{"checked",time(lastCalendar)}});
        sources.append(QJsonObject{{"name","Weather"},{"status",demo?"Sample":weatherStale?"Stale · retry pending":lastWeather.isValid()?"Up to date":"Waiting"},{"checked",time(lastWeather)}});
        sources.append(QJsonObject{{"name","AI briefing"},{"status",demo?"Sample":!latestBriefing["enabled"].toBool()?"Off":diagnostics.hasActive(DataSource::Summary)?"Needs attention":lastSummary.isValid()?"Last result available":"Waiting"},{"checked",time(lastSummary)}});
        value["sources"]=sources;return value;
    }
    void syncSettings() {
        if(!settings)return;
        QStringList names;for(const auto &calendar:selected)names<<calendar.name;
        settings->updateProfile(householdProfile().json());settings->updateBriefing(latestBriefing);
        settings->updateDiagnostics(diagnosticState());
        settings->updateState(calendarStatus,weatherStatus,names,darkTheme,demo,caldav.hasAccount(),!caldav.availableCalendars().isEmpty());
    }
    void showSettings() {
        if(settings){settings->raise();return;}
        SettingsActions actions;actions.profile=[this](QJsonObject value){caldav.saveProfile(value);};
        actions.signIn=[this]{signIn();};
        actions.signOut=[this]{if(QMessageBox::question(settings?static_cast<QWidget*>(settings.data()):this,"Forget iCloud account","Remove the saved sign-in and calendar selection from this frame? Wi-Fi remains saved.")==QMessageBox::Yes){caldav.forgetAccount();selected.clear();events.clear();calendarStatus="Forgetting iCloud account…";renderAgenda();markDates();}};
        actions.changeCalendars=[this]{chooseCalendars(caldav.availableCalendars());};
        actions.useLive=[this]{useLiveData();};actions.refresh=[this]{useLiveData();};actions.useSample=[this]{sampleData();};
        actions.theme=[this](bool dark){applyTheme(dark);const auto path=preferencesPath();QDir().mkpath(QFileInfo(path).absolutePath());QSettings preferences(path,QSettings::IniFormat);preferences.setValue("display/darkMode",dark);preferences.sync();syncSettings();};
        actions.briefing=[this](bool enabled,QString priorities,int cents){caldav.briefingSettings(enabled,priorities,cents);};
        actions.refreshBriefing=[this]{caldav.refreshBriefing();};
        actions.forgetBriefingKey=[this]{if(QMessageBox::question(settings?static_cast<QWidget*>(settings.data()):this,"Forget API key","Remove the saved OpenAI key and briefing from this frame? Your iCloud and Wi-Fi connections stay saved.")==QMessageBox::Yes)caldav.forgetBriefingKey();};
        actions.clearDiagnosticHistory=[this]{diagnostics.clearResolved();renderStatus();};
        SettingsPane pane(&wifi,actions,this);settings=&pane;syncSettings();pane.exec();settings=nullptr;
    }
    void useLiveData() {
        if(demo){demo=false;events.clear();lastCalendar=caldav.lastChecked();lastWeather={};lastSummary=QDateTime::fromString(latestBriefing["generated"].toString(),Qt::ISODate);weatherStale=false;calendarStatus="iCloud is not connected";temperature->setText("—");weather->setText("Connecting weather…");currentIcon->setCode(-1);feels->clear();sunTimes->clear();forecast->clear();}
        briefing->updateBriefing(latestBriefing);caldav.reload();refreshData();renderAgenda();markDates();
    }
    static QString preferencesPath() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/porthole/preferences.ini";
    }
    void applyTheme(bool dark) {
        darkTheme=dark;
        const QString background=dark?"#171d25":"#f6f5ef";
        const QString foreground=dark?"#e6ebe9":"#253c3c";
        const QString border=dark?"#39464f":"#d7ddd5";
        const QString base=dark?"#202a33":"#ffffff";
        const QString muted=dark?"#a3b3b8":"#61736d";
        const QString accent=dark?"#82c6b6":"#26756a";
        const QString selected=dark?"#28564d":"#26756a";
        const QString selectedText=dark?"#f0fff8":"#ffffff";
        const QString pressed=dark?"#30434b":"#dce6df";
        QPalette palette=qApp->style()->standardPalette();
        palette.setColor(QPalette::Window,QColor(background));palette.setColor(QPalette::WindowText,QColor(foreground));
        palette.setColor(QPalette::Base,QColor(base));palette.setColor(QPalette::AlternateBase,QColor(background));
        palette.setColor(QPalette::Text,QColor(foreground));palette.setColor(QPalette::Button,QColor(background));
        palette.setColor(QPalette::ButtonText,QColor(foreground));palette.setColor(QPalette::Highlight,QColor(selected));
        palette.setColor(QPalette::HighlightedText,QColor(selectedText));palette.setColor(QPalette::Link,QColor(accent));
        palette.setColor(QPalette::ToolTipBase,QColor(base));palette.setColor(QPalette::ToolTipText,QColor(foreground));
        palette.setColor(QPalette::PlaceholderText,QColor(muted));
        for(auto role:{QPalette::Text,QPalette::WindowText,QPalette::ButtonText})palette.setColor(QPalette::Disabled,role,QColor(muted));
        qApp->setPalette(palette);
        qApp->setStyleSheet(QString(
            "QWidget {background:%1;color:%2;} QLabel {border:0;} QLabel[tone=secondary] {color:%5;} QLabel[tone=notice] {color:%10;} "
            "QPushButton {border:1px solid %3;border-radius:4px;padding:6px 16px;} "
            "QPushButton:checked {background:%7;color:%8;border-color:%7;} QPushButton:pressed {background:%9;} "
            "QPushButton:disabled {color:%5;} QLineEdit {background:%4;border:1px solid %5;padding:8px;selection-background-color:%7;selection-color:%8;} "
            "QScrollArea {border:0;} QFrame[frameShape=\"4\"] {color:%3;} "
            "QWidget#agendaEvent {border-left:3px solid %6;} QLabel#aiBadge {border:1px solid %3;border-radius:2px;padding:1px;} "
            "QCalendarWidget QToolButton {min-height:44px;font-size:22px;} "
            "QCalendarWidget QAbstractItemView {font-size:24px;selection-background-color:%7;selection-color:%8;} "
            "QMenu {background:%4;border:1px solid %3;} QMenu::item:selected {background:%7;color:%8;} "
            "QTabWidget::pane {border:0;} QTabBar::tab {min-height:26px;padding:10px 24px;margin-right:6px;border:1px solid %3;border-radius:4px;} QTabBar::tab:selected {background:%7;color:%8;} "
            "QListWidget {border:1px solid %3;border-radius:4px;} QCheckBox {spacing:12px;} QCheckBox::indicator {width:24px;height:24px;}"
        ).arg(background,foreground,border,base,muted,accent,selected,selectedText,pressed).arg(dark?"#d7b18c":"#8b542c"));
        QTextCharFormat weekend;weekend.setForeground(QColor(dark?"#ddad9a":"#9a5949"));
        calendar->setWeekdayTextFormat(Qt::Saturday,weekend);calendar->setWeekdayTextFormat(Qt::Sunday,weekend);
        markDates();
    }
    void updateClock() {
        const auto time=now();if(sunDate.isValid()&&sunDate!=time.date()){sunDate={};sunTimes->setText("Sunrise / sunset updating…");if(!demo)refreshWeather();}
        clock->setTime(time.time());date->setText(time.toString("dddd, MMMM d"));
        if(view<2&&selectedDate!=time.date().addDays(view)){selectedDate=time.date().addDays(view);if(!demo)refreshCalendar();renderAgenda();}
    }
    void renderStatus() {
        footer->setUpdateTimes(lastCalendar,lastWeather,lastSummary,weatherStale,demo);
        footer->setIssueCount(diagnostics.activeCount());syncSettings();
    }
    void changeView(int next) {
        view=next;footer->setView(view);
        conditions->setVisible(view!=2);calendar->setVisible(view==2);
        selectedDate=view==2?calendar->selectedDate():now().date().addDays(view);
        if(view<2&&!demo&&!selected.isEmpty())caldav.fetch(selected,selectedDate);
        renderAgenda();
    }
    void renderAgenda() {
        while(auto *item=eventLayout->takeAt(0)){delete item->widget();delete item;}
        heading->setText(view==0?"Today":view==1?"Tomorrow":selectedDate.toString("ddd, MMM d"));
        int count=0;
        for(const auto &event:events){
            if(!event.onDate(selectedDate))continue;
            ++count;
            auto *box=new QWidget;auto *layout=new QVBoxLayout(box);layout->setContentsMargins(16,2,0,8);
            box->setObjectName("agendaEvent");
            auto *metadata=label(event.timeLabel()+" · "+event.calendar,16);metadata->setProperty("tone","secondary");layout->addWidget(metadata);
            layout->addWidget(label(event.title,28));
            if(!event.location.isEmpty()){auto *place=label(event.location,16);place->setProperty("tone","secondary");layout->addWidget(place);}
            eventLayout->addWidget(box);
        }
        caption->setText(QString::number(count)+(count==1?" event":" events")+(view==0?" today":view==1?" tomorrow":" on this day"));
        if(!demo&&selected.isEmpty())caption->setText("Your plans, together");
        if(count==0){
            eventLayout->addWidget(label(selected.isEmpty()&&!demo?"Connect iCloud in Settings to see your family’s plans.":"Nothing scheduled for this day.",26));
        }
        eventLayout->addStretch();renderStatus();
    }
    void markDates() {
        calendar->setDateTextFormat(QDate(),QTextCharFormat());
        QTextCharFormat fmt;fmt.setFontWeight(QFont::Bold);fmt.setForeground(QColor(darkTheme?"#82c6b6":"#26756a"));
        for(const auto &event:events){QDate date=event.start.date();for(int i=0;i<90&&event.onDate(date);++i,date=date.addDays(1))calendar->setDateTextFormat(date,fmt);}
    }
    void sampleData() {
        selected.clear();events.clear();demo=true;briefing->updateBriefing({},true);
        ++weatherGeneration;for(auto *reply:weatherNetwork.findChildren<QNetworkReply*>())reply->abort();
        const auto today=now().date();
        events<<Event{"sample-library","Library visit","Example: after school","Family",QDateTime(today,QTime(16,0),configuredZone()),QDateTime(today,QTime(17,0),configuredZone()),false};
        events<<Event{"sample-dinner","Dinner together","Example: at home","Home",QDateTime(today,QTime(18,30),configuredZone()),QDateTime(today,QTime(19,30),configuredZone()),false};
        events<<Event{"sample-soccer","Soccer practice","Example: community field","Family",QDateTime(today.addDays(1),QTime(17,0),configuredZone()),QDateTime(today.addDays(1),QTime(18,0),configuredZone()),false};
        calendarStatus="Sample calendar · iCloud is not connected";weatherStatus="Sample forecast · not live weather";
        temperature->setText("14°");weather->setText("Partly cloudy");feels->setText("Feels like 12° · High 16° / Low 8°");weatherLocation->setText("Example City");currentIcon->setCode(2);sunDate=today;sunTimes->setText("Sunrise 7:35 AM · Sunset 6:35 PM");
        forecast->setForecast({{today.addDays(1),3,13,7},{today.addDays(2),61,12,5},{today.addDays(3),0,15,6}});
        lastCalendar=lastWeather=lastSummary=now();weatherStale=false;
        if(qApp->arguments().contains("--preview-errors")){weatherStale=true;diagnostics.fail(DataSource::Weather,IssueCode::WeatherUnavailable);}
        updateClock();changeView(0);markDates();
    }
    void signIn() {
        if(!online()){QMessageBox::information(settings?static_cast<QWidget*>(settings.data()):this,"Frame is offline","Choose a Wi-Fi network in Settings first, then connect iCloud.");return;}
        SignIn dialog(settings?static_cast<QWidget*>(settings.data()):this);dialog.account->setText(caldav.accountEmail());if(dialog.exec()!=QDialog::Accepted)return;
        QByteArray password=dialog.password->text().toLatin1();const auto account=dialog.account->text();dialog.password->clear();
        demo=false;events.clear();selected.clear();calendarStatus="Discovering your iCloud calendars…";
        temperature->setText("—");weather->setText("Loading weather…");forecast->clear();
        caldav.connectAccount(account,password,dialog.remember->isChecked());password.fill('\0');renderAgenda();
    }
    void chooseCalendars(const QList<Calendar> &calendars) {
        if(calendars.isEmpty()){calendarStatus="No readable event calendars were found.";renderAgenda();return;}
        QDialog dialog(settings?static_cast<QWidget*>(settings.data()):this);dialog.setWindowTitle("Choose calendars");dialog.resize(this->size());
        auto *layout=new QVBoxLayout(&dialog);layout->addWidget(label("Which calendars should appear?",28));
        layout->addWidget(label("Choose up to 16. Porthole only reads events.",17));
        auto *list=new QListWidget;list->setStyleSheet("QListWidget::item {min-height:48px;}");
        for(const auto &calendar:calendars){auto *item=new QListWidgetItem(calendar.name,list);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(std::any_of(selected.begin(),selected.end(),[&](const Calendar &chosen){return chosen.url==calendar.url;})?Qt::Checked:Qt::Unchecked);}
        layout->addWidget(list,1);auto *actions=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);layout->addWidget(actions);
        connect(actions,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(actions,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted){calendarStatus="Calendar selection unchanged";renderAgenda();return;}
        selected.clear();for(int i=0;i<list->count();++i)if(list->item(i)->checkState()==Qt::Checked)selected<<calendars[i];
        if(selected.isEmpty()||selected.size()>16){selected.clear();calendarStatus="Choose between 1 and 16 calendars to connect.";renderAgenda();return;}
        caldav.selectCalendars(selected);refreshData();
    }
    void refreshData() {
        if(demo)return;
        refreshCalendar();refreshWeather();
    }
    void refreshCalendar(){
        if(demo)return;
        if(!selected.isEmpty())caldav.fetch(selected,view==2?month:now().date().addDays(view));
        else caldav.requestStatus();
    }
    void refreshWeather(){
        if(demo)return;
        if(online())fetchWeather();else{weatherStale=true;diagnostics.fail(DataSource::Weather,IssueCode::WeatherUnavailable);weatherStatus=lastWeather.isValid()?"Weather is stale · frame offline":"Weather unavailable · frame offline";}
        renderStatus();
    }
    void fetchWeather() {
        const auto configured=weatherForecastUrl(householdProfile());
        if(!configured){weatherStatus="Choose a weather location in Settings → Data.";weather->setText("Weather is not configured");renderStatus();return;}
        const QUrl url=*configured;
        QNetworkRequest request(url);request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        auto *reply=weatherNetwork.get(request);const auto token=weatherGeneration;
        connect(reply,&QNetworkReply::readyRead,this,[reply]{if(reply->bytesAvailable()>65536)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,token]{
            reply->deleteLater();if(token!=weatherGeneration)return;
            if(reply->error()!=QNetworkReply::NoError||reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200||reply->bytesAvailable()>65536){weatherStale=true;diagnostics.fail(DataSource::Weather,IssueCode::WeatherUnavailable);weatherStatus=lastWeather.isValid()?"Weather is stale · refresh failed":"Weather unavailable · refresh failed";renderStatus();return;}
            QJsonParseError error;const auto doc=QJsonDocument::fromJson(reply->readAll(),&error);const auto data=doc.object();const auto current=data["current"].toObject(),daily=data["daily"].toObject();
            const auto days=daily["time"].toArray(),high=daily["temperature_2m_max"].toArray(),low=daily["temperature_2m_min"].toArray(),codes=daily["weather_code"].toArray();
            bool valid=error.error==QJsonParseError::NoError&&current["temperature_2m"].isDouble()&&current["apparent_temperature"].isDouble()&&current["weather_code"].isDouble()&&validWeatherCode(current["weather_code"].toInt())&&days.size()>=4&&high.size()>=4&&low.size()>=4&&codes.size()>=4;
            QList<ForecastDay> future;QJsonArray briefingDays;
            for(int i=0;valid&&i<4;++i){const auto day=QDate::fromString(days[i].toString(),Qt::ISODate);valid=day.isValid()&&high[i].isDouble()&&low[i].isDouble()&&codes[i].isDouble()&&validWeatherCode(codes[i].toInt());
                if(valid){if(i>0)future<<ForecastDay{day,codes[i].toInt(),qRound(high[i].toDouble()),qRound(low[i].toDouble())};briefingDays.append(QJsonObject{{"date",days[i]},{"high_c",high[i]},{"low_c",low[i]},{"code",codes[i]}});}
            }
            if(!valid){weatherStale=true;diagnostics.fail(DataSource::Weather,IssueCode::InvalidForecast);weatherStatus="Weather response could not be read.";renderStatus();return;}
            temperature->setText(QString::number(qRound(current["temperature_2m"].toDouble()))+"°");weather->setText(weatherCondition(current["weather_code"].toInt()));currentIcon->setCode(current["weather_code"].toInt());
            feels->setText(QString("Feels like %1° · High %2° / Low %3°").arg(qRound(current["apparent_temperature"].toDouble())).arg(qRound(high[0].toDouble())).arg(qRound(low[0].toDouble())));
            const auto sunrise=QDateTime::fromString((daily["sunrise"].toArray().isEmpty()?QString():daily["sunrise"].toArray().first().toString()),Qt::ISODate);
            const auto sunset=QDateTime::fromString((daily["sunset"].toArray().isEmpty()?QString():daily["sunset"].toArray().first().toString()),Qt::ISODate);
            sunDate=now().date();
            sunTimes->setText(sunrise.isValid()&&sunset.isValid()&&sunrise.date()==now().date()&&sunset.date()==now().date()?QString("Sunrise %1 · Sunset %2").arg(sunrise.time().toString("h:mm AP"),sunset.time().toString("h:mm AP")):QString("Sunrise / sunset unavailable"));
            forecast->setForecast(future);lastWeather=now();weatherStale=false;diagnostics.recover(DataSource::Weather);
            caldav.briefingWeather({{"profileRevision",profileRevision(householdProfile())},{"checked",lastWeather.toString(Qt::ISODate)},{"days",briefingDays}});
            weatherStatus="Open-Meteo · updated "+lastWeather.toString("h:mm AP");renderStatus();
        });
    }
};

int main(int argc,char **argv) {
    for(int i=1;i<argc;++i)if(QString::fromLocal8Bit(argv[i])=="--self-check"){
        QCoreApplication app(argc,argv);const bool tls=QSslSocket::supportsSsl(),zone=configuredZone().isValid();
        QTextStream(stdout)<<"TLS available: "<<tls<<"\nConfigured timezone valid: "<<zone<<"\n";
        return tls&&zone?0:1;
    }
    QApplication app(argc,argv);app.setApplicationName("Porthole");app.setDesktopFileName("porthole");app.setStyle("Fusion");
    QFont appFont("DejaVu Sans");appFont.setPixelSize(18);app.setFont(appFont);
    Dashboard dashboard(app.arguments().contains("--demo"));dashboard.resize(1280,800);
    if(app.arguments().contains("--portrait"))dashboard.resize(800,1280);
    if(app.arguments().contains("--windowed"))dashboard.show();else dashboard.showFullScreen();
    if(app.arguments().contains("--settings")||(app.arguments().contains("--demo")&&app.arguments().contains("--preview-settings"))){
        QTimer::singleShot(0,&dashboard,[&dashboard]{if(auto *button=dashboard.findChild<QPushButton*>("settingsButton"))button->click();});
    }
    if(app.arguments().contains("--demo")&&app.arguments().contains("--preview-settings")){
        const auto tab=app.arguments().indexOf("--preview-tab");
        if(tab>=0&&tab+1<app.arguments().size()){
            const auto name=app.arguments()[tab+1];
            QTimer::singleShot(500,&app,[name]{if(auto *window=QApplication::activeModalWidget())if(auto *tabs=window->findChild<QTabWidget*>())for(int i=0;i<tabs->count();++i)if(QString(tabs->tabText(i)).replace("&&","&")==name)tabs->setCurrentIndex(i);});
        }
    }
    const auto capture=app.arguments().indexOf("--capture");
    if(app.arguments().contains("--demo")&&capture>=0&&capture+1<app.arguments().size()){
        const auto path=app.arguments()[capture+1];
        QTimer::singleShot(1500,&app,[&dashboard,&app,path]{auto *window=QApplication::activeModalWidget();app.exit((window?window:static_cast<QWidget*>(&dashboard))->grab().save(path)?0:1);});
    }
    return app.exec();
}
