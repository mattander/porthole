#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QWidget>
#include <optional>
#include <utility>

struct RainHour {QDateTime end;int chance;};
class RainForecast {
public:
    static std::optional<RainForecast> parse(const QJsonObject &value,QDateTime checked);
    QList<RainHour> nextHours(QDateTime at)const;
    QDateTime checkedAt()const{return checked;}
private:
    QList<RainHour> hours;
    QDateTime checked;
    RainForecast(QList<RainHour> values,QDateTime time):hours(std::move(values)),checked(time){}
};
class RainStrip:public QWidget {
    Q_OBJECT
public:
    explicit RainStrip(QWidget *parent=nullptr);
    void setForecast(const RainForecast &forecast,QDateTime at);
    void setTime(QDateTime at);
    void setStale(bool value);
    void clear();
protected:
    void paintEvent(QPaintEvent *)override;
private:
    std::optional<RainForecast> forecast;
    QDateTime current;
    bool failed=false;
    void describe();
};
