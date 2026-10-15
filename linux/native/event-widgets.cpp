#include "event-widgets.h"
#include "calendar-style.h"
#include "profile.h"
#include "typography.h"
#include <QScrollArea>
#include <QScroller>
#include <QTimeZone>
#include <QTextLayout>
#include <QPainter>
#include <QtMath>
#include <QVBoxLayout>

namespace {
class FlowLabel:public QLabel {
    void flowingText(QTextLayout &result,int width)const{
        auto value=text();value.replace('\n',QChar::LineSeparator);result.setText(value);result.setFont(font());
        QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);result.setTextOption(option);
        result.beginLayout();qreal top=0;
        while(true){auto line=result.createLine();if(!line.isValid())break;line.setLineWidth(qMax(1,width));line.setPosition(QPointF(0,top));top+=line.height();}
        result.endLayout();
    }
public:
    explicit FlowLabel(QWidget *parent):QLabel(parent){}
    int heightForWidth(int width)const override{QTextLayout layout;flowingText(layout,width);return qCeil(layout.boundingRect().height());}
    QSize minimumSizeHint()const override{return {0,fontMetrics().height()};}
protected:
    void paintEvent(QPaintEvent *)override{QTextLayout layout;flowingText(layout,contentsRect().width());QPainter painter(this);painter.setPen(palette().color(QPalette::WindowText));layout.draw(&painter,contentsRect().topLeft());}
};
QLabel *copy(const QString &text,int pixels,const QString &name,QWidget *parent=nullptr){
    auto *label=new FlowLabel(parent);label->setText(text);label->setObjectName(name);label->setTextFormat(Qt::PlainText);label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    label->setWordWrap(true);label->setFont(interfaceFont(pixels));label->setTextInteractionFlags(Qt::NoTextInteraction);return label;
}
}
QString eventDateRange(const Event &event){
    const auto start=event.start.toTimeZone(configuredZone()),end=event.end.toTimeZone(configuredZone());
    if(event.allDay){
        const auto last=(end>start?end.addSecs(-1):start).date();
        return start.date()==last?start.toString("dddd, MMMM d · 'All day'"):start.toString("ddd, MMM d")+" – "+last.toString("ddd, MMM d")+" · All day";
    }
    return start.date()==end.date()?start.toString("ddd, MMM d · h:mm AP")+" – "+end.toString("h:mm AP"):
        start.toString("ddd, MMM d · h:mm AP")+" – "+end.toString("ddd, MMM d · h:mm AP");
}
EventCard::EventCard(const Event &event,QWidget *parent):QPushButton(parent){
    setObjectName("agendaEvent");setMinimumHeight(88);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    setAccessibleName(event.title+" · "+eventDateRange(event)+" · "+event.calendar);setAccessibleDescription("Open event details");
    setToolTip("Open event details");setCursor(Qt::PointingHandCursor);
    QString colour=palette().color(QPalette::Highlight).name();
    if(const auto source=calendarSourceColour(event.colour))colour=compositeCalendarColour(*source,palette().color(QPalette::Window)).name();
    setStyleSheet("QPushButton#agendaEvent {border:0;border-left:5px solid "+colour+";border-radius:0;padding:0;text-align:left;} QPushButton#agendaEvent QLabel {background:transparent;}");
    auto *row=new QVBoxLayout(this);row->setContentsMargins(16,4,28,10);row->setSpacing(5);
    auto *metadata=copy(event.timeLabel()+" · "+event.calendar,16,"eventMetadata",this);metadata->setProperty("tone","secondary");row->addWidget(metadata);
    row->addWidget(copy(event.title,28,"eventTitle",this));
    if(!event.location.isEmpty()){auto *place=copy(event.location,16,"eventLocation",this);place->setProperty("tone","secondary");row->addWidget(place);}
    for(auto *label:findChildren<QLabel*>())label->setAttribute(Qt::WA_TransparentForMouseEvents);
}
QSize EventCard::sizeHint()const{return layout()->sizeHint().expandedTo(QSize(0,88));}
int EventCard::heightForWidth(int width)const{return qMax(88,layout()->totalHeightForWidth(width));}
void EventCard::paintEvent(QPaintEvent *event){
    QPushButton::paintEvent(event);QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(palette().color(QPalette::ButtonText),1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    const QPointF center(width()-13,height()/2.0);painter.drawLine(center+QPointF(-3,-5),center+QPointF(2,0));painter.drawLine(center+QPointF(2,0),center+QPointF(-3,5));
}
EventDetails::EventDetails(const Event &event,QWidget *parent):QDialog(parent){
    setObjectName("eventDetails");setWindowTitle("Event details");setModal(true);setMinimumSize(320,280);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(24,24,24,20);root->setSpacing(18);
    auto *content=new QWidget;auto *body=new QVBoxLayout(content);body->setContentsMargins(0,0,16,0);body->setSpacing(16);
    title=copy({},30,"detailTitle");when=copy({},20,"detailTime");source=copy({},18,"detailCalendar");location=copy({},20,"detailLocation");notes=copy("Loading notes…",20,"detailNotes");
    for(auto *label:{title,when,source,location})body->addWidget(label);
    auto *notesHeading=copy("Notes",16,"detailNotesHeading");notesHeading->setProperty("tone","secondary");body->addWidget(notesHeading);body->addWidget(notes);
    body->addStretch();auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setWidget(content);
    QScroller::grabGesture(scroll->viewport(),QScroller::TouchGesture);root->addWidget(scroll,1);
    auto *close=new QPushButton("Close");close->setObjectName("closeEventDetails");close->setMinimumHeight(56);close->setFont(interfaceFont(20));root->addWidget(close);connect(close,&QPushButton::clicked,this,&QDialog::accept);
    showEventData(event);
}
void EventDetails::showEventData(const Event &event){
    title->setText(event.title);when->setText(eventDateRange(event));source->setText("Calendar · "+event.calendar);
    location->setText(event.location);location->setVisible(!event.location.isEmpty());
}
void EventDetails::setDetails(const std::optional<Event> &event){
    if(!event){notes->setText("Notes unavailable. Close and try again after calendars refresh.");return;}
    showEventData(*event);notes->setText(event->description.isEmpty()?"No notes provided.":event->description);
}
