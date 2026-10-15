#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QUrl>
#include <optional>

struct Calendar {
    QUrl url;
    QString name;
    QString colour = {};
};
struct Event {
    QString uid, title, location, calendar;
    QDateTime start, end;
    bool allDay = false;
    QString calendarId = {};
    QString colour = {};
    QString description = {};
    QString detailsReference = {};
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
QString canonicalCalendarColour(const QString &colour);
QString calendarIdentity(const Calendar &calendar);
QJsonObject calendarEventJson(const Event &event);
QString calendarEventReference(const Event &event);
QString boundedEventDescription(const QString &value);
QJsonObject calendarEventDetailsJson(const QList<Event> &events,const QString &reference);
std::optional<Event> calendarEventFromJson(const QJsonObject &value);
bool sameCalendarSelection(const QList<Calendar> &left,const QList<Calendar> &right);
