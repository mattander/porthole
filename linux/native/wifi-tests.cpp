#include "wifi.h"
#include "keyboard.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVirtualObject>
#include <QDBusMetaType>
#include <QTextStream>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QtTest>

namespace {
const QString NetworkPath="/net/connman/iwd/0/1/test_psk";
class DelayedIwd:public QDBusVirtualObject {
public:
    QString introspect(const QString &) const override {
        // Dynamic QDBusInterface construction would block the UI here.
        QThread::msleep(1000);
        return QStringLiteral("<interface name=\"org.freedesktop.DBus.ObjectManager\"><method name=\"GetManagedObjects\"><arg type=\"a{oa{sa{sv}}}\" direction=\"out\"/></method></interface>");
    }
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &connection) override {
        if(message.member()=="GetManagedObjects"){
            IwdObjects objects;
            objects[QDBusObjectPath("/net/connman/iwd/0/1")]["net.connman.iwd.Station"]={{"State","disconnected"},{"Scanning",false}};
            objects[QDBusObjectPath(NetworkPath)]["net.connman.iwd.Network"]={{"Name","Test network"},{"Type","psk"},{"Connected",false}};
            QTimer::singleShot(1200,this,[message,connection,objects]{connection.send(message.createReply({QVariant::fromValue(objects)}));});
        }else if(message.member()=="RegisterAgent"){
            agent=message.service();agentPath=qvariant_cast<QDBusObjectPath>(message.arguments().first()).path();
            connection.send(message.createReply());
        }else if(message.member()=="Connect"){
            auto request=QDBusMessage::createMethodCall(agent,agentPath,"net.connman.iwd.Agent","RequestPassphrase");
            request.setArguments({QVariant::fromValue(QDBusObjectPath(NetworkPath))});
            auto *watcher=new QDBusPendingCallWatcher(connection.asyncCall(request),this);
            QObject::connect(watcher,&QDBusPendingCallWatcher::finished,this,[watcher,message,connection]{
                QDBusPendingReply<QString> reply=*watcher;watcher->deleteLater();
                connection.send(reply.isError()?message.createErrorReply("net.connman.iwd.Failed","Fixture canceled"):message.createReply());
            });
        }else connection.send(message.createReply());
        return true;
    }
private:
    QString agent,agentPath;
};
QPushButton *key(QWidget &widget,const QString &text){for(auto *button:widget.findChildren<QPushButton*>())if(button->text()==text)return button;return nullptr;}
}
class WifiResponsivenessTests:public QObject {
    Q_OBJECT
private slots:
    void delayedNetworkServiceDoesNotFreezeTyping(){
        QElapsedTimer elapsed;elapsed.start();qint64 previous=0,worstGap=0;int pulses=0;
        QTimer heartbeat;connect(&heartbeat,&QTimer::timeout,this,[&]{const auto now=elapsed.elapsed();worstGap=qMax(worstGap,now-previous);previous=now;++pulses;});heartbeat.start(20);
        WifiController wifi;
        WifiPassword typing("Offline test",nullptr);typing.show();
        QTimer taps;int inputs=0;
        connect(&taps,&QTimer::timeout,this,[&]{if(inputs<12){QTest::mouseClick(key(typing,"a"),Qt::LeftButton);++inputs;}});taps.start(60);
        QTRY_COMPARE_WITH_TIMEOUT(wifi.networks().size(),1,4000);
        QTRY_COMPARE_WITH_TIMEOUT(inputs,12,2000);
        QCOMPARE(typing.password->text(),QString(12,'a'));QVERIFY(pulses>20);
        QVERIFY2(worstGap<400,"A delayed network daemon blocked the UI event loop");
        typing.close();
        bool entered=false;int passwordInputs=0;
        QTimer enter;connect(&enter,&QTimer::timeout,this,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog||dialog->windowTitle()!="Wi-Fi password")return;
            if(passwordInputs<10){QTest::mouseClick(key(*dialog,"a"),Qt::LeftButton);++passwordInputs;}
            else {entered=true;dialog->reject();enter.stop();}
        });enter.start(60);
        wifi.connectNetwork(NetworkPath);
        QTRY_VERIFY_WITH_TIMEOUT(entered,4000);QCOMPARE(passwordInputs,10);
        QVERIFY2(worstGap<400,"Password entry froze while the connection awaited its agent reply");
        qInfo()<<"Maximum UI timer gap with delayed iwd fixture (ms):"<<worstGap;
    }
};
int main(int argc,char **argv){
    if(argc>1&&QString::fromLocal8Bit(argv[1])=="--fake-iwd"){
        QCoreApplication app(argc,argv);qDBusRegisterMetaType<IwdInterfaces>();qDBusRegisterMetaType<IwdObjects>();
        auto bus=QDBusConnection::systemBus();DelayedIwd service;
        if(!bus.registerService("net.connman.iwd")||!bus.registerVirtualObject("/",&service,QDBusConnection::SubPath))return 2;
        QTextStream(stdout)<<"READY\n";return app.exec();
    }
    QApplication app(argc,argv);QProcess daemon;
    daemon.start("dbus-daemon",{"--session","--nofork","--print-address=1"});
    if(!daemon.waitForStarted()||!daemon.waitForReadyRead())return 2;
    qputenv("DBUS_SYSTEM_BUS_ADDRESS",daemon.readLine().trimmed());
    QProcess fake;fake.start(QApplication::applicationFilePath(),{"--fake-iwd"});
    if(!fake.waitForStarted()||!fake.waitForReadyRead()||fake.readLine().trimmed()!="READY")return 2;
    WifiResponsivenessTests test;const int result=QTest::qExec(&test,argc,argv);
    fake.terminate();fake.waitForFinished();daemon.terminate();daemon.waitForFinished();return result;
}
#include "wifi-tests.moc"
