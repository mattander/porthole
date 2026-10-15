#include "display-controls.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

namespace {
std::optional<int> integer(const QString &path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};
    const auto bytes=file.read(65);if(bytes.size()>64)return {};
    bool ok=false;const int value=bytes.trimmed().toInt(&ok);if(!ok)return {};return value;
}
QString modeName(AppearanceMode mode){
    switch(mode){
    case AppearanceMode::Manual:return "manual";
    case AppearanceMode::AmbientLight:return "ambient";
    case AppearanceMode::SunSchedule:return "sun";
    }
    Q_UNREACHABLE();
}
}
AppearanceMode appearanceMode(const QSettings &settings){
    if(!settings.contains("display/appearanceMode"))return settings.value("display/followSun",false).toBool()?AppearanceMode::SunSchedule:AppearanceMode::Manual;
    const auto name=settings.value("display/appearanceMode").toString();
    if(name=="ambient")return AppearanceMode::AmbientLight;
    if(name=="sun")return AppearanceMode::SunSchedule;
    return AppearanceMode::Manual;
}
void saveAppearanceMode(QSettings &settings,AppearanceMode mode,bool dark){
    settings.setValue("display/appearanceMode",modeName(mode));settings.setValue("display/darkMode",dark);
    // Keep the legacy flag consistent for an updater rollback to an older app.
    settings.setValue("display/followSun",mode==AppearanceMode::SunSchedule);settings.sync();
}
std::optional<bool> AmbientTheme::observe(std::optional<double> lux,qint64 time,bool dark){
    if(!lux||!std::isfinite(*lux)||*lux<0||*lux>200000||time<0){reset();return {};}
    const std::optional<bool> target=*lux<=15?std::optional<bool>(true):*lux>=40?std::optional<bool>(false):std::nullopt;
    if(!target||*target==dark){reset();return {};}
    if(!candidate||candidate->dark!=*target||time<candidate->last||time-candidate->last>7000){candidate=Candidate{*target,time,time};return {};}
    candidate->last=time;
    if(time-candidate->since<15000)return {};
    reset();return target;
}
int BacklightState::percent()const{return qRound(100.0*level/maximum);}
Backlight::Backlight(QString path):directory(std::move(path)){}
std::optional<BacklightState> Backlight::state()const{
    const auto maximum=integer(directory+"/max_brightness"),level=integer(directory+"/brightness");
    if(!maximum||!level||*maximum<1||*maximum>10000||*level<0||*level>*maximum)return {};
    return BacklightState{*level,*maximum,QFileInfo(directory+"/brightness").isWritable()};
}
std::optional<BacklightState> Backlight::setLevel(int level)const{
    const auto current=state();if(!current||!current->writable||level<1||level>current->maximum)return {};
    QFile file(directory+"/brightness");if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate))return {};
    const auto bytes=QByteArray::number(level)+'\n';if(file.write(bytes)!=bytes.size()||!file.flush())return {};file.close();
    const auto applied=state();if(!applied||applied->level!=level)return {};return applied;
}
std::optional<BacklightState> Backlight::setPercent(int percent)const{
    if(percent<1||percent>100)return {};
    const auto current=state();if(!current)return {};
    return setLevel(std::max(1,qRound(percent*current->maximum/100.0)));
}
AmbientSensor::AmbientSensor(QObject *parent,QString path):QObject(parent),directory(std::move(path)){
    timeout.setSingleShot(true);timeout.setInterval(2000);
    connect(&timeout,&QTimer::timeout,this,[this]{timedOut=true;process.kill();emit unavailable();});
    connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){timeout.stop();emit unavailable();}});
    connect(&process,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){
        timeout.stop();if(timedOut)return;const auto output=process.readAllStandardOutput();bool ok=false;const double lux=output.trimmed().toDouble(&ok);
        if(status!=QProcess::NormalExit||code!=0||output.size()>64||!ok||!std::isfinite(lux)||lux<0||lux>200000){emit unavailable();return;}
        emit measured(lux);
    });
}
void AmbientSensor::sample(){
    if(process.state()!=QProcess::NotRunning)return;
    const auto devices=QDir(directory).entryList({"iio:device*"},QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name);
    int count=0;
    for(const auto &device:devices){
        if(++count>32)break;
        for(const auto *name:{"in_illuminance_input","in_illuminance0_input"}){
            const auto path=directory+'/'+device+'/'+name;
            if(!QFileInfo(path).isReadable())continue;
            timedOut=false;process.setProgram("/usr/bin/cat");process.setArguments({"--",path});process.setStandardErrorFile(QProcess::nullDevice());process.start();timeout.start();return;
        }
    }
    emit unavailable();
}
