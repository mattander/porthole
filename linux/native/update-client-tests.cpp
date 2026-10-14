#include "update-client.h"
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QtTest>
namespace {
QJsonObject state(){return {{"ok",true},{"automatic",true},{"version","2026.10.14.1"},{"phase","up-to-date"},{"reason",""},{"checked","2026-10-14T21:00:00+00:00"}};}
}
class UpdateTests:public QObject {
    Q_OBJECT
private slots:
    void parsesOnlySafeStates(){
        const auto parsed=parseSoftwareUpdate(state());QVERIFY(parsed);QCOMPARE(parsed->phase,UpdatePhase::Current);QVERIFY(parsed->automatic);QVERIFY(parsed->checked.isValid());
        for(const auto field:{QString("version"),QString("phase"),QString("reason"),QString("checked")}){auto v=state();v[field]="private text\nsecret";QVERIFY(!parseSoftwareUpdate(v));}
        auto v=state();v["automatic"]="true";QVERIFY(!parseSoftwareUpdate(v));v=state();v["ok"]=false;QVERIFY(!parseSoftwareUpdate(v));
        for(const auto phase:{UpdatePhase::Idle,UpdatePhase::Checking,UpdatePhase::Current,UpdatePhase::Installing,UpdatePhase::Installed,UpdatePhase::RolledBack,UpdatePhase::Blocked,UpdatePhase::Unavailable})QVERIFY(!updatePhaseText(phase).isEmpty());
    }
    void usesOnlyFixedLocalOperations(){
        QTemporaryDir directory;const auto path=directory.filePath("socket");QLocalServer server;QVERIFY(server.listen(path));QList<QJsonObject> requests;
        connect(&server,&QLocalServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{const auto request=QJsonDocument::fromJson(socket->readAll().trimmed()).object();requests<<request;socket->write(QJsonDocument(state()).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();});connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        }});
        UpdateClient client(nullptr,false,path);QTRY_VERIFY(client.state());client.check();client.automatic(false);QTRY_VERIFY(requests.size()>=3);
        QCOMPARE(requests[0],QJsonObject({{"type","status"}}));QVERIFY(requests.contains(QJsonObject{{"type","check"}}));QVERIFY(requests.contains(QJsonObject{{"type","automatic"},{"enabled",false}}));
    }
};
QTEST_GUILESS_MAIN(UpdateTests)
#include "update-client-tests.moc"
