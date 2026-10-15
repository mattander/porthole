#include "rain-forecast.h"
#include "profile.h"
#include "typography.h"
#include <QJsonArray>
#include <QPainter>
#include <QTimeZone>
#include <cmath>

std::optional<RainForecast> RainForecast::parse(const QJsonObject &value,QDateTime checked){
    const auto hourly=value["hourly"].toObject(),units=value["hourly_units"].toObject();
    if(!checked.isValid()||units["time"].toString()!="unixtime"||units["precipitation_probability"].toString()!="%"||!hourly["time"].isArray()||!hourly["precipitation_probability"].isArray())return std::nullopt;
    const auto times=hourly["time"].toArray(),probabilities=hourly["precipitation_probability"].toArray();
    if(times.size()<6||times.size()>24||times.size()!=probabilities.size())return std::nullopt;
    QList<RainHour> hours;
    for(qsizetype index=0;index<times.size();++index){
        const auto seconds=times[index].toInteger(-1);const auto chance=probabilities[index].toDouble(-1);
        if(!times[index].isDouble()||times[index].toDouble()!=seconds||seconds<0||!probabilities[index].isDouble()||!std::isfinite(chance)||chance<0||chance>100||std::floor(chance)!=chance)return std::nullopt;
        const auto end=QDateTime::fromSecsSinceEpoch(seconds,QTimeZone::utc());
        if(!end.isValid()||checked.secsTo(end)<-3600||checked.secsTo(end)>86400)return std::nullopt;
        if(!hours.isEmpty()&&hours.last().end.secsTo(end)!=3600)return std::nullopt;
        hours.append({end,probabilities[index].toInt()});
    }
    RainForecast forecast(std::move(hours),checked);if(forecast.nextHours(checked).size()!=6)return std::nullopt;return forecast;
}
QList<RainHour> RainForecast::nextHours(QDateTime at)const{
    if(!at.isValid()||checked.secsTo(at)<-60||checked.secsTo(at)>3600)return {};
    QList<RainHour> next;
    for(const auto &hour:hours)if(hour.end>at){
        if(next.isEmpty()&&at.secsTo(hour.end)>3600)return {};
        next.append(hour);if(next.size()==6)return next;
    }
    return {};
}
RainStrip::RainStrip(QWidget *parent):QWidget(parent){setObjectName("rainStrip");setFixedHeight(88);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);describe();}
void RainStrip::setForecast(const RainForecast &value,QDateTime at){forecast=value;current=at;failed=false;describe();update();}
void RainStrip::setTime(QDateTime at){if(current.isValid()&&at.isValid()&&current.toSecsSinceEpoch()/60==at.toSecsSinceEpoch()/60)return;current=at;describe();update();}
void RainStrip::setStale(bool value){failed=value;describe();update();}
void RainStrip::clear(){forecast.reset();failed=false;describe();update();}
void RainStrip::describe(){
    const auto hours=forecast?forecast->nextHours(current):QList<RainHour>{};QStringList values;
    for(const auto &hour:hours)values.append(hour.end.addSecs(-3600).toTimeZone(configuredZone()).toString("h:mm AP t")+" – "+hour.end.toTimeZone(configuredZone()).toString("h:mm AP t")+": "+QString::number(hour.chance)+"%");
    setAccessibleName("Rain and snow chance · next six hours");setAccessibleDescription(hours.isEmpty()?(!forecast&&!failed?"Loading hourly forecast…":"Hourly forecast unavailable"):(failed?"Saved forecast. ":QString())+values.join("; "));
    setToolTip(accessibleDescription());
}
void RainStrip::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const auto foreground=palette().color(QPalette::WindowText);
    const bool dark=palette().color(QPalette::Window).lightness()<128;const QColor blue=dark?QColor("#66b8f0"):QColor("#2875af"),track=dark?QColor("#34414f"):QColor("#dce4e7");
    p.setPen(foreground);p.setFont(interfaceFont(14));const auto hours=forecast?forecast->nextHours(current):QList<RainHour>{};
    p.drawText(QRectF(0,0,width(),20),Qt::AlignLeft,QString("Rain / snow chance · next 6 hours")+(failed&&!hours.isEmpty()?" · saved":""));
    if(hours.isEmpty()){p.setFont(interfaceFont(16));p.drawText(QRectF(0,30,width(),40),Qt::AlignLeft|Qt::AlignVCenter,!forecast&&!failed?"Loading hourly forecast…":"Hourly forecast unavailable");return;}
    const qreal column=width()/6.0;
    for(qsizetype index=0;index<hours.size();++index){
        const qreal x=index*column;const auto &hour=hours[index];const QRectF bar(x+6,27,column-12,22);
        p.setPen(Qt::NoPen);p.setBrush(track);p.drawRoundedRect(bar,3,3);
        if(hour.chance){p.setBrush(blue);p.drawRoundedRect(QRectF(bar.left(),bar.bottom()-bar.height()*hour.chance/100.0,bar.width(),bar.height()*hour.chance/100.0),2,2);}
        p.setPen(foreground);p.setFont(interfaceFont(17));p.drawText(QRectF(x,49,column,22),Qt::AlignCenter,QString::number(hour.chance)+"%");
        p.setFont(interfaceFont(13));p.drawText(QRectF(x,71,column,17),Qt::AlignCenter,index==0?QString("Now"):hour.end.addSecs(-3600).toTimeZone(configuredZone()).toString("h AP"));
    }
}
