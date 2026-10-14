#pragma once
#include <QLabel>
#include <QWidget>
#include <QJsonObject>

QString fittingBriefing(QString text,const QFont &font,QSize area);
class BriefingText;
class CalendarKey;
class QScrollArea;
class BriefingWidget:public QWidget {
public:
    explicit BriefingWidget(QWidget *parent=nullptr);
    void updateBriefing(const QJsonObject &state,bool sample=false);
    QString visibleText() const;
protected:
    void resizeEvent(QResizeEvent *event)override;
private:
    BriefingText *body;
    CalendarKey *key;
    QScrollArea *keyArea;
    QLabel *stamp;
    QString fullText;
    void fit();
};
