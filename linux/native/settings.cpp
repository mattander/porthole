#include "profile.h"
#include "typography.h"
#include <QMessageBox>
#include <algorithm>
#include "settings.h"
#include "keyboard.h"
#include "diagnostics.h"
#include <QDialogButtonBox>
#include <QDateTime>
#include <QGuiApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QScroller>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QTabWidget>
#include <QTimer>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace {
QString checked(QDateTime value){
    if(!value.isValid())return "—";
    value=value.toTimeZone(configuredZone());
    return value.toString(value.date()==QDateTime::currentDateTimeUtc().toTimeZone(configuredZone()).date()?"h:mm AP":"ddd, MMM d · h:mm AP");
}
QLabel *text(const QString &value,int size,bool secondary=false) {
    auto *label=new QLabel(value);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);
    QFont font=label->font();font.setPixelSize(size);label->setFont(font);
    if(secondary)label->setProperty("tone","secondary");
    return label;
}
QPushButton *action(const QString &name){auto *button=new QPushButton(name);button->setMinimumHeight(48);return button;}
}

SettingsPane::SettingsPane(WifiController *wifi,const SettingsActions &actions,QWidget *parent):QDialog(parent) {
    setWindowTitle("Porthole settings");resize(parent?parent->size():QSize(1050,730));
    auto *root=new QVBoxLayout(this);root->setContentsMargins(28,20,28,20);root->setSpacing(20);
    auto *header=new QHBoxLayout;auto *title=new QVBoxLayout;title->addWidget(text("Settings",34));header->addLayout(title,1);
    auto *done=action("Back to dashboard");header->addWidget(done);connect(done,&QPushButton::clicked,this,&QDialog::accept);root->addLayout(header);
    auto *tabs=new QTabWidget;tabs->setStyleSheet("QTabBar::tab {padding:10px 14px;margin-right:5px;}");QFont tabFont=tabs->font();tabFont.setPixelSize(16);tabs->setFont(tabFont);root->addWidget(tabs,1);

    auto *wifiPage=new QWidget;auto *networkLayout=new QVBoxLayout(wifiPage);networkLayout->setContentsMargins(16,20,16,16);networkLayout->setSpacing(16);
    auto *networkStatus=text(wifi->status(),22);networkLayout->addWidget(networkStatus);
    networkLayout->addWidget(text("Choose your network. The password is entered on this frame.",16,true));
    auto *networks=new QListWidget;networks->setObjectName("wifiNetworks");networks->setStyleSheet("QListWidget::item {min-height:58px;padding:8px;}");networkLayout->addWidget(networks,1);
    QScroller::grabGesture(networks->viewport(),QScroller::TouchGesture);
    auto *buttons=new QHBoxLayout;auto *scan=action("Refresh networks"),*join=action("Join selected network"),*disconnect=action("Disconnect");buttons->addWidget(scan);buttons->addWidget(join);buttons->addWidget(disconnect);networkLayout->addLayout(buttons);
    auto update=[wifi,networkStatus,networks,disconnect]{
        const auto selected=networks->currentItem()?networks->currentItem()->data(Qt::UserRole).toString():QString();
        const int scroll=networks->verticalScrollBar()->value();
        networks->clear();networkStatus->setText(wifi->status());disconnect->setEnabled(wifi->connected());
        for(const auto &network:wifi->networks()){
            const QString security=network.type=="psk"?"Password protected":network.type=="open"?"Open network":network.type=="8021x"?"Enterprise":"Unsupported security";
            auto *item=new QListWidgetItem(network.name+"\n"+(network.connected?"Connected · ":"")+security,networks);item->setData(Qt::UserRole,network.path);
            if(network.path==selected)networks->setCurrentItem(item);
        }
        networks->verticalScrollBar()->setValue(scroll);
    };
    connect(wifi,&WifiController::changed,this,update);update();
    connect(scan,&QPushButton::clicked,wifi,&WifiController::scan);
    connect(join,&QPushButton::clicked,this,[wifi,networks]{if(auto *item=networks->currentItem())wifi->connectNetwork(item->data(Qt::UserRole).toString());});
    connect(disconnect,&QPushButton::clicked,wifi,&WifiController::disconnectNetwork);
    tabs->addTab(wifiPage,"Wi-Fi");wifi->setDialogParent(this);
    connect(this,&QDialog::finished,wifi,[wifi]{wifi->setDialogParent(nullptr);});
    QTimer::singleShot(0,wifi,&WifiController::scan);

    auto *cloudPage=new QWidget;auto *cloud=new QVBoxLayout(cloudPage);cloud->setContentsMargins(24,24,24,24);cloud->setSpacing(20);
    cloud->addWidget(text("Your iCloud calendars",28));calendarStatus=text("",20);cloud->addWidget(calendarStatus);
    cloud->addWidget(text("Sign in with your Apple Account email and a dedicated app-specific password. Porthole reads only the calendars you select.",18,true));
    auto *cloudButtons=new QHBoxLayout;auto *signIn=action("Connect iCloud");signOutButton=action("Forget account");chooseButton=action("Change calendars");cloudButtons->addWidget(signIn);cloudButtons->addWidget(chooseButton);cloudButtons->addWidget(signOutButton);cloud->addLayout(cloudButtons);
    connect(signIn,&QPushButton::clicked,this,actions.signIn);connect(chooseButton,&QPushButton::clicked,this,actions.changeCalendars);connect(signOutButton,&QPushButton::clicked,this,actions.signOut);
    auto *replace=action("Replace password");cloud->addWidget(replace,0,Qt::AlignLeft);connect(replace,&QPushButton::clicked,this,actions.signIn);
    cloud->addWidget(text("Selected calendars",18));calendarNames=text("",18,true);cloud->addWidget(calendarNames);cloud->addStretch();
    cloud->addWidget(text("Remembered accounts reconnect after restarting. Stored credentials are encrypted on this SD card. Never enter your primary Apple Account password.",16,true));
    auto *accounts=new QTabWidget;accounts->addTab(cloudPage,"iCloud");
    auto *googlePage=new QWidget;auto *gl=new QVBoxLayout(googlePage);gl->setContentsMargins(24,24,24,24);gl->setSpacing(20);
    gl->addWidget(text("Google Workspace & Google Calendar",28));googleStatus=text("Not connected",20);gl->addWidget(googleStatus);
    gl->addWidget(text("Google calendars appear alongside iCloud in the dashboard and AI briefing. Access is read-only.",18,true));
    auto *gb=new QHBoxLayout;auto *setup=action("Connect Google");googleChoose=action("Choose Google calendars");googleForget=action("Forget Google");gb->addWidget(setup);gb->addWidget(googleChoose);gb->addWidget(googleForget);gl->addLayout(gb);
    connect(setup,&QPushButton::clicked,this,[actions]{if(actions.setupGoogle)actions.setupGoogle();});connect(googleChoose,&QPushButton::clicked,this,[actions]{if(actions.chooseGoogle)actions.chooseGoogle();});connect(googleForget,&QPushButton::clicked,this,[actions]{if(actions.forgetGoogle)actions.forgetGoogle();});
    gl->addWidget(text("Sign in once through your Mac browser using the setup helper. No Google password is entered on the frame. The encrypted refresh token reconnects after restarting.",18,true));gl->addStretch();
    gl->addWidget(text("Workspace administrators may need to allow your OAuth client. See the Google setup guide in the project README.",16,true));accounts->addTab(googlePage,"Google");tabs->addTab(accounts,"Calendars");updateGoogle({});

    auto *displayPage=new QWidget;auto *display=new QVBoxLayout(displayPage);display->setContentsMargins(24,24,24,24);display->setSpacing(20);
    display->addWidget(text("Appearance",28));display->addWidget(text("Choose the look of your wall display. Your choice is saved on this SD card.",18,true));
    auto *themes=new QHBoxLayout;lightButton=action("Light");darkButton=action("Dark");lightButton->setCheckable(true);darkButton->setCheckable(true);auto *group=new QButtonGroup(this);group->addButton(lightButton);group->addButton(darkButton);themes->addWidget(lightButton);themes->addWidget(darkButton);themes->addStretch();display->addLayout(themes);
    connect(lightButton,&QPushButton::clicked,this,[actions]{actions.theme(false);});connect(darkButton,&QPushButton::clicked,this,[actions]{actions.theme(true);});
    auto *appearanceModes=new QHBoxLayout;auto *modeGroup=new QButtonGroup(this);
    manualMode=action("Manual");ambientMode=action("Auto\nLight sensor");sunMode=action("Auto\nSunrise / sunset");
    const auto addMode=[&](QPushButton *button,AppearanceMode mode,const QString &name){
        button->setObjectName(name);button->setCheckable(true);button->setMinimumHeight(64);modeGroup->addButton(button);appearanceModes->addWidget(button,1);
        connect(button,&QPushButton::clicked,this,[actions,mode]{if(actions.appearance)actions.appearance(mode);});
    };
    addMode(manualMode,AppearanceMode::Manual,"appearanceManual");addMode(ambientMode,AppearanceMode::AmbientLight,"appearanceAmbient");addMode(sunMode,AppearanceMode::SunSchedule,"appearanceSun");display->addLayout(appearanceModes);
    appearanceStatus=text("",16,true);appearanceStatus->setObjectName("appearanceStatus");display->addWidget(appearanceStatus);
    auto *brightnessHeading=new QHBoxLayout;brightnessHeading->addWidget(text("Brightness",24),1);brightnessValue=text("—",24);brightnessValue->setObjectName("brightnessValue");brightnessHeading->addWidget(brightnessValue);display->addLayout(brightnessHeading);
    auto *brightnessRow=new QHBoxLayout;dimmer=action("−");brighter=action("+");dimmer->setFixedWidth(64);brighter->setFixedWidth(64);dimmer->setAccessibleName("Decrease brightness");brighter->setAccessibleName("Increase brightness");
    brightness=new QSlider(Qt::Horizontal);brightness->setObjectName("brightness");brightness->setAccessibleName("Screen brightness");brightness->setMinimumHeight(56);brightness->setRange(1,10);brightness->setPageStep(1);
    brightness->setStyleSheet("QSlider::groove:horizontal {height:8px;background:#81918c;border-radius:4px;} QSlider::handle:horizontal {width:32px;margin:-16px 0;background:palette(highlight);border:2px solid palette(text);border-radius:16px;}");
    brightnessRow->addWidget(dimmer);brightnessRow->addWidget(brightness,1);brightnessRow->addWidget(brighter);display->addLayout(brightnessRow);
    connect(dimmer,&QPushButton::clicked,brightness,[this]{brightness->setValue(brightness->value()-1);});connect(brighter,&QPushButton::clicked,brightness,[this]{brightness->setValue(brightness->value()+1);});
    connect(brightness,&QSlider::valueChanged,this,[actions](int level){if(actions.brightness)actions.brightness(level);});
    brightnessStatus=text("Brightness control is unavailable.",16,true);brightnessStatus->setObjectName("brightnessStatus");display->addWidget(brightnessStatus);updateBrightness({});
    display->addWidget(text("Rotation follows the frame",22));display->addWidget(text("Turn it to either landscape or portrait direction. The screen and touch positions follow together.",18,true));display->addStretch();
    auto *fontLicence=action("Font licences");display->addWidget(fontLicence,0,Qt::AlignLeft);
    connect(fontLicence,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);dialog.setWindowTitle("Bundled font licences");dialog.resize(size());auto *layout=new QVBoxLayout(&dialog);
        layout->addWidget(text("Bundled font licences",28));auto *licence=new QPlainTextEdit(interfaceFontLicence());licence->setObjectName("fontLicenceText");licence->setReadOnly(true);licence->setLineWrapMode(QPlainTextEdit::WidgetWidth);layout->addWidget(licence,1);
        auto *close=action("Back to Settings");layout->addWidget(close);connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);dialog.exec();
    });auto *displayScroll=new QScrollArea;displayScroll->setObjectName("displayScroll");displayScroll->setWidgetResizable(true);displayScroll->setFrameShape(QFrame::NoFrame);displayScroll->setWidget(displayPage);QScroller::grabGesture(displayScroll->viewport(),QScroller::TouchGesture);tabs->addTab(displayScroll,"Display");

    auto *dataPage=new QWidget;auto *data=new QVBoxLayout(dataPage);data->setContentsMargins(24,24,24,24);data->setSpacing(20);
    data->addWidget(text("Last updated",22));dataUpdated=text("",18);dataUpdated->setObjectName("dataUpdated");data->addWidget(dataUpdated);updateTimes({},{},{});
    data->addWidget(text("Weather & time",28));profileInfo=text("Weather location is not configured",18);profileInfo->setObjectName("profileInfo");data->addWidget(profileInfo);weatherStatus=text("",18,true);data->addWidget(weatherStatus);data->addWidget(text("Calendar checks every minute · Weather refreshes every 15 minutes",16,true));data->addWidget(text("Weather provider: Open-Meteo",16,true));
    auto *editProfile=action("Edit location and timezone");data->addWidget(editProfile,0,Qt::AlignLeft);
    connect(editProfile,&QPushButton::clicked,this,[this,actions]{
        QDialog dialog(this);dialog.setWindowTitle("Weather & timezone");dialog.resize(size());auto *layout=new QVBoxLayout(&dialog);layout->setSpacing(10);layout->addWidget(text("Weather & timezone",28));
        auto *city=new QLineEdit(lastProfile["weather"].toObject()["label"].toString());city->setObjectName("profileLocation");city->setMaxLength(120);city->setMinimumHeight(48);layout->addWidget(text("Location name",16));layout->addWidget(city);
        auto *zone=new QLineEdit(lastProfile["timezone"].toString("UTC"));zone->setObjectName("profileTimezone");zone->setMaxLength(100);zone->setMinimumHeight(48);layout->addWidget(text("Timezone (IANA region/city or UTC)",16));layout->addWidget(zone);
        auto *coordinates=new QHBoxLayout;auto *latColumn=new QVBoxLayout,*lonColumn=new QVBoxLayout;
        auto *lat=new QLineEdit,*lon=new QLineEdit;lat->setObjectName("profileLatitude");lon->setObjectName("profileLongitude");
        const auto weather=lastProfile["weather"].toObject();if(weather.contains("latitude"))lat->setText(QString::number(weather["latitude"].toDouble(),'g',12));if(weather.contains("longitude"))lon->setText(QString::number(weather["longitude"].toDouble(),'g',12));
        lat->setMaxLength(30);lon->setMaxLength(30);lat->setMinimumHeight(48);lon->setMinimumHeight(48);latColumn->addWidget(text("Latitude",16));latColumn->addWidget(lat);lonColumn->addWidget(text("Longitude",16));lonColumn->addWidget(lon);coordinates->addLayout(latColumn);coordinates->addLayout(lonColumn);layout->addLayout(coordinates);
        layout->addWidget(new TouchKeyboard({city,zone,lat,lon},&dialog));layout->addStretch();
        auto *error=text("",16,true);error->setObjectName("profileError");layout->addWidget(error);
        auto *buttons=new QHBoxLayout;auto *save=action("Save profile"),*cancel=action("Cancel");buttons->addWidget(save);buttons->addWidget(cancel);buttons->addStretch();layout->addLayout(buttons);
        connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
        connect(save,&QPushButton::clicked,&dialog,[&]{
            auto value=lastProfile;value["version"]=1;value["timezone"]=zone->text().trimmed();if(!value.contains("familyContext"))value["familyContext"]="";
            if(city->text().trimmed().isEmpty()&&lat->text().trimmed().isEmpty()&&lon->text().trimmed().isEmpty())value["weather"]=QJsonValue::Null;
            else {bool latOk=false,lonOk=false;const auto latitude=lat->text().toDouble(&latOk),longitude=lon->text().toDouble(&lonOk);if(!latOk||!lonOk){error->setText("Enter numeric latitude and longitude.");return;}value["weather"]=QJsonObject{{"label",city->text()},{"latitude",latitude},{"longitude",longitude}};}
            const auto parsed=HouseholdProfile::parse(value);
            if(const auto *failure=std::get_if<ProfileError>(&parsed)){error->setText(profileErrorMessage(*failure));return;}
            if(const auto *profile=std::get_if<HouseholdProfile>(&parsed)){if(actions.profile)actions.profile(profile->json());dialog.accept();}
        });dialog.exec();
    });
    auto *refresh=action("Refresh live data");data->addWidget(refresh,0,Qt::AlignLeft);connect(refresh,&QPushButton::clicked,this,actions.refresh);
    data->addWidget(text("Software updates",22));softwareStatus=text("Update service is not connected",18);softwareStatus->setObjectName("softwareStatus");data->addWidget(softwareStatus);
    automaticUpdates=new TouchCheckBox("Install updates automatically");automaticUpdates->setObjectName("automaticUpdates");data->addWidget(automaticUpdates);
    checkUpdates=action("Check for updates");checkUpdates->setObjectName("checkUpdates");data->addWidget(checkUpdates,0,Qt::AlignLeft);
    connect(automaticUpdates,&QCheckBox::clicked,this,[actions](bool enabled){if(actions.automaticUpdates)actions.automaticUpdates(enabled);});
    connect(checkUpdates,&QPushButton::clicked,this,[actions]{if(actions.checkUpdates)actions.checkUpdates();});
    data->addWidget(text("Checks run hourly over Wi-Fi. Installing an update briefly restarts the dashboard. Your settings and saved accounts stay on this frame.",16,true));
    updateSoftware(std::nullopt);
    auto *line=new QFrame;line->setFrameShape(QFrame::HLine);data->addWidget(line);data->addWidget(text("Data mode",22));modeStatus=text("",17,true);data->addWidget(modeStatus);
    auto *modes=new QHBoxLayout;auto *live=action("Use live data"),*sample=action("View sample data");modes->addWidget(live);modes->addWidget(sample);modes->addStretch();data->addLayout(modes);
    connect(live,&QPushButton::clicked,this,actions.useLive);connect(sample,&QPushButton::clicked,this,actions.useSample);
    data->addStretch();data->addWidget(text("Porthole · Native Linux app · Running from your SD card",15,true));auto *dataScroll=new QScrollArea;dataScroll->setWidgetResizable(true);dataScroll->setFrameShape(QFrame::NoFrame);dataScroll->setWidget(dataPage);QScroller::grabGesture(dataScroll->viewport(),QScroller::TouchGesture);tabs->addTab(dataScroll,"Data");
    auto *aiPage=new QWidget;auto *ai=new QVBoxLayout(aiPage);ai->setContentsMargins(24,24,24,24);ai->setSpacing(16);
    ai->addWidget(text("Family briefing",28));briefingStatus=text("AI briefing is off",18);briefingStatus->setObjectName("briefingStatus");ai->addWidget(briefingStatus);
    ai->addWidget(text("Add your OpenAI API key from the Mac over USB. The key stays in encrypted storage on this SD card.",17,true));
    briefingEnabled=new TouchCheckBox("Enable AI briefing");briefingEnabled->setObjectName("briefingEnabled");ai->addWidget(briefingEnabled);
    ai->addWidget(text("Household context",18));ai->addWidget(text("Names, school or lunch priorities, and household routines are saved privately on this frame. This context is included in AI requests when briefing is enabled.",16,true));
    householdContext=new QLineEdit;householdContext->setObjectName("householdContext");householdContext->setReadOnly(true);householdContext->setMaxLength(1600);householdContext->setPlaceholderText("Optional household context");ai->addWidget(householdContext);
    auto *editContext=action("Edit household context");ai->addWidget(editContext,0,Qt::AlignLeft);
    connect(editContext,&QPushButton::clicked,this,[this,actions]{
        QDialog dialog(this);dialog.setWindowTitle("Household context");dialog.resize(size());auto *layout=new QVBoxLayout(&dialog);layout->addWidget(text("Household context",28));layout->addWidget(text("Up to 1600 characters. Include relevant names, preferences and weekday/weekend routines.",17,true));
        auto *field=new QLineEdit(householdContext->text());field->setMaxLength(1600);field->setMinimumHeight(48);layout->addWidget(field);layout->addWidget(new TouchKeyboard({field},&dialog));layout->addStretch();
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()==QDialog::Accepted){auto value=lastProfile;value["familyContext"]=field->text();const auto parsed=HouseholdProfile::parse(value);if(const auto *profile=std::get_if<HouseholdProfile>(&parsed)){if(actions.profile)actions.profile(profile->json());}else QMessageBox::warning(this,"Settings not saved","Household context could not be validated.");}
    });
    ai->addWidget(text("Extra family priorities",18));briefingPriorities=new QLineEdit;briefingPriorities->setReadOnly(true);briefingPriorities->setMaxLength(240);briefingPriorities->setPlaceholderText("Conflicts, early starts, preparation and outdoor weather");ai->addWidget(briefingPriorities);
    auto *edit=action("Edit priorities");ai->addWidget(edit,0,Qt::AlignLeft);
    connect(edit,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);dialog.setWindowTitle("Family priorities");dialog.resize(size());auto *layout=new QVBoxLayout(&dialog);
        layout->addWidget(text("What should the briefing notice?",28));layout->addWidget(text("Up to 240 characters. For example: highlight school events and appointment overlaps.",17,true));
        auto *field=new QLineEdit(briefingPriorities->text());field->setMaxLength(240);field->setMinimumHeight(48);layout->addWidget(field);layout->addWidget(new TouchKeyboard({field},&dialog));layout->addStretch();
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()==QDialog::Accepted)briefingPriorities->setText(field->text());
    });
    auto *budgetRow=new QHBoxLayout;budgetRow->addWidget(text("Monthly app limit (US$)",18));briefingBudget=new QComboBox;briefingBudget->setMinimumHeight(48);briefingBudget->addItem("$0.25",25);briefingBudget->addItem("$1.00",100);briefingBudget->addItem("$3.00",300);briefingBudget->setCurrentIndex(1);budgetRow->addWidget(briefingBudget);budgetRow->addStretch();ai->addLayout(budgetRow);
    briefingSpending=text("",16,true);ai->addWidget(briefingSpending);
    auto *aiButtons=new QHBoxLayout;auto *save=action("Save AI settings"),*regenerate=action("Update briefing");aiButtons->addWidget(save);aiButtons->addWidget(regenerate);aiButtons->addStretch();ai->addLayout(aiButtons);
    connect(save,&QPushButton::clicked,this,[this,actions]{if(actions.briefing)actions.briefing(briefingEnabled->isChecked(),briefingPriorities->text(),briefingBudget->currentData().toInt());});
    connect(regenerate,&QPushButton::clicked,this,[actions]{if(actions.refreshBriefing)actions.refreshBriefing();});
    auto *forgetKey=action("Forget API key");ai->addWidget(forgetKey,0,Qt::AlignLeft);connect(forgetKey,&QPushButton::clicked,this,[actions]{if(actions.forgetBriefingKey)actions.forgetBriefingKey();});
    ai->addWidget(text("Today and the next 3 days of event titles, times and calendar names, plus the configured forecast, are sent to OpenAI. Descriptions, attendees and locations stay out.",16,true));
    ai->addWidget(text("GPT-6 Luna · Up to 400 characters · Morning 6 AM / Midday noon / Evening 4 PM · Change checks every 30 minutes, 6 AM–9 PM · Up to 31 attempts/day · Manual updates have a 5-minute cooldown",15,true));
    ai->addStretch();auto *aiScroll=new QScrollArea;aiScroll->setWidgetResizable(true);aiScroll->setFrameShape(QFrame::NoFrame);aiScroll->setWidget(aiPage);QScroller::grabGesture(aiScroll->viewport(),QScroller::TouchGesture);tabs->addTab(aiScroll,"AI");
    auto *diagnosticPage=new QWidget;auto *diagnostic=new QVBoxLayout(diagnosticPage);diagnostic->setContentsMargins(24,24,24,24);diagnostic->setSpacing(18);
    diagnostic->addWidget(text("Errors & debugging",28));diagnosticState=text("No active issues",20);diagnosticState->setObjectName("diagnosticState");diagnostic->addWidget(diagnosticState);
    diagnosticSources=text("",18);diagnosticSources->setObjectName("diagnosticSources");diagnostic->addWidget(diagnosticSources);
    diagnosticHealth=text("",16,true);diagnostic->addWidget(diagnosticHealth);
    diagnosticHistory=text("",17);diagnosticHistory->setObjectName("diagnosticHistory");diagnostic->addWidget(diagnosticHistory);
    auto *diagnosticButtons=new QHBoxLayout;auto *retry=action("Retry data"),*copy=action("Copy safe diagnostics"),*clear=action("Clear resolved history");diagnosticButtons->addWidget(retry);diagnosticButtons->addWidget(copy);diagnosticButtons->addWidget(clear);diagnostic->addLayout(diagnosticButtons);
    connect(retry,&QPushButton::clicked,this,actions.refresh);
    connect(copy,&QPushButton::clicked,this,[this]{QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(lastDiagnostics).toJson(QJsonDocument::Indented)));});
    connect(clear,&QPushButton::clicked,this,[actions]{if(actions.clearDiagnosticHistory)actions.clearDiagnosticHistory();});
    diagnostic->addWidget(text("History stays in memory, up to 50 entries. Diagnostics exclude passwords, API keys, account details, network names and event contents.",15,true));diagnostic->addStretch();
    auto *diagnosticScroll=new QScrollArea;diagnosticScroll->setWidgetResizable(true);diagnosticScroll->setFrameShape(QFrame::NoFrame);diagnosticScroll->setWidget(diagnosticPage);QScroller::grabGesture(diagnosticScroll->viewport(),QScroller::TouchGesture);tabs->addTab(diagnosticScroll,"Errors && debugging");

}
void SettingsPane::updateState(const QString &calendar,const QString &weather,const QStringList &calendars,bool dark,bool demo,bool hasAccount,bool canChoose) {
    calendarStatus->setText(calendar);weatherStatus->setText(weather);calendarNames->setText(calendars.isEmpty()?"No calendars connected":calendars.join("\n"));
    signOutButton->setEnabled(hasAccount||!calendars.isEmpty());chooseButton->setEnabled(canChoose);darkButton->setChecked(dark);lightButton->setChecked(!dark);
    modeStatus->setText(demo?"Sample mode · fictional events and weather":"Live mode · data appears after connecting");
}

