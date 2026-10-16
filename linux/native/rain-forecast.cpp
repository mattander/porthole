#include "rain-forecast.h"
#include "profile.h"
#include "typography.h"
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <cmath>

namespace {
std::optional<double> millimetres(const QJsonValue &value){
    const auto amount=value.toDouble(-1);
    if(!value.isDouble()||!std::isfinite(amount)||amount<0||amount>1000)return std::nullopt;
    return amount;
}
std::optional<QDateTime> timestamp(const QJsonValue &value){
    const auto seconds=value.toInteger(-1);
    if(!value.isDouble()||value.toDouble()!=seconds||seconds<0)return std::nullopt;
    const auto time=QDateTime::fromSecsSinceEpoch(seconds,QTimeZone::utc());
    if(!time.isValid())return std::nullopt;
    return time;
}
}
std::optional<RainForecast> RainForecast::parse(const QJsonObject &value,QDateTime checked){
    const auto hourly=value["hourly"].toObject(),units=value["hourly_units"].toObject();
    const auto current=value["current"].toObject(),currentUnits=value["current_units"].toObject();
    if(!checked.isValid()||units["time"].toString()!="unixtime"||units["rain"].toString()!="mm"||units["showers"].toString()!="mm"||
       currentUnits["time"].toString()!="unixtime"||currentUnits["interval"].toString()!="seconds"||currentUnits["rain"].toString()!="mm"||currentUnits["showers"].toString()!="mm"||
       !current["interval"].isDouble()||current["interval"].toDouble()!=900||!hourly["time"].isArray()||!hourly["rain"].isArray()||!hourly["showers"].isArray())return std::nullopt;
    const auto currentTime=timestamp(current["time"]);const auto rain=millimetres(current["rain"]),showers=millimetres(current["showers"]);
    if(!currentTime||!rain||!showers||currentTime->secsTo(checked)<-60||currentTime->secsTo(checked)>1200)return std::nullopt;
    const auto times=hourly["time"].toArray(),amounts=hourly["rain"].toArray(),convective=hourly["showers"].toArray();
    if(times.size()<6||times.size()>24||times.size()!=amounts.size()||times.size()!=convective.size())return std::nullopt;
    QList<RainHour> hours;
    for(qsizetype index=0;index<times.size();++index){
        const auto end=timestamp(times[index]);const auto amount=millimetres(amounts[index]),shower=millimetres(convective[index]);
        if(!end||!amount||!shower||checked.secsTo(*end)<-3600||checked.secsTo(*end)>86400)return std::nullopt;
        if(!hours.isEmpty()&&hours.last().end.secsTo(*end)!=3600)return std::nullopt;
        hours.append({*end,*amount+*shower});
    }
    // Current amounts cover 15 minutes; the curve compares hourly rates.
    RainForecast forecast(std::move(hours),checked,*currentTime,(*rain+*showers)*4);
    if(forecast.nextHours(checked).size()!=6)return std::nullopt;
    return forecast;
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
bool RainForecast::raining(QDateTime at)const{
    return at.isValid()&&rate>0&&checked.secsTo(at)>=-60&&checked.secsTo(at)<=900&&currentAt.secsTo(at)>=-60&&currentAt.secsTo(at)<=1200;
}
RainStrip::RainStrip(QWidget *parent):QWidget(parent){setObjectName("rainStrip");setFixedHeight(80);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);describe();}
void RainStrip::setForecast(const RainForecast &value,QDateTime at){forecast=value;current=at;describe();update();}
void RainStrip::setTime(QDateTime at){if(current.isValid()&&at.isValid()&&current.toSecsSinceEpoch()/60==at.toSecsSinceEpoch()/60)return;current=at;describe();update();}
void RainStrip::clear(){forecast.reset();current={};describe();update();}
void RainStrip::describe(){
    const auto hours=forecast?forecast->nextHours(current):QList<RainHour>{};
    const bool wet=forecast&&forecast->raining(current)&&hours.size()==6;setVisible(wet);
    setAccessibleName("Rain intensity forecast");QStringList values;
    if(wet){
        values.append("Current model estimate: "+QString::number(forecast->currentRate(),'g',3)+" mm per hour");
        for(qsizetype index=1;index<hours.size();++index){const auto &hour=hours[index];values.append(hour.end.addSecs(-3600).toTimeZone(configuredZone()).toString("h:mm AP t")+" – "+hour.end.toTimeZone(configuredZone()).toString("h:mm AP t")+": "+QString::number(hour.amountMm,'g',3)+" mm per hour");}
    }
    setAccessibleDescription(values.join("; "));
}
void RainStrip::paintEvent(QPaintEvent *){
    if(!forecast||!forecast->raining(current))return;
    const auto hours=forecast->nextHours(current);if(hours.size()!=6)return;
    QList<double> amounts{forecast->currentRate()};
    for(qsizetype index=1;index<hours.size();++index)amounts.append(hours[index].amountMm);
    double peak=1;for(const auto amount:amounts)peak=qMax(peak,amount);
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    const bool dark=palette().color(QPalette::Window).lightness()<128;const QColor blue=dark?QColor("#66b8f0"):QColor("#2875af");
    QPainterPath drop;drop.moveTo(12,14);
    drop.cubicTo(10,19,4,25,4,31);drop.cubicTo(4,42,20,42,20,31);
    drop.cubicTo(20,25,14,19,12,14);drop.closeSubpath();p.fillPath(drop,blue);
    const qreal left=28,right=width()-28,baseline=50,amplitude=42;
    const auto span=current.secsTo(hours.last().end.addSecs(-3600));if(span<=0)return;
    QList<qreal> positions{left};
    for(qsizetype index=1;index<hours.size();++index)positions.append(left+(right-left)*current.secsTo(hours[index].end.addSecs(-3600))/span);
    QPainterPath curve;QPointF previous(left,baseline-amplitude*amounts.first()/peak);curve.moveTo(previous);
    for(qsizetype index=1;index<amounts.size();++index){
        const QPointF next(positions[index],baseline-amplitude*amounts[index]/peak);
        const auto step=next.x()-previous.x();
        // Horizontal tangents keep the guide continuous without overshooting
        // the sampled intensities or dipping below the dry baseline.
        curve.cubicTo(previous+QPointF(step/3,0),next-QPointF(step/3,0),next);previous=next;
    }
    auto fill=curve;fill.lineTo(right,baseline);fill.lineTo(left,baseline);fill.closeSubpath();
    auto tint=blue;tint.setAlpha(dark?30:22);p.fillPath(fill,tint);
    p.setPen(QPen(blue,2.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPath(curve);
    p.setPen(palette().color(QPalette::WindowText));p.setFont(interfaceFont(13));
    qreal lastLabelRight=-8;
    for(qsizetype index=0;index<hours.size();++index){
        const auto x=positions[index];if(x-28<lastLabelRight+8)continue;
        p.drawText(QRectF(x-28,55,56,25),Qt::AlignCenter,index==0?QString("Now"):hours[index].end.addSecs(-3600).toTimeZone(configuredZone()).toString("h AP"));lastLabelRight=x+28;
    }
}
