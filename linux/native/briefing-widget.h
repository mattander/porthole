#pragma once
#include <QLabel>
#include <QWidget>
#include <QJsonObject>

QString fittingBriefing(QString text,const QFont &font,QSize area);
class BriefingWidget:public QWidget {
public:
    explicit BriefingWidget(QWidget *parent=nullptr);
    void updateBriefing(const QJsonObject &state,bool sample=false);
    QString visibleText() const{return body->text();}
protected:
    void resizeEvent(QResizeEvent *event)override;
private:
    QLabel *body,*stamp;
    QString fullText;
    void fit();
};
