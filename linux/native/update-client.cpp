#include "update-client.h"
#include <QJsonDocument>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QTimer>
namespace {
struct Read {QByteArray bytes;bool finished=false;};
}
std::optional<SoftwareUpdate> parseSoftwareUpdate(const QJsonObject &v){
    if(v["ok"]!=QJsonValue(true)||!v["automatic"].isBool()||!v["version"].isString()||!v["checked"].isString())return std::nullopt;
    const auto version=v["version"].toString();if(version!="local"&&!QRegularExpression("^[0-9]{1,4}(\\.[0-9]{1,4}){2,3}$").match(version).hasMatch())return std::nullopt;
    std::optional<UpdatePhase> phase;const auto p=v["phase"].toString();
    if(p=="idle")phase=UpdatePhase::Idle;else if(p=="checking")phase=UpdatePhase::Checking;else if(p=="up-to-date")phase=UpdatePhase::Current;else if(p=="installing")phase=UpdatePhase::Installing;else if(p=="installed")phase=UpdatePhase::Installed;else if(p=="rolled-back")phase=UpdatePhase::RolledBack;else if(p=="blocked")phase=UpdatePhase::Blocked;else if(p=="unavailable")phase=UpdatePhase::Unavailable;
    std::optional<UpdateFailure> failure;const auto f=v["reason"].toString();
    if(f.isEmpty())failure=UpdateFailure::None;else if(f=="network")failure=UpdateFailure::Network;else if(f=="signature")failure=UpdateFailure::Signature;else if(f=="archive")failure=UpdateFailure::Archive;else if(f=="platform")failure=UpdateFailure::Platform;else if(f=="storage")failure=UpdateFailure::Storage;else if(f=="health")failure=UpdateFailure::Health;else if(f=="rollback")failure=UpdateFailure::Rollback;
    const auto checked=QDateTime::fromString(v["checked"].toString(),Qt::ISODate);if(!v["checked"].toString().isEmpty()&&!checked.isValid())return std::nullopt;
    if(!phase||!failure)return std::nullopt;
    return SoftwareUpdate{*phase,*failure,v["automatic"].toBool(),version,checked};
}
QString updatePhaseText(UpdatePhase phase){switch(phase){case UpdatePhase::Idle:return "Ready to check";case UpdatePhase::Checking:return "Checking for updates…";case UpdatePhase::Current:return "Up to date";case UpdatePhase::Installing:return "Installing update…";case UpdatePhase::Installed:return "Update installed";case UpdatePhase::RolledBack:return "Previous version restored";case UpdatePhase::Blocked:return "Update held back";case UpdatePhase::Unavailable:return "Update check unavailable";}return {};}
QString updateFailureText(UpdateFailure failure){switch(failure){case UpdateFailure::None:return {};case UpdateFailure::Network:return "Check Wi-Fi. Automatic checks will retry.";case UpdateFailure::Signature:return "The release signature could not be verified.";case UpdateFailure::Archive:return "The release files did not pass verification.";case UpdateFailure::Platform:return "This release does not match the installed platform.";case UpdateFailure::Storage:return "The SD storage needs attention.";case UpdateFailure::Health:return "The update failed its startup check.";case UpdateFailure::Rollback:return "Recovery needs maintenance.";}return {};}
UpdateClient::UpdateClient(QObject *parent,bool sample,QString path):QObject(parent),preview(sample),endpoint(std::move(path)){
    if(preview){latest=SoftwareUpdate{UpdatePhase::Current,UpdateFailure::None,false,"local",{}};return;}
    auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,&UpdateClient::status);timer->start(5000);QTimer::singleShot(0,this,&UpdateClient::status);
}
void UpdateClient::send(QJsonObject request,bool poll){
    if(preview)return;
    if(poll&&polling)return;
    if(poll)polling=true;
    auto *socket=new QLocalSocket(this);auto read=QSharedPointer<Read>::create();auto *timer=new QTimer(socket);timer->setSingleShot(true);timer->start(4000);
    auto finish=[this,socket,read,poll](std::optional<SoftwareUpdate> state){if(read->finished)return;read->finished=true;if(poll)polling=false;latest=std::move(state);socket->abort();socket->deleteLater();emit changed();};
    connect(timer,&QTimer::timeout,this,[finish]{finish(std::nullopt);});connect(socket,&QLocalSocket::errorOccurred,this,[finish](auto){finish(std::nullopt);});
    connect(socket,&QLocalSocket::connected,this,[socket,request]{socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');});
    connect(socket,&QLocalSocket::readyRead,this,[socket,read,finish]{
        read->bytes+=socket->readAll();if(read->bytes.size()>8192){finish(std::nullopt);return;}const auto line=read->bytes.indexOf('\n');if(line<0)return;QJsonParseError error;const auto v=QJsonDocument::fromJson(read->bytes.left(line),&error);finish(error.error==QJsonParseError::NoError&&v.isObject()?parseSoftwareUpdate(v.object()):std::nullopt);
    });socket->connectToServer(endpoint);
}
void UpdateClient::status(){send({{"type","status"}},true);}
void UpdateClient::check(){send({{"type","check"}});}
void UpdateClient::automatic(bool enabled){send({{"type","automatic"},{"enabled",enabled}});}
