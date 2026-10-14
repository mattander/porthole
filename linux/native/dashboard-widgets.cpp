#include "profile.h"
#include "dashboard-widgets.h"
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QVBoxLayout>
#include <cmath>

namespace {
QFont pixelFont(int pixels){QFont value("DejaVu Sans");value.setPixelSize(pixels);return value;}
QLabel *text(QString value,int size,bool muted=false){auto *label=new QLabel(value);label->setTextFormat(Qt::PlainText);label->setFont(pixelFont(size));if(muted)label->setProperty("tone","secondary");return label;}
QString checked(QDateTime value){
    if(!value.isValid())return "—";
    const QTimeZone zone=configuredZone();value=value.toTimeZone(zone);
    return value.toString(value.date()==QDateTime::currentDateTimeUtc().toTimeZone(zone).date()?"h:mm AP":"ddd h:mm AP");
}
}
ClockFace::ClockFace(QWidget *parent):QWidget(parent){setObjectName("clockFace");setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setFixedHeight(164);}
void ClockFace::setTime(QTime value){digits=QString("%1:%2").arg(value.hour()%12?value.hour()%12:12).arg(value.minute(),2,10,QChar('0'));period=value.hour()<12?"AM":"PM";setAccessibleName(digits+" "+period);update();}
void ClockFace::setPortrait(bool value){pixels=value?118:160;setFixedHeight(pixels+4);update();}
QFont ClockFace::periodFont()const{return pixelFont(pixels<160?30:35);}
QFont ClockFace::timeFont()const{
    auto result=pixelFont(pixels);result.setLetterSpacing(QFont::AbsoluteSpacing,-6);
    while(result.pixelSize()>60&&QFontMetricsF(result).horizontalAdvance(digits)+QFontMetricsF(periodFont()).horizontalAdvance(period)+22>width())result.setPixelSize(result.pixelSize()-2);
    return result;
}
qreal ClockFace::splitPosition()const{return QFontMetricsF(timeFont()).horizontalAdvance(digits)+18;}
void ClockFace::paintEvent(QPaintEvent *){
    QPainter painter(this);painter.setRenderHint(QPainter::TextAntialiasing);painter.setPen(palette().color(QPalette::WindowText));
    const auto large=timeFont();const auto ink=QFontMetricsF(large).tightBoundingRect(digits);
    const qreal baseline=(height()-ink.height())/2-ink.top();
    painter.setFont(large);painter.drawText(QPointF(0,baseline),digits);
    painter.setFont(periodFont());painter.drawText(QPointF(splitPosition(),baseline),period);
}
bool validWeatherCode(int code){return QList<int>{0,1,2,3,45,48,51,53,55,56,57,61,63,65,66,67,71,73,75,77,80,81,82,85,86,95,96,99}.contains(code);}
QString weatherCondition(int code){
    if(code==0)return "Clear skies";
    if(code==1||code==2)return "Partly cloudy";
    if(code==3)return "Cloudy";
    if(code==45||code==48)return "Fog";
    if(code>=51&&code<=67)return "Rain";
    if(code>=71&&code<=77)return "Snow";
    if(code>=80&&code<=82)return "Rain showers";
    if(code==85||code==86)return "Snow showers";
    if(code>=95&&code<=99)return "Thunderstorms";
    return "Unavailable";
}
WeatherIcon::WeatherIcon(int size,QWidget *parent):QWidget(parent){setFixedSize(size,size);setCode(-1);}
void WeatherIcon::setCode(int value){code=validWeatherCode(value)?value:-1;setAccessibleName(weatherCondition(code));update();}
void WeatherIcon::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.scale(width()/64.0,height()/64.0);
    const bool dark=palette().color(QPalette::Window).lightness()<128;
    const QColor muted=dark?QColor("#a3b3b8"):QColor("#64756e"),sun=dark?QColor("#d7b16b"):QColor("#b68137");
    p.setPen(QPen(muted,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
    auto sunshine=[&](QPointF center,qreal radius){p.setPen(QPen(sun,1.8,Qt::SolidLine,Qt::RoundCap));p.drawEllipse(center,radius,radius);for(int i=0;i<8;++i){const qreal a=i*3.141592653589793/4;p.drawLine(center+QPointF(std::cos(a)*(radius+5),std::sin(a)*(radius+5)),center+QPointF(std::cos(a)*(radius+11),std::sin(a)*(radius+11)));}};
    if(code==0){sunshine({32,32},12);return;}
    if(code==1||code==2)sunshine({24,23},10);
    if(code<0){p.setFont(pixelFont(27));p.drawText(QRectF(0,0,64,64),Qt::AlignCenter,"—");return;}
    const bool precipitation=code>=51;const qreal y=precipitation?0:10;
    QPainterPath cloud;cloud.moveTo(16,36+y);cloud.lineTo(47,36+y);cloud.cubicTo(61,36+y,61,16+y,47,16+y);cloud.cubicTo(45,0+y,24,-1+y,20,14+y);cloud.cubicTo(4,10+y,1,36+y,16,36+y);cloud.closeSubpath();
    p.setPen(QPen(muted,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(palette().color(QPalette::Window));p.drawPath(cloud);p.setBrush(Qt::NoBrush);
    if(code==45||code==48){p.drawLine(11,53,48,53);p.drawLine(18,59,54,59);}
    else if((code>=71&&code<=77)||code==85||code==86){for(int x:{19,34,49}){p.drawLine(x,42,x,55);p.drawLine(x-5,45,x+5,52);p.drawLine(x-5,52,x+5,45);}}
    else if(code>=95){QPainterPath bolt;bolt.moveTo(33,40);bolt.lineTo(25,51);bolt.lineTo(34,51);bolt.lineTo(29,61);p.setPen(QPen(sun,2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPath(bolt);}
    else if(precipitation){for(int x:{20,34,48})p.drawLine(x,43,x-5,54);}
}
ForecastStrip::ForecastStrip(QWidget *parent):QWidget(parent){
    auto *row=new QHBoxLayout(this);row->setContentsMargins(0,0,0,0);row->setSpacing(16);
    for(int i=0;i<3;++i){auto *column=new QVBoxLayout;column->setSpacing(6);dates[i]=text("—",16,true);column->addWidget(dates[i]);
        auto *details=new QHBoxLayout;details->setSpacing(10);icons[i]=new WeatherIcon(40);details->addWidget(icons[i]);auto *copy=new QVBoxLayout;copy->setSpacing(3);
        temperatures[i]=text("— / —",19);conditions[i]=text("Unavailable",13,true);copy->addWidget(temperatures[i]);copy->addWidget(conditions[i]);details->addLayout(copy);details->addStretch();column->addLayout(details);row->addLayout(column,1);
    }
}
void ForecastStrip::setForecast(const QList<ForecastDay> &days){for(int i=0;i<3;++i){const bool present=i<days.size();dates[i]->setText(present?days[i].date.toString("ddd, MMM d"):"—");icons[i]->setCode(present?days[i].code:-1);temperatures[i]->setText(present?QString("%1° / %2°").arg(days[i].high).arg(days[i].low):"— / —");conditions[i]->setText(present?weatherCondition(days[i].code):"Unavailable");}}
GearButton::GearButton(QWidget *parent):QPushButton(parent){setObjectName("settingsButton");setFixedSize(48,48);setFocusPolicy(Qt::NoFocus);setAccessibleName("Settings");setToolTip("Settings");}
void GearButton::setIssueCount(int count){issues=qBound(0,count,99);setAccessibleName(issues?QString("Settings · %1 issues").arg(issues):"Settings");setToolTip(issues?QString("Settings · %1 issues need attention").arg(issues):"Settings");update();}
void GearButton::paintEvent(QPaintEvent *event){
    QPushButton::paintEvent(event);QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.translate(13,13);p.scale(22.0/24,22.0/24);p.setPen(QPen(palette().color(QPalette::ButtonText),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
    QPolygonF points;points<<QPointF(9.5,3)<<QPointF(9.1,5.1)<<QPointF(7.3,6.1)<<QPointF(5.3,5.3)<<QPointF(2.8,9.6)<<QPointF(4.5,10.9)<<QPointF(4.5,13.1)<<QPointF(2.8,14.4)<<QPointF(5.3,18.7)<<QPointF(7.3,17.9)<<QPointF(9.1,18.9)<<QPointF(9.5,21)<<QPointF(14.5,21)<<QPointF(14.9,18.9)<<QPointF(16.7,17.9)<<QPointF(18.7,18.7)<<QPointF(21.2,14.4)<<QPointF(19.5,13.1)<<QPointF(19.5,10.9)<<QPointF(21.2,9.6)<<QPointF(18.7,5.3)<<QPointF(16.7,6.1)<<QPointF(14.9,5.1)<<QPointF(14.5,3);p.drawPolygon(points);p.drawEllipse(QPointF(12,12),3.1,3.1);
    if(issues){p.resetTransform();p.setPen(Qt::NoPen);p.setBrush(QColor("#ad622b"));p.drawEllipse(QRectF(33,2,13,13));p.setPen(Qt::white);p.setFont(pixelFont(9));p.drawText(QRectF(33,2,13,13),Qt::AlignCenter,QString::number(issues));}
}
DashboardFooter::DashboardFooter(QWidget *parent):QWidget(parent){
    setObjectName("dashboardFooter");setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setFixedHeight(63);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(14);auto *line=new QFrame;line->setFrameShape(QFrame::HLine);line->setFixedHeight(1);root->addWidget(line);
    auto *row=new QHBoxLayout;row->setSpacing(8);updated=text("",12,true);updated->setObjectName("lastUpdated");updated->setWordWrap(true);updated->setAlignment(Qt::AlignLeft|Qt::AlignBottom);updated->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);row->addWidget(updated,1,Qt::AlignBottom);
    const QStringList names{"Today","Tomorrow","Calendar"};for(int i=0;i<3;++i){tabs[i]=new QPushButton(names[i]);tabs[i]->setObjectName("viewButton"+QString::number(i));tabs[i]->setFont(pixelFont(16));tabs[i]->setFixedHeight(48);tabs[i]->setCheckable(true);tabs[i]->setFocusPolicy(Qt::NoFocus);row->addWidget(tabs[i]);connect(tabs[i],&QPushButton::clicked,this,[this,i]{emit viewRequested(i);});}
    gear=new GearButton;row->addWidget(gear);connect(gear,&QPushButton::clicked,this,&DashboardFooter::settingsRequested);root->addLayout(row);setView(0);setUpdateTimes({},{},{});
}
void DashboardFooter::setView(int view){for(int i=0;i<3;++i)tabs[i]->setChecked(i==view);}
void DashboardFooter::setUpdateTimes(QDateTime calendar,QDateTime weather,QDateTime summary,bool stale,bool sample){updated->setText(QString("%1Last updated: iCloud %2 · weather %3%4 · summary %5").arg(sample?"Sample · ":"",checked(calendar),checked(weather),stale?" (stale)":"",checked(summary)));}