void SettingsPane::updateBriefing(const QJsonObject &state){
    const auto next=QDateTime::fromString(state["nextCheck"].toString(),Qt::ISODate).toTimeZone(configuredZone());
    const auto period=state["period"].toString();
    briefingStatus->setText(state["status"].toString("AI briefing is off")+(next.isValid()?QString("\n%1 briefing · Next scheduled check: %2").arg(period.left(1).toUpper()+period.mid(1),next.toString("ddd h:mm AP")):QString()));
    briefingSpending->setText(QString("Conservative app estimate: $%1 used · %2 of %3 attempts today").arg(state["spentMicros"].toInteger()/1000000.0,0,'f',3).arg(state["attemptsToday"].toInt()).arg(state["dailyAttemptLimit"].toInt(31)));
    if(!briefingInitialized&&!state.isEmpty()){
        briefingInitialized=true;briefingEnabled->setChecked(state["enabled"].toBool());briefingPriorities->setText(state["priorities"].toString());
        const int index=briefingBudget->findData(state["budgetCents"].toInt(100));if(index>=0)briefingBudget->setCurrentIndex(index);
    }
}

void SettingsPane::updateTimes(QDateTime calendar,QDateTime weather,QDateTime summary,bool stale,bool sample,const QString &holidayStatus){
    dataUpdated->setText(QString("%1Calendars · %2\nWeather · %3%4\nBriefing · %5").arg(sample?"Sample data · fictional\n":"",checked(calendar),checked(weather),stale?" (stale)":"",checked(summary))+(holidayStatus.isEmpty()?QString():"\n"+holidayStatus));
}
void SettingsPane::updateAppearance(AppearanceMode mode,const QString &status){
    manualMode->setChecked(mode==AppearanceMode::Manual);ambientMode->setChecked(mode==AppearanceMode::AmbientLight);sunMode->setChecked(mode==AppearanceMode::SunSchedule);
    appearanceStatus->setText(mode==AppearanceMode::Manual?"Choose Light or Dark above. This choice stays until you change it.":status);
}
void SettingsPane::updateBrightness(const std::optional<BacklightState> &state,const QString &status){
    const bool available=state&&state->writable;
    brightness->setEnabled(available);dimmer->setEnabled(available);brighter->setEnabled(available);
    if(state){
        QSignalBlocker blocker(brightness);brightness->setRange(1,state->maximum);if(!brightness->isSliderDown())brightness->setValue(std::max(1,state->level));
        brightnessValue->setText(QString::number(state->percent())+"%");dimmer->setEnabled(available&&state->level>1);brighter->setEnabled(available&&state->level<state->maximum);
    }else brightnessValue->setText("—");
    brightnessStatus->setText(!status.isEmpty()?status:available?"Saved on this frame. The lowest level keeps the screen on.":"Brightness control is unavailable on this device.");
}
void SettingsPane::updateDiagnostics(const QJsonObject &state){
    lastDiagnostics=safeDiagnostics(state);const auto &safe=lastDiagnostics;const int active=safe["active"].toInt();diagnosticState->setText(active?QString("%1 %2").arg(active).arg(active==1?"issue needs attention":"issues need attention"):"No active issues");
    QStringList sources;for(const auto &value:safe["sources"].toArray()){const auto s=value.toObject();const auto time=QDateTime::fromString(s["checked"].toString(),Qt::ISODate).toTimeZone(configuredZone());sources<<s["name"].toString()+" · "+s["status"].toString()+" · "+(time.isValid()?time.toString("ddd h:mm AP"):s["checked"].toString());}diagnosticSources->setText(sources.join("\n\n"));
    const auto health=safe["device"].toObject();QStringList facts;
    if(health.contains("availableMemoryMB"))facts<<QString("Memory available: %1 MB").arg(health["availableMemoryMB"].toInteger());
    if(health.contains("uptimeSeconds")){const qint64 minutes=health["uptimeSeconds"].toInteger()/60;facts<<QString("Uptime: %1h %2m").arg(minutes/60).arg(minutes%60);}
    facts<<QString("Conservative AI spend: US$%1").arg(safe["spentMicros"].toInteger()/1000000.0,0,'f',6);diagnosticHealth->setText(facts.join(" · "));
    QStringList entries;int count=0;for(const auto &value:safe["issues"].toArray()){
        if(count++>=50)break;
        const auto issue=value.toObject();const auto time=QDateTime::fromString(issue["last"].toString(),Qt::ISODate).toTimeZone(configuredZone());
        entries<<issue["service"].toString()+" · "+issue["message"].toString()+(issue["resolved"].toBool()?" (resolved)":"")+"\n"+issue["action"].toString()+"\n"+time.toString("ddd h:mm AP");
    }
    diagnosticHistory->setText(entries.isEmpty()?"No errors recorded in this app session.":entries.join("\n\n"));
}

