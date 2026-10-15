#pragma once
#include "data.h"
#include <QDialog>
#include <QLabel>
#include <QPushButton>

QString eventDateRange(const Event &event);

class EventCard:public QPushButton {
    Q_OBJECT
public:
    explicit EventCard(const Event &event,QWidget *parent=nullptr);
    QSize sizeHint()const override;
    int heightForWidth(int width)const override;
protected:
    void paintEvent(QPaintEvent *event)override;
};

class EventDetails:public QDialog {
    Q_OBJECT
public:
    explicit EventDetails(const Event &event,QWidget *parent=nullptr);
    void setDetails(const std::optional<Event> &event);
private:
    QLabel *title,*when,*source,*location,*notes;
    void showEventData(const Event &event);
};
