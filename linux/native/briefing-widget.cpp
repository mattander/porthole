#include "briefing-widget.h"
#include "briefing.h"
#include "calendar-style.h"
#include <QJsonArray>
#include <QFontMetricsF>
#include <QTextLayout>
#include <QTextBoundaryFinder>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QPainter>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QTextDocument>
#include <QToolTip>
#include <QScrollArea>
#include <QScroller>
#include <QtMath>

namespace {
std::optional<qreal> wrappedHeight(const QString &text,const QFont &font,int width){
    if(text.isEmpty())return 0;
    if(width<=0)return std::nullopt;
    QTextLayout layout(text,font);QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
    layout.beginLayout();qreal height=0;
    while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(width);height+=line.height();if(line.naturalTextWidth()>width+.1){layout.endLayout();return std::nullopt;}}
    layout.endLayout();return height;
}
QLabel *text(QString value,int size){auto *label=new QLabel(value);label->setTextFormat(Qt::PlainText);QFont font("DejaVu Sans");font.setPixelSize(size);label->setFont(font);return label;}
}
// QLabel's word wrap can clip an unbroken word instead of using our measured
// wrapping. Draw with exactly the same QTextLayout policy used by the fitter.
class BriefingText:public QLabel {
public:
    QJsonArray segments,calendars;
protected:
    void mouseReleaseEvent(QMouseEvent *event)override{
        QLabel::mouseReleaseEvent(event);
        if(event->button()==Qt::LeftButton&&!toolTip().isEmpty())QToolTip::showText(mapToGlobal(contentsRect().center()),toolTip(),this);
    }
    void paintEvent(QPaintEvent *)override{
        QPainter painter(this);painter.setClipRect(contentsRect());painter.setPen(palette().color(QPalette::WindowText));
        QTextLayout layout(text(),font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
        layout.setFormats(briefingFormats(text(),segments,calendars,palette().color(QPalette::Window),palette().color(QPalette::WindowText)));
        layout.beginLayout();qreal height=0;
        while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(qMax(0,width()-3));line.setPosition(QPointF(0,height));height+=line.height();}
        layout.endLayout();layout.draw(&painter,contentsRect().topLeft());
    }
};
// Plain-text names share the normal foreground; only the source dots are coloured.
class CalendarKey:public QLabel {
public:
    void setSources(const QList<BriefingCalendarKey> &sources){
        colours.clear();QString value;QStringList names;
        for(const auto &source:sources){
            if(!value.isEmpty())value+="   ";
            colours.append({value.size(),source.colour});value+=QString(QChar(0x25cf))+QChar(0xa0)+source.name;names.append(source.name);
        }
        setText(value);setAccessibleName("Calendar colour key");setAccessibleDescription(names.join(", "));setToolTip(Qt::convertFromPlainText(names.join('\n')));update();
    }
    int heightForWidth(int width)const override{return qCeil(wrappedHeight(text(),font(),width).value_or(22))+3;}
protected:
    void paintEvent(QPaintEvent *)override{
        const auto background=palette().color(QPalette::Window),foreground=palette().color(QPalette::WindowText);
        QPainter painter(this);painter.setPen(foreground);
        QTextLayout layout(text(),font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
        QList<QTextLayout::FormatRange> formats;
        for(const auto &dot:colours){
            QTextLayout::FormatRange format;format.start=dot.position;format.length=1;
            format.format.setForeground(compositeCalendarColour(dot.colour,background));format.format.setTextOutline(QPen(foreground,.5));formats.append(format);
        }
        layout.setFormats(formats);layout.beginLayout();qreal height=0;
        while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(width());line.setPosition(QPointF(0,height));height+=line.height();}
        layout.endLayout();layout.draw(&painter,QPointF(0,0));
    }
private:
    struct Dot {int position;QColor colour;};
    QList<Dot> colours;
};
QString BriefingWidget::visibleText()const{return body->text();}
QString fittingBriefing(QString value,const QFont &,QSize){return boundedBriefing(value);}
BriefingWidget::BriefingWidget(QWidget *parent):QWidget(parent){
    setObjectName("familyBriefing");setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setFixedHeight(166);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(8);
    auto *title=text("Family briefing",18);auto font=title->font();font.setWeight(QFont::DemiBold);title->setFont(font);title->setFixedHeight(24);
    auto *header=new QHBoxLayout;header->setSpacing(8);header->addWidget(title);
    auto *badge=text("AI",11);badge->setObjectName("aiBadge");badge->setProperty("tone","secondary");badge->setFixedSize(24,18);header->addWidget(badge);header->addStretch();
    stamp=text("",12);stamp->setObjectName("briefingStamp");stamp->setProperty("tone","secondary");stamp->setFixedHeight(18);header->addWidget(stamp);stamp->hide();layout->addLayout(header);
    body=new BriefingText;body->setText("Enable AI briefing in Settings");QFont bodyFont("DejaVu Sans");bodyFont.setPixelSize(21);body->setFont(bodyFont);
    body->setObjectName("briefingText");body->setTextFormat(Qt::PlainText);body->setWordWrap(true);body->setAlignment(Qt::AlignTop|Qt::AlignLeft);body->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);body->setFixedHeight(134);layout->addWidget(body);
    key=new CalendarKey;key->setObjectName("briefingCalendarKey");key->setTextFormat(Qt::PlainText);key->setWordWrap(true);QFont keyFont("DejaVu Sans");keyFont.setPixelSize(14);key->setFont(keyFont);
    keyArea=new QScrollArea;keyArea->setObjectName("briefingKeyArea");keyArea->setFrameShape(QFrame::NoFrame);keyArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);keyArea->setWidget(key);keyArea->hide();
    QScroller::grabGesture(keyArea->viewport(),QScroller::TouchGesture);layout->addWidget(keyArea);layout->addStretch();
    fullText=body->text();
}
void BriefingWidget::updateBriefing(const QJsonObject &state,bool sample){
    body->segments={};body->calendars={};
    if(sample){
        body->segments={QJsonObject{{"text","Sample: "},{"calendar_ids",QJsonArray{}}},
            QJsonObject{{"text","dinner together is at 6:30 PM today"},{"calendar_ids",QJsonArray{"sample-home"}}},
            QJsonObject{{"text",". "},{"calendar_ids",QJsonArray{}}},
            QJsonObject{{"text","Tomorrow’s soccer practice starts at 5 PM"},{"calendar_ids",QJsonArray{"sample-family"}}},
            QJsonObject{{"text",". The forecast is cool with rain."},{"calendar_ids",QJsonArray{}}}};
        body->calendars={QJsonObject{{"id","sample-home"},{"colour","#E7A82F"},{"name","Home"}},QJsonObject{{"id","sample-family"},{"colour","#AF52DE"},{"name","Family"}}};
        fullText.clear();for(const auto &value:body->segments)fullText+=value.toObject()["text"].toString();
        stamp->setText("Sample · fictional");stamp->show();
    }
    else {
        fullText=state["enabled"].toBool()?state["text"].toString():"Enable AI briefing in Settings";
        if(fullText.isEmpty())fullText=state["status"].toString("Set up AI briefing in Settings");
        const bool stale=state["stale"].toBool()&&state["enabled"].toBool()&&!state["text"].toString().isEmpty();
        const bool resting=state["status"].toString().startsWith("AI resting");
        stamp->setText(resting?"Saved · resumes at 6 AM":stale?"Source data stale":QString());stamp->setVisible(stale||resting);
        if(state["enabled"].toBool()&&fullText==state["text"].toString()){
            body->segments=state["segments"].toArray();body->calendars=state["calendar_colours"].toArray();
        }
    }
    // Preserve validated segment whitespace. Only truncate a legacy/invalid overlong string.
    if(fullText.toUcs4().size()>BriefingCharacterLimit)fullText=boundedBriefing(fullText);
    const auto description=briefingSourceDescription(fullText,body->segments,body->calendars);
    body->setToolTip(description.isEmpty()?QString():Qt::convertFromPlainText(description));
    body->setAccessibleDescription(description);fit();body->update();
}
void BriefingWidget::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);fit();}
void BriefingWidget::fit(){
    key->setSources(briefingCalendarKey(fullText,body->segments,body->calendars));
    const bool hasKey=!key->text().isEmpty();keyArea->setVisible(hasKey);
    const int keyWidth=qMax(1,width()-16),keyHeight=hasKey?qMin(48,key->heightForWidth(keyWidth)):0;
    key->setFixedSize(keyWidth,key->heightForWidth(keyWidth));keyArea->setFixedHeight(keyHeight);
    const int maximumHeight=134-(hasKey?keyHeight+8:0),bodyWidth=qMax(1,width()-3);
    QFont font("DejaVu Sans");font.setPixelSize(21);
    auto measured=wrappedHeight(fullText,font,bodyWidth);
    while(font.pixelSize()>1&&(!measured||*measured>maximumHeight-3)){font.setPixelSize(font.pixelSize()-1);measured=wrappedHeight(fullText,font,bodyWidth);}
    body->setFixedHeight(qBound(1,qCeil(measured.value_or(maximumHeight-3))+3,maximumHeight));body->setFont(font);body->setText(fullText);
}