void SettingsPane::updateProfile(const QJsonObject &profile){
    const auto parsed=HouseholdProfile::parse(profile);if(const auto *value=std::get_if<HouseholdProfile>(&parsed)){lastProfile=value->json();profileInfo->setText((value->weather()?value->weather()->label():QString("Weather is not configured"))+" · "+QString::fromUtf8(value->zone().id()));householdContext->setText(value->familyContext());}
}

void SettingsPane::updateSoftware(const std::optional<SoftwareUpdate> &state){
    const QSignalBlocker blocker(automaticUpdates);
    automaticUpdates->setEnabled(state.has_value());checkUpdates->setEnabled(state&&state->phase!=UpdatePhase::Checking&&state->phase!=UpdatePhase::Installing);
    if(!state){softwareStatus->setText("Update service is not connected");return;}
    automaticUpdates->setChecked(state->automatic);
    auto value=QString("Version %1 · %2").arg(state->version,updatePhaseText(state->phase));
    if(state->checked.isValid())value+="\nLast checked: "+state->checked.toTimeZone(configuredZone()).toString("ddd h:mm AP");
    const auto failure=updateFailureText(state->failure);if(!failure.isEmpty())value+="\n"+failure;
    softwareStatus->setText(value);
}

void SettingsPane::updateGoogle(const QJsonObject &state){googleStatus->setText(state["status"].toString("Connect Google using the Mac setup helper"));googleChoose->setEnabled(state["authenticated"].toBool()&&!state["available"].toArray().isEmpty());googleForget->setEnabled(state["authenticated"].toBool()||!state["issue"].toString().isEmpty());}
