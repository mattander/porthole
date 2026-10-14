#pragma once
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>
#include <QPointer>
#include <QVariantMap>
#include <QWidget>

using IwdInterfaces=QMap<QString,QVariantMap>;
using IwdObjects=QMap<QDBusObjectPath,IwdInterfaces>;
Q_DECLARE_METATYPE(IwdInterfaces)
Q_DECLARE_METATYPE(IwdObjects)
struct WifiNetwork {QString path,name,type;bool connected=false;};
class WifiPassword;

class WifiController : public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","net.connman.iwd.Agent")
public:
    explicit WifiController(QObject *parent=nullptr,bool preview=false);
    void refresh();
    void scan();
    void connectNetwork(const QString &path);
    void disconnectNetwork();
    QList<WifiNetwork> networks() const{return available;}
    QString status() const{return statusText;}
    bool connected() const{return isConnected;}
    void setDialogParent(QWidget *parent){dialogParent=parent;}
signals:
    void changed();
    void connectionReady();
public slots:
    QString RequestPassphrase(const QDBusObjectPath &network,const QDBusMessage &message);
    void Cancel(const QString &reason);
    void Release();
private:
    QDBusConnection bus=QDBusConnection::systemBus();
    QString station,requestedNetwork,lastError,serviceOwner,statusText="Wi-Fi is starting…";
    QList<WifiNetwork> available;
    QPointer<QWidget> dialogParent;
    QPointer<WifiPassword> passwordDialog;
    quint64 ownerGeneration=0;
    bool registering=false,registered=false,busy=false,isConnected=false,mock=false,refreshing=false;
    void registerAgent();
    void scanPoweredStation(const QString &path);
    void setStatus(const QString &status);
    bool trustedSender(const QDBusMessage &message) const;
};
