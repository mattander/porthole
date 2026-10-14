#include "wifi.h"
#include "keyboard.h"
#include <QApplication>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QTimer>
#include <algorithm>

namespace {
constexpr auto Service="net.connman.iwd";
constexpr auto AgentPath="/org/porthole/WifiAgent";
QDBusMessage call(const QString &path,const QString &interface,const QString &method,const QVariantList &arguments={}) {
    auto message=QDBusMessage::createMethodCall(Service,path,interface,method);
    message.setArguments(arguments);return message;
}
}
WifiController::WifiController(QObject *parent,bool preview):QObject(parent),mock(preview) {
    qDBusRegisterMetaType<IwdInterfaces>();qDBusRegisterMetaType<IwdObjects>();
    if(!mock){
        bus.registerObject(AgentPath,this,QDBusConnection::ExportAllSlots);
        auto *ownerWatcher=new QDBusServiceWatcher(Service,bus,QDBusServiceWatcher::WatchForOwnerChange,this);
        connect(ownerWatcher,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString &,const QString &,const QString &owner){
            ++ownerGeneration;serviceOwner=owner;registered=false;registering=false;
            if(passwordDialog)passwordDialog->reject();
            requestedNetwork.clear();busy=false;refresh();
        });
        const auto generation=ownerGeneration;
        auto ownerRequest=QDBusMessage::createMethodCall("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","GetNameOwner");
        ownerRequest.setArguments({QString(Service)});
        auto *ownerReply=new QDBusPendingCallWatcher(bus.asyncCall(ownerRequest,3000),this);
        connect(ownerReply,&QDBusPendingCallWatcher::finished,this,[this,ownerReply,generation]{
            QDBusPendingReply<QString> reply=*ownerReply;ownerReply->deleteLater();
            if(generation==ownerGeneration&&!reply.isError()){serviceOwner=reply.value();registerAgent();}
        });
    }
    auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,&WifiController::refresh);timer->start(10000);
    QTimer::singleShot(0,this,&WifiController::refresh);
}
void WifiController::setStatus(const QString &value){statusText=value;emit changed();}
void WifiController::registerAgent() {
    if(registered||registering||mock||serviceOwner.isEmpty())return;
    registering=true;const auto owner=serviceOwner;
    auto *watcher=new QDBusPendingCallWatcher(bus.asyncCall(call("/net/connman/iwd","net.connman.iwd.AgentManager","RegisterAgent",{QVariant::fromValue(QDBusObjectPath(AgentPath))}),3000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher,owner]{
        QDBusPendingReply<> reply=*watcher;watcher->deleteLater();
        if(owner!=serviceOwner)return;
        registering=false;registered=!reply.isError();
        if(reply.isError())setStatus("Wi-Fi control is unavailable. Check the Linux network service.");
    });
}
void WifiController::refresh() {
    if(mock){available={{"/sample/family","Sample family Wi-Fi","psk",false},{"/sample/guest","Sample guest Wi-Fi","psk",false}};setStatus("Preview networks · not real Wi-Fi");return;}
    // A pending Connect request may be waiting for our password dialog. Keep
    // its nested UI event loop free of background discovery and list updates.
    if(refreshing||busy||passwordDialog)return;
    refreshing=true;
    auto *watcher=new QDBusPendingCallWatcher(bus.asyncCall(call("/","org.freedesktop.DBus.ObjectManager","GetManagedObjects"),3000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        QDBusPendingReply<IwdObjects> reply=*watcher;watcher->deleteLater();refreshing=false;
        if(busy||passwordDialog)return;
        if(reply.isError()){registered=false;isConnected=false;available.clear();station.clear();setStatus("Wi-Fi adapter is unavailable.");return;}
        available.clear();station.clear();bool connectedNow=false,scanning=false;QString connectedName;
        const auto all=reply.value();
        for(auto it=all.cbegin();it!=all.cend();++it){
            if(it.value().contains("net.connman.iwd.Station")){
                station=it.key().path();const auto values=it.value().value("net.connman.iwd.Station");
                connectedNow=values.value("State").toString()=="connected";scanning=values.value("Scanning").toBool();
            }
            if(it.value().contains("net.connman.iwd.Network")){
                const auto values=it.value().value("net.connman.iwd.Network");
                WifiNetwork network{it.key().path(),values.value("Name").toString(),values.value("Type").toString(),values.value("Connected").toBool()};
                if(network.connected)connectedName=network.name;
                available<<network;
            }
        }
        std::sort(available.begin(),available.end(),[](const WifiNetwork &a,const WifiNetwork &b){if(a.connected!=b.connected)return a.connected;return a.name.localeAwareCompare(b.name)<0;});
        if(station.isEmpty()){isConnected=false;setStatus("No Wi-Fi adapter was detected.");return;}
        registerAgent();
        const bool justConnected=connectedNow&&!isConnected;isConnected=connectedNow;
        if(connectedNow)lastError.clear();
        if(!busy)setStatus(!lastError.isEmpty()?lastError:connectedNow?"Connected to "+connectedName:scanning?"Scanning nearby networks…":"Choose a Wi-Fi network");else emit changed();
        if(justConnected)emit connectionReady();
    });
}
void WifiController::scan() {
    if(mock){refresh();return;}
    if(station.isEmpty()){refresh();return;}
    lastError.clear();setStatus("Scanning nearby networks…");
    const auto path=station;
    auto *power=new QDBusPendingCallWatcher(bus.asyncCall(call(path,"org.freedesktop.DBus.Properties","Set",{QString("net.connman.iwd.Device"),QString("Powered"),QVariant::fromValue(QDBusVariant(true))}),3000),this);
    connect(power,&QDBusPendingCallWatcher::finished,this,[this,power,path]{
        QDBusPendingReply<> reply=*power;power->deleteLater();
        if(reply.isError()){setStatus("Wi-Fi adapter could not be powered. Try Refresh.");return;}
        scanPoweredStation(path);
    });
}
void WifiController::scanPoweredStation(const QString &path) {
    if(path!=station||busy)return;
    auto *watcher=new QDBusPendingCallWatcher(bus.asyncCall(call(path,"net.connman.iwd.Station","Scan"),5000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        QDBusPendingReply<> reply=*watcher;watcher->deleteLater();
        if(reply.isError()&&reply.error().name()!="net.connman.iwd.Busy")setStatus("Wi-Fi scan failed. Try Refresh.");
        QTimer::singleShot(1500,this,&WifiController::refresh);
    });
}
void WifiController::connectNetwork(const QString &path) {
    if(mock){setStatus("These are preview networks; no connection was made.");return;}
    if(busy||!registered)return;
    const auto it=std::find_if(available.cbegin(),available.cend(),[&](const WifiNetwork &n){return n.path==path;});
    if(it==available.cend())return;
    if(it->type!="psk"&&it->type!="open"){setStatus("Use personal Wi-Fi or a hotspot. Enterprise networks need separate provisioning.");return;}
    lastError.clear();requestedNetwork=path;busy=true;setStatus("Joining "+it->name+"…");
    auto *watcher=new QDBusPendingCallWatcher(bus.asyncCall(call(path,"net.connman.iwd.Network","Connect"),120000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        QDBusPendingReply<> reply=*watcher;watcher->deleteLater();busy=false;requestedNetwork.clear();
        if(reply.isError()){lastError="Connection failed. Check the Wi-Fi password and try again.";setStatus(lastError);}
        refresh();
    });
}
void WifiController::disconnectNetwork() {
    if(station.isEmpty()||mock)return;
    bus.asyncCall(call(station,"net.connman.iwd.Station","Disconnect"),5000);isConnected=false;setStatus("Wi-Fi disconnected");
}
bool WifiController::trustedSender(const QDBusMessage &message) const {
    return !serviceOwner.isEmpty()&&message.service()==serviceOwner;
}
QString WifiController::RequestPassphrase(const QDBusObjectPath &network,const QDBusMessage &request) {
    if(!trustedSender(request)||network.path()!=requestedNetwork||passwordDialog){
        sendErrorReply("net.connman.iwd.Agent.Error.Canceled","No active network selection");return {};
    }
    setDelayedReply(true);
    const auto found=std::find_if(available.cbegin(),available.cend(),[&](const WifiNetwork &n){return n.path==network.path();});
    const QString name=found==available.cend()?QString("Selected Wi-Fi network"):found->name;
    QTimer::singleShot(0,this,[this,request,name,network]{
        if(!busy||network.path()!=requestedNetwork||!trustedSender(request)){bus.send(request.createErrorReply("net.connman.iwd.Agent.Error.Canceled","Selection is no longer active"));return;}
        auto *dialog=new WifiPassword(name,dialogParent?dialogParent.data():QApplication::activeWindow());passwordDialog=dialog;
        connect(dialog,&QDialog::finished,this,[this,dialog,request,network](int result){
            if(result==QDialog::Accepted&&busy&&network.path()==requestedNetwork&&trustedSender(request)){
                QString password=dialog->password->text();dialog->password->clear();
                bus.send(request.createReply(QVariantList{password}));password.fill(QChar(0));
            }else bus.send(request.createErrorReply("net.connman.iwd.Agent.Error.Canceled","Canceled by user"));
            passwordDialog=nullptr;dialog->deleteLater();
        });
        dialog->open();
    });
    return {};
}
void WifiController::Cancel(const QString &) {
    if(!trustedSender(message()))return;
    requestedNetwork.clear();if(passwordDialog)passwordDialog->reject();
}
void WifiController::Release() {
    if(!trustedSender(message()))return;
    registered=false;requestedNetwork.clear();if(passwordDialog)passwordDialog->reject();
}
