#include "briefing-widget.h"
#include "briefing.h"
#include <QFontMetricsF>
#include <QTextLayout>
#include <QTextBoundaryFinder>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QPainter>
#include <QHBoxLayout>

namespace {
bool fits(const QString &text,const QFont &font,QSize area){
    if(text.isEmpty())return true;
    if(area.width()<=0||area.height()<=0)return false;
    QTextLayout layout(text,font);QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
    layout.beginLayout();qreal height=0;
    while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(area.width());height+=line.height();if(height>area.height()||line.naturalTextWidth()>area.width()+.1){layout.endLayout();return false;}}
    layout.endLayout();return true;
}
QLabel *text(QString value,int size){auto *label=new QLabel(value);label->setTextFormat(Qt::PlainText);QFont font("DejaVu Sans");font.setPixelSize(size);label->setFont(font);return label;}
// QLabel's word wrap can clip an unbroken word instead of using our measured
// wrapping. Draw with exactly the same QTextLayout policy used by the fitter.
class BriefingText:public QLabel {
protected:
    void paintEvent(QPaintEvent *)override{
        QPainter painter(this);painter.setClipRect(contentsRect());painter.setPen(palette().color(QPalette::WindowText));
        QTextLayout layout(text(),font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
        layout.beginLayout();qreal height=0;
        while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(qMax(0,width()-3));line.setPosition(QPointF(0,height));height+=line.height();}
        layout.endLayout();layout.draw(&painter,contentsRect().topLeft());
    }
};
}
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
    fullText=body->text();
}
void BriefingWidget::updateBriefing(const QJsonObject &state,bool sample){
    if(sample){fullText="Sample: tomorrow’s soccer practice starts at 5 PM. Check the forecast before heading out.";stamp->setText("Sample · fictional");stamp->show();}
    else {
        fullText=state["enabled"].toBool()?state["text"].toString():"Enable AI briefing in Settings";
        if(fullText.isEmpty())fullText=state["status"].toString("Set up AI briefing in Settings");
        const bool stale=state["stale"].toBool()&&state["enabled"].toBool()&&!state["text"].toString().isEmpty();
        const bool resting=state["status"].toString().startsWith("AI resting");
        stamp->setText(resting?"Saved · resumes at 6 AM":stale?"Source data stale":QString());stamp->setVisible(stale||resting);
    }
    fullText=boundedBriefing(fullText);fit();
}
void BriefingWidget::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);fit();}
void BriefingWidget::fit(){
    QFont font("DejaVu Sans");font.setPixelSize(21);const auto area=body->contentsRect().size()-QSize(3,3);
    while(font.pixelSize()>1&&!fits(fullText,font,area))font.setPixelSize(font.pixelSize()-1);
    body->setFont(font);body->setText(fullText);
}
