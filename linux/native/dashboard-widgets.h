#pragma once
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class ClockFace:public QWidget {
    Q_OBJECT
public:
    explicit ClockFace(QWidget *parent=nullptr);
    void setTime(QTime value);
    void setPortrait(bool portrait);
    qreal splitPosition()const;
protected:
    void paintEvent(QPaintEvent *)override;
private:
    QString digits="—:—",period;
    int pixels=120;
    QFont timeFont()const;
    QFont periodFont()const;
};
bool validWeatherCode(int code);
QString weatherCondition(int code);
class WeatherIcon:public QWidget {
    Q_OBJECT
public:
    explicit WeatherIcon(int size=64,QWidget *parent=nullptr);
    void setCode(int value);
protected:
    void paintEvent(QPaintEvent *)override;
private:
    int code=-1;
};
struct ForecastDay {QDate date;int code;int high;int low;};
class ForecastStrip:public QWidget {
public:
    explicit ForecastStrip(QWidget *parent=nullptr);
    void setForecast(const QList<ForecastDay> &days);
    void clear(){setForecast({});}
private:
    WeatherIcon *icons[3];
    QLabel *dates[3],*temperatures[3],*conditions[3];
};
class GearButton:public QPushButton {
public:
    explicit GearButton(QWidget *parent=nullptr);
    void setIssueCount(int count);
protected:
    void paintEvent(QPaintEvent *)override;
private:
    int issues=0;
};
class CalendarButton:public QPushButton {
public:
    explicit CalendarButton(QWidget *parent=nullptr);
protected:
    void paintEvent(QPaintEvent *event)override;
};
class AgendaDays:public QWidget {
    Q_OBJECT
public:
    explicit AgendaDays(QWidget *parent=nullptr);
    void setView(int view);
signals:
    void viewRequested(int view);
private:
    QPushButton *days[2];
};
class DashboardFooter:public QWidget {
    Q_OBJECT
public:
    explicit DashboardFooter(QWidget *parent=nullptr);
    void setView(int view);
    void setUpdateTimes(QDateTime calendar,QDateTime weather,QDateTime summary,bool weatherStale=false,bool sample=false);
    void setIssueCount(int count){gear->setIssueCount(count);}
signals:
    void viewRequested(int view);
    void settingsRequested();
private:
    QLabel *updated;
    GearButton *gear;
    CalendarButton *calendar;
};
