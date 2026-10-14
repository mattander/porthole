#pragma once
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <optional>

enum class UpdatePhase {Idle,Checking,Current,Installing,Installed,RolledBack,Blocked,Unavailable};
enum class UpdateFailure {None,Network,Signature,Archive,Platform,Storage,Health,Rollback};
struct SoftwareUpdate {UpdatePhase phase;UpdateFailure failure;bool automatic;QString version;QDateTime checked;};
std::optional<SoftwareUpdate> parseSoftwareUpdate(const QJsonObject &value);
QString updatePhaseText(UpdatePhase phase);
QString updateFailureText(UpdateFailure failure);
class UpdateClient:public QObject {
    Q_OBJECT
public:
    explicit UpdateClient(QObject *parent=nullptr,bool preview=false,QString endpoint="/run/porthole-update/socket");
    void status();void check();void automatic(bool enabled);
    std::optional<SoftwareUpdate> state()const{return latest;}
signals:
    void changed();
private:
    bool preview,polling=false;QString endpoint;std::optional<SoftwareUpdate> latest;
    void send(QJsonObject request,bool poll=false);
};
