#pragma once
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QUrl>

struct Calendar {
    QUrl url;
    QString name;
};
struct Event {
    QString uid, title, location, calendar;
    QDateTime start, end;
    bool allDay = false;
    bool onDate(QDate date) const;
    QString timeLabel() const;
};
struct DavResponse {
    QString href;
    QMap<QString, QString> properties;
};
QList<DavResponse> parseDav(const QByteArray &xml);
QList<Event> parseExpandedCalendar(const QByteArray &ics, const QString &calendar);
bool permittedCalendarUrl(const QUrl &url);
QUrl calendarHref(const QUrl &base, const QString &href);
QString davProperty(const QString &name);
QString calProperty(const QString &name);
