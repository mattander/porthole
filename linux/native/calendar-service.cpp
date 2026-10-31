#include "profile.h"
#include "caldav.h"
#include "google-calendar.h"
#include "calendar-sources.h"
#include <QSaveFile>
#include "briefing.h"
#include "diagnostic-log.h"
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QTextStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QTimeZone>
#include <QRegularExpression>
#include <QUuid>
#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <QSharedPointer>

namespace {
struct PendingRead {QByteArray bytes;bool finished=false;};
QJsonObject jsonCalendar(const Calendar &c){return {{"url",c.url.toString()},{"name",c.name},{"colour",canonicalCalendarColour(c.colour)}};}
QJsonArray jsonCalendars(const QList<Calendar> &items){QJsonArray values;for(const auto &c:items)values.append(jsonCalendar(c));return values;}
QJsonArray calendarSelection(const QList<Calendar> &items){QJsonArray values;for(const auto &c:items)values.append(QJsonObject{{"url",c.url.toString()},{"name",c.name}});return values;}
bool validAccount(const QString &account,const QByteArray &password){return account.size()<=254&&account.contains('@')&&!account.contains(':')&&!account.contains('\n')&&!account.contains('\r')&&!account.contains(QChar(0))&&QRegularExpression("^[a-z]{4}(-[a-z]{4}){3}$").match(QString::fromLatin1(password)).hasMatch();}
QByteArray readApiKey(){QFile f(qEnvironmentVariable("CREDENTIALS_DIRECTORY")+"/porthole-openai");if(!f.open(QIODevice::ReadOnly)||f.size()>600)return {};return f.readAll().trimmed();}
QDate today(){return QDateTime::currentDateTimeUtc().toTimeZone(configuredZone()).date();}
}
class CalendarService:public QObject {
    QElapsedTimer graphicsWindow;int graphicsPackets=0;
    Caldav caldav{this},briefingCalendar{this};GoogleCalendar google{this},googleUpcoming{this};
    QList<Calendar> googleAvailable,googleSelected;QList<Event> googleEvents,appleUpcomingEvents,googleUpcomingEvents;
    QDateTime googleChecked,appleUpcomingChecked,googleUpcomingChecked,upcomingEpoch;QString googleStatus="Connect Google using the Mac setup helper",googleIssue;
    std::optional<GoogleCredentials> googleCredentials;bool googleAuthenticated=false,googleListLoaded=false;QLocalServer server{this};
    BriefingEngine briefing{"/var/lib/porthole-calendar/briefing.json",readApiKey(),this};
    bool briefingConnecting=false,briefingReady=false,briefingBlocked=false;
    QString briefingStorageError;
    QDateTime lastChecked;QString calendarIssue;
    QString account,status="Connect iCloud in Settings";QByteArray password;
    QList<Calendar> available,selected;QList<Event> events;
    QStringList restoreUrls;QDate month=QDate(today().year(),today().month(),1);
    bool remember=false,remembered=false,authenticated=false,connecting=false,blocked=false,needsSelection=false,storing=false;
    qint64 eventsRevision=0,selectionRevision=0;
    uid_t allowedUid;QString instance=QUuid::createUuid().toString();
public:
    explicit CalendarService(uid_t uid):allowedUid(uid){
        connect(&caldav,&Caldav::calendarsReady,this,[this](const QList<Calendar> &items){
            connecting=false;authenticated=true;blocked=false;available=items;selected.clear();
            for(const auto &c:available)if(restoreUrls.contains(c.url.toString()))selected<<c;
            restoreUrls.clear();
            if(selected.isEmpty()){needsSelection=true;++selectionRevision;status="Choose your iCloud calendars";}
            else {needsSelection=false;fetch();refreshUpcoming();}
        });
        connect(&caldav,&Caldav::authorizationRejected,this,[this]{blocked=true;connecting=false;authenticated=false;calendarIssue="sign-in";resetUpcoming(true);status="Sign-in required · replace the app-specific password";});
        connect(&caldav,&Caldav::failed,this,[this](const QString &message){connecting=false;calendarIssue=blocked?"sign-in":"refresh";status=blocked?"Sign-in required · replace the app-specific password":message;});
        connect(&caldav,&Caldav::eventsReady,this,[this](const QList<Event> &loaded,QDate loadedMonth){
            if(loadedMonth!=month)return;
            events=loaded;++eventsRevision;lastChecked=QDateTime::currentDateTimeUtc();calendarIssue.clear();status="iCloud · checked "+QDateTime::currentDateTimeUtc().toTimeZone(configuredZone()).toString("h:mm AP");
        });
        connect(&briefingCalendar,&Caldav::calendarsReady,this,[this](const QList<Calendar> &items){
            briefingConnecting=false;briefingReady=true;
            for(const auto &c:selected)if(!std::any_of(items.begin(),items.end(),[&](const Calendar &v){return c.url==v.url;})){diagnosticEvent(DiagnosticEvent::UpcomingSelectionMissing);briefingReady=false;briefing.calendarUnavailable();return;}
            refreshUpcoming();
        });
        connect(&briefingCalendar,&Caldav::eventsReady,this,[this](const QList<Event> &items,QDate loaded){
            if(loaded==QDate(today().year(),today().month(),1)){appleUpcomingEvents=items;appleUpcomingChecked=QDateTime::currentDateTimeUtc();publishUpcoming();}
        });
        connect(&briefingCalendar,&Caldav::failed,this,[this](const QString &){briefingConnecting=false;briefingReady=false;appleUpcomingChecked={};upcomingEpoch={};diagnosticEvent(DiagnosticEvent::UpcomingUnavailable);briefing.calendarUnavailable();});
        connect(&briefingCalendar,&Caldav::authorizationRejected,this,[this]{briefingBlocked=true;briefing.calendarUnavailable(true);});
        server.setSocketOptions(QLocalServer::UserAccessOption|QLocalServer::GroupAccessOption);
        connect(&server,&QLocalServer::newConnection,this,[this]{while(server.hasPendingConnections())accept(server.nextPendingConnection());});
        QLocalServer::removeServer("/run/porthole-calendar/socket");
        if(!server.listen("/run/porthole-calendar/socket"))QTimer::singleShot(0,qApp,[]{QCoreApplication::exit(1);});
        setupGoogle();
        auto *poll=new QTimer(this);connect(poll,&QTimer::timeout,this,[this]{if(authenticated)fetch();else reconnect();if(googleCredentials)google.discover();refreshUpcoming();});poll->start(60000);
        QFile credential(qEnvironmentVariable("CREDENTIALS_DIRECTORY")+"/porthole-icloud");
        if(credential.open(QIODevice::ReadOnly)&&credential.size()<=16384){
            auto data=credential.readAll();const auto value=QJsonDocument::fromJson(data).object();data.fill('\0');
            account=value["account"].toString();password=value["password"].toString().toLatin1();
            if(validAccount(account,password)){
                for(const auto &item:value["calendars"].toArray()){
                    const QUrl url(item.toObject()["url"].toString());if(permittedCalendarUrl(url)&&restoreUrls.size()<16)restoreUrls<<url.toString();
                }
                remember=remembered=true;QTimer::singleShot(0,this,[this]{reconnect();});
            }else {account.clear();password.fill('\0');password.clear();}
        }
        if(account.isEmpty())vault({{"type","status"}},[](bool){},[this](const QJsonObject &reply){
            if(account.isEmpty()&&!connecting&&!authenticated&&reply["ok"].toBool()&&reply["remembered"].toBool()){
                remembered=true;status="Saved sign-in could not be loaded · replace password";
            }
        });
    }
private:
    void publishUpcoming(){
        CalendarSource apple=CalendarNotConfigured{},g=CalendarNotConfigured{};
        if(!account.isEmpty()||remembered){apple=CalendarUnavailable{};if(!selected.isEmpty()&&appleUpcomingChecked.isValid())apple=CheckedCalendarData{appleUpcomingEvents,appleUpcomingChecked};}
        if(googleCredentials||!googleSelected.isEmpty()){g=CalendarUnavailable{};if(!googleSelected.isEmpty()&&googleUpcomingChecked.isValid())g=CheckedCalendarData{googleUpcomingEvents,googleUpcomingChecked};}
        if(const auto merged=combineCalendarSources(apple,g,upcomingEpoch)){upcomingEpoch={};diagnosticEvent(DiagnosticEvent::UpcomingReady);briefing.calendarData(merged->events,merged->checked);}
    }
    void resetUpcoming(bool forget){upcomingEpoch={};appleUpcomingEvents.clear();appleUpcomingChecked={};briefingCalendar.disconnectAccount();briefingReady=briefingConnecting=briefingBlocked=false;briefing.calendarUnavailable(forget);}
    void refreshUpcoming(){
        const auto state=briefing.snapshot();
        if(!state["enabled"].toBool()||!state["hasKey"].toBool())return;
        if(!upcomingEpoch.isValid())upcomingEpoch=QDateTime::currentDateTimeUtc();
        if(googleAuthenticated&&!googleSelected.isEmpty())googleUpcoming.fetch(googleSelected,QDate(today().year(),today().month(),1));
        if(!authenticated||selected.isEmpty()||briefingBlocked){publishUpcoming();return;}
        if(!briefingReady){if(!briefingConnecting){briefingConnecting=true;briefingCalendar.connectAccount(account,password);}return;}
        briefingCalendar.fetch(selected,QDate(today().year(),today().month(),1));
    }
    void setupGoogle(){
        connect(&google,&GoogleCalendar::calendarsReady,this,[this](const QList<Calendar> &items){
            const auto previousSelection=googleSelected;googleAvailable=items;googleAuthenticated=true;googleIssue.clear();
            QStringList urls;
            if(googleListLoaded){for(const auto &calendar:previousSelection)urls<<calendar.url.toString();}
            else {
                QFile file("/var/lib/porthole-calendar/google-selection.json");
                if(file.exists()){
                    if(!file.open(QIODevice::ReadOnly)||file.size()>32768){googleStatus="Google calendar selection could not be read";googleIssue="storage";return;}
                    QJsonParseError error;const auto document=QJsonDocument::fromJson(file.readAll(),&error);
                    if(error.error!=QJsonParseError::NoError||!document.isArray()){googleStatus="Google calendar selection could not be read";googleIssue="storage";return;}
                    for(const auto &value:document.array()){
                        if(!value.isString()||urls.size()>=16){googleStatus="Google calendar selection could not be read";googleIssue="storage";return;}
                        urls<<value.toString();
                    }
                }
            }
            googleSelected.clear();for(const auto &c:items)if(urls.contains(c.url.toString())&&googleSelected.size()<16)googleSelected<<c;
            if(!googleListLoaded&&urls.isEmpty()&&!items.isEmpty()){googleSelected<<items.first();if(!saveGoogleSelection()){googleStatus="Google calendar selection could not be saved";googleIssue="storage";googleSelected.clear();return;}}
            const bool selectionChanged=googleListLoaded&&!sameCalendarSelection(previousSelection,googleSelected);googleListLoaded=true;
            if(selectionChanged){google.cancelFetch();googleUpcoming.cancelFetch();upcomingEpoch={};googleEvents.clear();googleUpcomingEvents.clear();googleUpcomingChecked={};++eventsRevision;briefing.calendarUnavailable(true);}
            googleStatus=googleSelected.isEmpty()?"Choose your Google calendars":"Google connected";fetchGoogle();refreshUpcoming();
        });
        connect(&google,&GoogleCalendar::eventsReady,this,[this](const QList<Event> &items,QDate loaded){if(loaded!=month)return;googleEvents=items;++eventsRevision;googleChecked=QDateTime::currentDateTimeUtc();googleIssue.clear();googleStatus="Google · checked "+googleChecked.toTimeZone(configuredZone()).toString("h:mm AP");});
        connect(&google,&GoogleCalendar::failed,this,[this](const QString &message){googleStatus=message;if(googleIssue!="sign-in")googleIssue="refresh";googleUpcomingChecked={};upcomingEpoch={};briefing.calendarUnavailable();});
        connect(&google,&GoogleCalendar::authorizationRejected,this,[this]{googleAuthenticated=googleListLoaded=false;googleCredentials.reset();googleIssue="sign-in";googleUpcoming.disconnectAccount();googleUpcomingChecked={};briefing.calendarUnavailable(true);});
        connect(&googleUpcoming,&GoogleCalendar::calendarsReady,this,[this](const QList<Calendar> &){refreshUpcoming();});
        connect(&googleUpcoming,&GoogleCalendar::eventsReady,this,[this](const QList<Event> &items,QDate loaded){if(loaded==QDate(today().year(),today().month(),1)){googleUpcomingEvents=items;googleUpcomingChecked=QDateTime::currentDateTimeUtc();publishUpcoming();}});
        connect(&googleUpcoming,&GoogleCalendar::failed,this,[this](const QString &){googleUpcomingChecked={};upcomingEpoch={};briefing.calendarUnavailable();});
        connect(&googleUpcoming,&GoogleCalendar::authorizationRejected,this,[this]{google.disconnectAccount();googleCredentials.reset();googleAuthenticated=false;googleIssue="sign-in";googleStatus="Reconnect Google using the Mac helper";googleUpcomingChecked={};briefing.calendarUnavailable(true);});
        QFile file(qEnvironmentVariable("CREDENTIALS_DIRECTORY")+"/porthole-google");
        if(file.open(QIODevice::ReadOnly)&&file.size()<=8192){auto bytes=file.readAll();googleCredentials=GoogleCredentials::parse(QJsonDocument::fromJson(bytes).object());bytes.fill('\0');}
        if(googleCredentials){googleStatus="Connecting Google…";google.connectAccount(*googleCredentials);googleUpcoming.connectAccount(*googleCredentials);}
    }
    bool saveGoogleSelection(){QSaveFile file("/var/lib/porthole-calendar/google-selection.json");if(!file.open(QIODevice::WriteOnly))return false;file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);QJsonArray urls;for(const auto &c:googleSelected)urls.append(c.url.toString());const auto data=QJsonDocument(urls).toJson(QJsonDocument::Compact);return file.write(data)==data.size()&&file.commit();}
    void fetchGoogle(){if(googleAuthenticated&&!googleSelected.isEmpty())google.fetch(googleSelected,month);}
    void reconnect(){if(account.isEmpty()||blocked||connecting)return;connecting=true;status="Connecting iCloud…";caldav.connectAccount(account,password);}
    void fetch(){if(authenticated&&!selected.isEmpty())caldav.fetch(selected,month);}
    QJsonObject snapshot(qint64 since){
        auto ai=briefing.snapshot();if(!briefingStorageError.isEmpty())ai["status"]=briefingStorageError;
        QJsonObject value{{"ok",true},{"profile",householdProfile().json()},{"instance",instance},{"status",status},{"lastChecked",lastChecked.toString(Qt::ISODate)},{"calendarIssue",calendarIssue},{"authenticated",authenticated},{"remembered",remembered},{"account",account},{"available",jsonCalendars(available)},{"selected",jsonCalendars(selected)},{"needsSelection",needsSelection},{"selectionRevision",selectionRevision},{"eventsRevision",eventsRevision},{"month",month.toString(Qt::ISODate)},{"briefing",ai}};
        value["google"]=QJsonObject{{"authenticated",googleAuthenticated},{"status",googleStatus},{"issue",googleIssue},{"lastChecked",googleChecked.toString(Qt::ISODate)},{"available",jsonCalendars(googleAvailable)},{"selected",jsonCalendars(googleSelected)}};
        const bool hasApple=!selected.isEmpty(),hasGoogle=!googleSelected.isEmpty();
        value["authenticated"]=authenticated||googleAuthenticated;
        value["calendarIssue"]=!calendarIssue.isEmpty()?calendarIssue:googleIssue;
        value["lastChecked"]=(hasApple&&hasGoogle?std::min(lastChecked,googleChecked):hasGoogle?googleChecked:lastChecked).toString(Qt::ISODate);
        value["status"]=hasGoogle?(hasApple?status+" · ":QString())+googleStatus:status;
        value["selected"]=jsonCalendars(selected+googleSelected);
        value["icloudAuthenticated"]=authenticated;value["icloudStatus"]=status;
        if(since!=eventsRevision){QJsonArray data;for(const auto &e:mergeCalendarEvents(events,googleEvents))data.append(calendarEventJson(e));value["events"]=data;}
        return value;
    }
    void vault(QJsonObject command,std::function<void(bool)> done,std::function<void(QJsonObject)> details={}){
        auto *socket=new QLocalSocket(this);auto state=QSharedPointer<PendingRead>::create();auto *timer=new QTimer(socket);timer->setSingleShot(true);timer->start(20000);
        auto finish=[socket,state,done](bool ok){if(state->finished)return;state->finished=true;state->bytes.fill('\0');state->bytes.clear();socket->abort();socket->deleteLater();done(ok);};
        connect(timer,&QTimer::timeout,this,[finish]{finish(false);});connect(socket,&QLocalSocket::errorOccurred,this,[finish](auto){finish(false);});
        connect(socket,&QLocalSocket::connected,this,[socket,command]{socket->write(QJsonDocument(command).toJson(QJsonDocument::Compact)+'\n');});
        connect(socket,&QLocalSocket::readyRead,this,[socket,state,finish,details]{if(state->finished)return;state->bytes.append(socket->readAll());if(state->bytes.size()>1024){finish(false);return;}if(state->bytes.contains('\n')){const auto reply=QJsonDocument::fromJson(state->bytes).object();if(details)details(reply);finish(reply["ok"].toBool());}});
        socket->connectToServer("/run/porthole-credentials/socket");
    }
    void accept(QLocalSocket *socket){
        struct ucred peer{};socklen_t length=sizeof(peer);
        if(getsockopt(socket->socketDescriptor(),SOL_SOCKET,SO_PEERCRED,&peer,&length)!=0||peer.uid!=allowedUid){socket->abort();socket->deleteLater();return;}
        auto buffer=QSharedPointer<QByteArray>::create();auto *timeout=new QTimer(socket);timeout->setSingleShot(true);timeout->start(5000);
        connect(timeout,&QTimer::timeout,socket,&QLocalSocket::abort);
        connect(socket,&QLocalSocket::disconnected,this,[socket,buffer]{buffer->fill('\0');buffer->clear();socket->deleteLater();});
        connect(socket,&QLocalSocket::readyRead,this,[this,socket,buffer]{
            if(socket->property("handled").toBool())return;
            buffer->append(socket->readAll());if(buffer->size()>16384){socket->abort();return;}const int end=buffer->indexOf('\n');if(end<0)return;socket->setProperty("handled",true);
            const auto request=QJsonDocument::fromJson(buffer->left(end)).object();buffer->fill('\0');buffer->clear();QJsonObject reply{{"ok",true}};
            const auto kind=request["type"].toString();
            if(kind=="ping")reply={{"ok",true},{"instance",instance}};
            else if(kind=="status")reply=snapshot(request["eventsRevision"].toInteger(-1));
            else if(kind=="graphicsDiagnostics"){
                if(!graphicsWindow.isValid()||graphicsWindow.elapsed()>=60000){graphicsWindow.start();graphicsPackets=0;}
                const auto sample=parseGraphicsDiagnosticRequest(request);const bool accepted=sample&&graphicsPackets<240;
                if(accepted){++graphicsPackets;writeGraphicsDiagnostic(sample->metric,sample->value);}
                reply={{"ok",accepted}};
            }
            else if(kind=="eventDetails")reply=calendarEventDetailsJson(events+googleEvents,request["reference"].toString());
            else if(kind=="login"){
                const auto email=request["account"].toString();const auto secret=request["password"].toString().toLatin1();
                if(!validAccount(email,secret))reply={{"error","Use your Apple Account email and dedicated app-specific password."}};
                else {resetUpcoming(true);lastChecked={};calendarIssue.clear();caldav.disconnectAccount();account=email;password.fill('\0');password=secret;remember=request["remember"].toBool();blocked=authenticated=connecting=needsSelection=false;selected.clear();available.clear();events.clear();restoreUrls.clear();++eventsRevision;reconnect();}
            }else if(kind=="select"){
                QList<Calendar> chosen;for(const auto &url:request["urls"].toArray())for(const auto &c:available)if(c.url.toString()==url.toString()&&!std::any_of(chosen.begin(),chosen.end(),[&](const Calendar &v){return v.url==c.url;}))chosen<<c;
                if(!authenticated||chosen.isEmpty()||chosen.size()>16||storing)reply={{"error","Choose between 1 and 16 available calendars."}};
                else {resetUpcoming(true);caldav.cancelFetch();selected=chosen;needsSelection=false;storing=true;status="Saving account…";
                    QJsonObject command=remember?QJsonObject{{"type","save"},{"bundle",QJsonObject{{"account",account},{"password",QString::fromLatin1(password)},{"calendars",calendarSelection(selected)}}}}:QJsonObject{{"type","clear"}};
                    vault(command,[this](bool ok){storing=false;remembered=ok&&remember;status=ok?remember?"Account saved · reconnects automatically":"Connected for this device session":"Connected · account could not be saved";fetch();refreshUpcoming();});
                }
            }else if(kind=="googleSelect"){
                QList<Calendar> chosen;for(const auto &v:request["urls"].toArray())for(const auto &c:googleAvailable)if(v.toString()==c.url.toString()&&!std::any_of(chosen.begin(),chosen.end(),[&](const Calendar &a){return a.url==c.url;}))chosen<<c;
                if(!googleAuthenticated||chosen.isEmpty()||chosen.size()>16)reply={{"error","Choose between 1 and 16 Google calendars."}};
                else {const auto old=googleSelected;googleSelected=chosen;if(!saveGoogleSelection()){googleSelected=old;reply={{"error","Google calendar selection could not be saved."}};}else {google.cancelFetch();googleUpcoming.cancelFetch();upcomingEpoch={};googleEvents.clear();googleUpcomingEvents.clear();googleUpcomingChecked={};++eventsRevision;briefing.calendarUnavailable(true);fetchGoogle();refreshUpcoming();}}
            }else if(kind=="googleForget"){
                google.disconnectAccount();googleUpcoming.disconnectAccount();googleCredentials.reset();googleAuthenticated=googleListLoaded=false;googleAvailable.clear();googleSelected.clear();googleEvents.clear();googleUpcomingEvents.clear();googleUpcomingChecked={};googleChecked={};googleIssue.clear();++eventsRevision;briefing.calendarUnavailable(true);QFile::remove("/var/lib/porthole-calendar/google-selection.json");
                vault({{"type","forgetGoogle"}},[this](bool ok){googleStatus=ok?"Google account forgotten":"Google storage could not be removed. Try Forget again.";});
            }else if(kind=="fetch"){
                const auto next=QDate::fromString(request["month"].toString(),Qt::ISODate);
                if(next.isValid()&&next.year()>=2000&&next.year()<=2099){if(month!=next){month=QDate(next.year(),next.month(),1);google.cancelFetch();events.clear();googleEvents.clear();++eventsRevision;}if(authenticated)fetch();else reconnect();if(googleCredentials){if(googleAuthenticated)fetchGoogle();else google.discover();}refreshUpcoming();}
                else reply={{"error","Invalid calendar month."}};
            }else if(kind=="forget"){
                resetUpcoming(true);lastChecked={};calendarIssue.clear();caldav.disconnectAccount();account.clear();password.fill('\0');password.clear();selected.clear();available.clear();events.clear();restoreUrls.clear();remembered=authenticated=needsSelection=connecting=false;blocked=true;++eventsRevision;status="Forgetting account…";
                vault({{"type","forget"}},[this](bool ok){status=ok?"iCloud account forgotten":"Account storage could not be removed; try Forget again";});
            }else if(kind=="profileSettings"){
                const auto parsed=HouseholdProfile::parse(request["profile"].toObject());
                if(const auto *error=std::get_if<ProfileError>(&parsed))reply={{"type","profile"},{"error",profileErrorMessage(*error)}};
                else if(const auto *profile=std::get_if<HouseholdProfile>(&parsed)){
                    if(!saveProfile("/var/lib/porthole-calendar/profile.json",*profile))reply={{"type","profile"},{"error","Private settings could not be saved. Previous settings remain active."}};
                    else if(!(householdProfile()==*profile)){
                        const bool zoneChanged=profile->zone()!=configuredZone();setHouseholdProfile(*profile);resetUpcoming(false);briefing.profileChanged();google.cancelFetch();googleUpcoming.cancelFetch();googleUpcomingChecked={};googleUpcomingEvents.clear();googleEvents.clear();++eventsRevision;
                        if(zoneChanged){caldav.disconnectAccount();restoreUrls.clear();for(const auto &c:selected)restoreUrls<<c.url.toString();selected.clear();events.clear();++eventsRevision;month=QDate(today().year(),today().month(),1);authenticated=connecting=false;reconnect();}
                        else refreshUpcoming();
                        fetchGoogle();refreshUpcoming();
                    }
                }
            }else if(kind=="briefingWeather"){
                const auto value=request["weather"].toObject();if(value["profileRevision"].toString()==profileRevision(householdProfile()))briefing.weatherData(value);
            }
            else if(kind=="briefingSettings"){
                briefing.configure(request["enabled"].toBool(),request["priorities"].toString(),request["budgetCents"].toInt(100));
                if(!request["enabled"].toBool()){briefingCalendar.disconnectAccount();briefingReady=briefingConnecting=false;}
                else refreshUpcoming();
            }else if(kind=="briefingForget"){resetUpcoming(true);briefing.forgetKey();briefingStorageError.clear();vault({{"type","forgetOpenai"}},[this](bool ok){if(!ok)briefingStorageError="API key could not be removed · try Forget API key again";});}
            else if(kind=="briefingRefresh"){refreshUpcoming();briefing.refresh(true);}
            else reply={{"error","Unknown calendar operation."}};
            socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });
    }
};
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(argc==3&&QString::fromLocal8Bit(argv[1])=="--check-profile"){const auto path=QString::fromLocal8Bit(argv[2]);if(!QFile::exists(path))return 2;const auto check=loadProfile(path);const bool valid=std::holds_alternative<HouseholdProfile>(check);QTextStream(stdout)<<"Profile valid: "<<valid<<"\n";return valid?0:2;}const auto parsed=loadProfile("/var/lib/porthole-calendar/profile.json");if(const auto *profile=std::get_if<HouseholdProfile>(&parsed))setHouseholdProfile(*profile);else return 2;auto *user=getpwnam("porthole");if(!user)return 1;if(argc==2&&QString::fromLocal8Bit(argv[1])=="--self-check"){QTextStream(stdout)<<"Configured profile valid: 1\n";return 0;}CalendarService service(user->pw_uid);
    writeGraphicsDiagnostic(GraphicsMetric::BackendProtocol,2);
    QTimer diagnosticsHeartbeat;QObject::connect(&diagnosticsHeartbeat,&QTimer::timeout,&app,[]{writeGraphicsDiagnostic(GraphicsMetric::BackendProtocol,2);});diagnosticsHeartbeat.start(15*60*1000);
    return app.exec();}
