#include "profile.h"
#include "caldav.h"
#include "briefing.h"
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
QJsonObject jsonCalendar(const Calendar &c){return {{"url",c.url.toString()},{"name",c.name}};}
QJsonArray jsonCalendars(const QList<Calendar> &items){QJsonArray values;for(const auto &c:items)values.append(jsonCalendar(c));return values;}
bool validAccount(const QString &account,const QByteArray &password){return account.size()<=254&&account.contains('@')&&!account.contains(':')&&!account.contains('\n')&&!account.contains('\r')&&!account.contains(QChar(0))&&QRegularExpression("^[a-z]{4}(-[a-z]{4}){3}$").match(QString::fromLatin1(password)).hasMatch();}
QByteArray readApiKey(){QFile f(qEnvironmentVariable("CREDENTIALS_DIRECTORY")+"/porthole-openai");if(!f.open(QIODevice::ReadOnly)||f.size()>600)return {};return f.readAll().trimmed();}
QDate today(){return QDateTime::currentDateTimeUtc().toTimeZone(configuredZone()).date();}
}
class CalendarService:public QObject {
    Caldav caldav{this},briefingCalendar{this};QLocalServer server{this};
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
            for(const auto &c:selected)if(!std::any_of(items.begin(),items.end(),[&](const Calendar &v){return c.url==v.url;})){briefingBlocked=true;briefingReady=false;briefing.calendarUnavailable(true);return;}
            refreshUpcoming();
        });
        connect(&briefingCalendar,&Caldav::eventsReady,this,[this](const QList<Event> &items,QDate loaded){
            if(loaded==QDate(today().year(),today().month(),1))briefing.calendarData(items,QDateTime::currentDateTimeUtc());
        });
        connect(&briefingCalendar,&Caldav::failed,this,[this](const QString &){briefingConnecting=false;briefingReady=false;briefing.calendarUnavailable();});
        connect(&briefingCalendar,&Caldav::authorizationRejected,this,[this]{briefingBlocked=true;briefing.calendarUnavailable(true);});
        server.setSocketOptions(QLocalServer::UserAccessOption|QLocalServer::GroupAccessOption);
        connect(&server,&QLocalServer::newConnection,this,[this]{while(server.hasPendingConnections())accept(server.nextPendingConnection());});
        QLocalServer::removeServer("/run/porthole-calendar/socket");
        if(!server.listen("/run/porthole-calendar/socket"))QTimer::singleShot(0,qApp,[]{QCoreApplication::exit(1);});
        auto *poll=new QTimer(this);connect(poll,&QTimer::timeout,this,[this]{if(authenticated){fetch();refreshUpcoming();}else reconnect();});poll->start(60000);
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
    void resetUpcoming(bool forget){briefingCalendar.disconnectAccount();briefingReady=briefingConnecting=briefingBlocked=false;briefing.calendarUnavailable(forget);}
    void refreshUpcoming(){
        const auto state=briefing.snapshot();
        if(!state["enabled"].toBool()||!state["hasKey"].toBool()||!authenticated||selected.isEmpty()||briefingBlocked)return;
        if(!briefingReady){if(!briefingConnecting){briefingConnecting=true;briefingCalendar.connectAccount(account,password);}return;}
        briefingCalendar.fetch(selected,QDate(today().year(),today().month(),1));
    }
    void reconnect(){if(account.isEmpty()||blocked||connecting)return;connecting=true;status="Connecting iCloud…";caldav.connectAccount(account,password);}
    void fetch(){if(authenticated&&!selected.isEmpty())caldav.fetch(selected,month);}
    QJsonObject snapshot(qint64 since){
        auto ai=briefing.snapshot();if(!briefingStorageError.isEmpty())ai["status"]=briefingStorageError;
        QJsonObject value{{"ok",true},{"profile",householdProfile().json()},{"instance",instance},{"status",status},{"lastChecked",lastChecked.toString(Qt::ISODate)},{"calendarIssue",calendarIssue},{"authenticated",authenticated},{"remembered",remembered},{"account",account},{"available",jsonCalendars(available)},{"selected",jsonCalendars(selected)},{"needsSelection",needsSelection},{"selectionRevision",selectionRevision},{"eventsRevision",eventsRevision},{"month",month.toString(Qt::ISODate)},{"briefing",ai}};
        if(since!=eventsRevision){QJsonArray data;for(const auto &e:events)data.append(QJsonObject{{"uid",e.uid.left(240)},{"title",e.title},{"location",e.location},{"calendar",e.calendar},{"start",e.start.toString(Qt::ISODate)},{"end",e.end.toString(Qt::ISODate)},{"allDay",e.allDay}});value["events"]=data;}
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
            else if(kind=="login"){
                const auto email=request["account"].toString();const auto secret=request["password"].toString().toLatin1();
                if(!validAccount(email,secret))reply={{"error","Use your Apple Account email and dedicated app-specific password."}};
                else {resetUpcoming(true);lastChecked={};calendarIssue.clear();caldav.disconnectAccount();account=email;password.fill('\0');password=secret;remember=request["remember"].toBool();blocked=authenticated=connecting=needsSelection=false;selected.clear();available.clear();events.clear();restoreUrls.clear();++eventsRevision;reconnect();}
            }else if(kind=="select"){
                QList<Calendar> chosen;for(const auto &url:request["urls"].toArray())for(const auto &c:available)if(c.url.toString()==url.toString()&&!std::any_of(chosen.begin(),chosen.end(),[&](const Calendar &v){return v.url==c.url;}))chosen<<c;
                if(!authenticated||chosen.isEmpty()||chosen.size()>16||storing)reply={{"error","Choose between 1 and 16 available calendars."}};
                else {resetUpcoming(true);caldav.cancelFetch();selected=chosen;needsSelection=false;storing=true;status="Saving account…";
                    QJsonObject command=remember?QJsonObject{{"type","save"},{"bundle",QJsonObject{{"account",account},{"password",QString::fromLatin1(password)},{"calendars",jsonCalendars(selected)}}}}:QJsonObject{{"type","clear"}};
                    vault(command,[this](bool ok){storing=false;remembered=ok&&remember;status=ok?remember?"Account saved · reconnects automatically":"Connected for this device session":"Connected · account could not be saved";fetch();refreshUpcoming();});
                }
            }else if(kind=="fetch"){
                const auto next=QDate::fromString(request["month"].toString(),Qt::ISODate);
                if(next.isValid()&&next.year()>=2000&&next.year()<=2099){if(month!=next){month=QDate(next.year(),next.month(),1);events.clear();++eventsRevision;}if(authenticated){fetch();refreshUpcoming();}else reconnect();}
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
                        const bool zoneChanged=profile->zone()!=configuredZone();setHouseholdProfile(*profile);resetUpcoming(false);briefing.profileChanged();
                        if(zoneChanged){caldav.disconnectAccount();restoreUrls.clear();for(const auto &c:selected)restoreUrls<<c.url.toString();selected.clear();events.clear();++eventsRevision;month=QDate(today().year(),today().month(),1);authenticated=connecting=false;reconnect();}
                        else refreshUpcoming();
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
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(argc==3&&QString::fromLocal8Bit(argv[1])=="--check-profile"){const auto path=QString::fromLocal8Bit(argv[2]);if(!QFile::exists(path))return 2;const auto check=loadProfile(path);const bool valid=std::holds_alternative<HouseholdProfile>(check);QTextStream(stdout)<<"Profile valid: "<<valid<<"\n";return valid?0:2;}const auto parsed=loadProfile("/var/lib/porthole-calendar/profile.json");if(const auto *profile=std::get_if<HouseholdProfile>(&parsed))setHouseholdProfile(*profile);else return 2;auto *user=getpwnam("porthole");if(!user)return 1;if(argc==2&&QString::fromLocal8Bit(argv[1])=="--self-check"){QTextStream(stdout)<<"Configured profile valid: 1\n";return 0;}CalendarService service(user->pw_uid);return app.exec();}
