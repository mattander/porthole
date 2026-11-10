#include "roux-menu.h"
#include "typography.h"
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimeZone>
#include <QTimer>
#include <QSet>
#include <algorithm>
#include <QVBoxLayout>

namespace {
constexpr int maxBytes=32768;
QString connectionPath(){return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+"/porthole/roux.json";}
bool plain(const QString &text){for(const auto c:text)if(c.category()==QChar::Other_Control||c.category()==QChar::Other_Format)return false;return true;}
QString identity(const RouxConnection &c){return QString::fromLatin1(QCryptographicHash::hash(c.url.toEncoded()+c.token,QCryptographicHash::Sha256).toHex());}
}
std::optional<RouxMenu> parseRouxMenu(const QByteArray &bytes,QDateTime now){
    if(bytes.size()>maxBytes)return std::nullopt;
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())return std::nullopt;
    const auto object=document.object();
    if(!object["updatedAt"].isString()||!object["timezone"].isString()||!object["meals"].isArray())return std::nullopt;
    const auto updated=QDateTime::fromString(object["updatedAt"].toString(),Qt::ISODate);
    const auto zone=object["timezone"].toString();
    if(!updated.isValid()||updated>now.addSecs(300)||!QTimeZone::isTimeZoneIdAvailable(zone.toUtf8()))return std::nullopt;
    const auto meals=object["meals"].toArray();if(meals.size()>14)return std::nullopt;
    RouxMenu result{updated,zone,{}};QSet<QDate> dates;
    for(const auto value:meals){
        if(!value.isObject())return std::nullopt;const auto meal=value.toObject();
        if(!meal["date"].isString()||!meal["title"].isString())return std::nullopt;
        const auto date=QDate::fromString(meal["date"].toString(),Qt::ISODate);const auto title=meal["title"].toString().trimmed();
        if(!date.isValid()||dates.contains(date)||title.isEmpty()||title.size()>200||!plain(title))return std::nullopt;
        dates.insert(date);result.meals.append({date,title});
    }
    std::sort(result.meals.begin(),result.meals.end(),[](const RouxMeal &a,const RouxMeal &b){return a.date<b.date;});return result;
}
QJsonObject rouxMenuJson(const RouxMenu &menu){QJsonArray meals;for(const auto &meal:menu.meals)meals.append(QJsonObject{{"date",meal.date.toString(Qt::ISODate)},{"title",meal.title}});return {{"updatedAt",menu.updatedAt.toString(Qt::ISODate)},{"timezone",menu.timezone},{"meals",meals}};}
std::optional<RouxConnection> loadRouxConnection(const QString &path){
    QFileInfo info(path);if(!info.isFile()||info.isSymLink())return std::nullopt;
    const auto permissions=info.permissions();if(permissions.testFlag(QFileDevice::ReadGroup)||permissions.testFlag(QFileDevice::ReadOther)||permissions.testFlag(QFileDevice::WriteGroup)||permissions.testFlag(QFileDevice::WriteOther))return std::nullopt;
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>4096)return std::nullopt;
    const auto document=QJsonDocument::fromJson(file.readAll());if(!document.isObject())return std::nullopt;
    const auto object=document.object();if(!object["url"].isString()||!object["token"].isString())return std::nullopt;
    const QUrl url(object["url"].toString(),QUrl::StrictMode);const auto token=object["token"].toString().toLatin1();
    if(!url.isValid()||url.scheme()!="https"||url.host().isEmpty()||!url.userInfo().isEmpty()||url.hasQuery()||url.hasFragment()||url.path()!="/v1/porthole/menu"||(url.port()!=-1&&url.port()!=443))return std::nullopt;
    if(token.size()<32||token.size()>256||!QRegularExpression("^[A-Za-z0-9_-]+$").match(QString::fromLatin1(token)).hasMatch())return std::nullopt;
    return RouxConnection{url,token};
}
RouxMenuCard::RouxMenuCard(bool demo,QWidget *parent):QPushButton(parent),demoMode(demo){
    setObjectName("rouxDinnerCard");setFont(interfaceFont(17));setMinimumHeight(74);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setAccessibleName("Tonight's dinner. Open the week's menu.");
    cachePath=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/porthole/roux-menu.json";
    connect(this,&QPushButton::clicked,this,[this]{showWeek();});
    if(!demoMode){connection=loadRouxConnection(connectionPath());if(connection)loadCache();}
    updateDisplay();auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{refresh();});timer->start(15*60*1000);
    if(!demoMode&&connection)QTimer::singleShot(0,this,[this]{refresh();});
}
void RouxMenuCard::setMenuForPreview(const RouxMenu &value){if(!demoMode)return;menu=value;fetchedAt=QDateTime::currentDateTimeUtc();updateDisplay();}
void RouxMenuCard::updateDisplay(){
    setVisible((demoMode&&menu.has_value())||connection.has_value());if(menu&&fetchedAt.isValid()&&fetchedAt.secsTo(QDateTime::currentDateTimeUtc())>7*86400){menu.reset();QFile::remove(cachePath);}
    if(!menu){setText("Tonight's dinner\nRoux menu unavailable");return;}
    const auto today=QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone(menu->timezone.toUtf8())).date();
    QString title="Nothing planned";for(const auto &meal:menu->meals)if(meal.date==today)title=meal.title;
    const bool stale=failed||!fetchedAt.isValid()||fetchedAt.secsTo(QDateTime::currentDateTimeUtc())>1800;
    const auto shortTitle=fontMetrics().elidedText(title,Qt::ElideRight,qMax(180,width()-28));
    setText((demoMode?"Sample dinner":stale?"Tonight's dinner · cached":"Tonight's dinner")+QString("\n")+shortTitle);
    setToolTip(title+(stale?" — last saved menu; may have changed":""));
}
void RouxMenuCard::refresh(){
    if(demoMode||pending)return;
    const auto next=loadRouxConnection(connectionPath());
    if(!next){connection.reset();menu.reset();fetchedAt={};QFile::remove(cachePath);updateDisplay();return;}
    if(!connection||identity(*next)!=identity(*connection)){menu.reset();fetchedAt={};QFile::remove(cachePath);}connection=next;
    QNetworkRequest request(connection->url);request.setRawHeader("Authorization","Bearer "+connection->token);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(15000);
    const auto requestedIdentity=identity(*connection);
    auto *reply=network.get(request);pending=reply;reply->setReadBufferSize(maxBytes+1);
    auto *deadline=new QTimer(reply);deadline->setSingleShot(true);connect(deadline,&QTimer::timeout,reply,[reply]{reply->abort();});deadline->start(15000);
    connect(reply,&QNetworkReply::readyRead,this,[reply]{if(reply->bytesAvailable()>maxBytes)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,requestedIdentity]{
        pending=nullptr;reply->deleteLater();const auto current=loadRouxConnection(connectionPath());if(!current||identity(*current)!=requestedIdentity){connection=current;menu.reset();fetchedAt={};QFile::remove(cachePath);updateDisplay();return;}
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(status==401||status==403){menu.reset();fetchedAt={};QFile::remove(cachePath);failed=true;updateDisplay();return;}
        if(reply->error()!=QNetworkReply::NoError||status!=200||reply->bytesAvailable()>maxBytes){failed=true;updateDisplay();return;}
        const auto parsed=parseRouxMenu(reply->readAll(),QDateTime::currentDateTimeUtc());if(!parsed){failed=true;updateDisplay();return;}
        menu=parsed;fetchedAt=QDateTime::currentDateTimeUtc();failed=false;saveCache();updateDisplay();
    });
}
void RouxMenuCard::loadCache(){
    QFile file(cachePath);if(!connection||!file.open(QIODevice::ReadOnly)||file.size()>maxBytes)return;
    const auto value=QJsonDocument::fromJson(file.readAll()).object();if(value["connection"].toString()!=identity(*connection))return;
    const auto fetched=QDateTime::fromString(value["fetchedAt"].toString(),Qt::ISODate);const auto now=QDateTime::currentDateTimeUtc();
    if(!fetched.isValid()||fetched>now.addSecs(300)||fetched.secsTo(now)>7*86400)return;
    menu=parseRouxMenu(QJsonDocument(value["menu"].toObject()).toJson(),now);if(menu){fetchedAt=fetched;failed=true;}
}
void RouxMenuCard::saveCache(){
    if(!connection||!menu)return;QDir().mkpath(QFileInfo(cachePath).absolutePath());
    QSaveFile file(cachePath);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))return;file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    const auto data=QJsonDocument(QJsonObject{{"connection",identity(*connection)},{"fetchedAt",fetchedAt.toString(Qt::ISODate)},{"menu",rouxMenuJson(*menu)}}).toJson(QJsonDocument::Compact);
    if(file.write(data)==data.size())file.commit();
}
void RouxMenuCard::showWeek(){
    QDialog dialog(this);dialog.setWindowTitle("Roux · this week's dinners");dialog.resize(qMin(620,window()->width()-40),qMin(620,window()->height()-40));auto *layout=new QVBoxLayout(&dialog);
    auto *heading=new QLabel("This week's dinners");heading->setFont(interfaceFont(24));layout->addWidget(heading);
    auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);auto *body=new QWidget;auto *rows=new QVBoxLayout(body);
    if(menu){const auto today=QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone(menu->timezone.toUtf8())).date();const auto first=today.addDays(1-today.dayOfWeek());
        for(int i=0;i<7;++i){const auto date=first.addDays(i);QString title="Nothing planned";for(const auto &meal:menu->meals)if(meal.date==date)title=meal.title;auto *row=new QLabel(date.toString("ddd, MMM d")+"\n"+title);row->setTextFormat(Qt::PlainText);row->setWordWrap(true);row->setFont(interfaceFont(18));rows->addWidget(row);}
        auto *status=new QLabel(demoMode?"Fictional preview menu":(failed||fetchedAt.secsTo(QDateTime::currentDateTimeUtc())>1800?"Cached menu · may have changed. Last fetched ":"Last fetched ")+fetchedAt.toLocalTime().toString("MMM d, h:mm AP"));status->setWordWrap(true);rows->addWidget(status);
    }else rows->addWidget(new QLabel("Connect Roux and plan a dinner to see it here."));
    scroll->setWidget(body);layout->addWidget(scroll);auto *close=new QPushButton("Close");close->setMinimumHeight(48);connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);layout->addWidget(close);dialog.exec();
}
