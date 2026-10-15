#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QWidget>
#include <optional>
#include <utility>

struct RainHour {QDateTime end;double amountMm;};
class RainForecast {
public:
    static std::optional<RainForecast> parse(const QJsonObject &value,QDateTime checked);
    QList<RainHour> nextHours(QDateTime at)const;
    bool raining(QDateTime at)const;
    double currentRate()const{return rate;}
private:
    QList<RainHour> hours;
    QDateTime checked,currentAt;
    double rate;
    RainForecast(QList<RainHour> values,QDateTime time,QDateTime currentTime,double currentRate):hours(std::move(values)),checked(time),currentAt(currentTime),rate(currentRate){}
};
class RainStrip:public QWidget {
    Q_OBJECT
public:
    explicit RainStrip(QWidget *parent=nullptr);
    void setForecast(const RainForecast &forecast,QDateTime at);
    void setTime(QDateTime at);
    void clear();
protected:
    void paintEvent(QPaintEvent *)override;
private:
    std::optional<RainForecast> forecast;
    QDateTime current;
    void describe();
};
