#include "profile.h"
#include "data.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QXmlStreamReader>
#include <libical/ical.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
constexpr auto Dav = "DAV:";
constexpr auto Cal = "urn:ietf:params:xml:ns:caldav";
const QRegularExpression CalendarIdPattern("\\A[0-9a-f]{64}\\z");
QString key(const QString &ns, const QString &name) { return ns + '|' + name; }
bool named(const QXmlStreamReader &xml, const char *ns, const char *name) {
    return xml.namespaceUri() == QLatin1String(ns) && xml.name() == QLatin1String(name);
}
QString propertyValue(QXmlStreamReader &xml) {
    if (named(xml, Dav, "resourcetype")) {
        bool calendar = false;
        while (xml.readNextStartElement()) {
            if (named(xml, Cal, "calendar")) calendar = true;
            xml.skipCurrentElement();
        }
        return calendar ? "calendar" : "";
    }
    if (named(xml, Cal, "supported-calendar-component-set")) {
        QStringList kinds;
        while (xml.readNextStartElement()) {
            if (named(xml, Cal, "comp")) kinds << xml.attributes().value("name").toString();
            xml.skipCurrentElement();
        }
        return kinds.join(',');
    }
    if (named(xml, Dav, "current-user-principal") || named(xml, Cal, "calendar-home-set")) {
        QString href;
        while (xml.readNextStartElement()) {
            if (named(xml, Dav, "href")) href = xml.readElementText();
            else xml.skipCurrentElement();
        }
        return href;
    }
    return xml.readElementText(QXmlStreamReader::IncludeChildElements);
}
QMap<QString, QString> propstat(QXmlStreamReader &xml) {
    QMap<QString, QString> properties;
    QString status;
    while (xml.readNextStartElement()) {
        if (named(xml, Dav, "status")) status = xml.readElementText();
        else if (named(xml, Dav, "prop")) {
            while (xml.readNextStartElement()) {
                QString name = key(xml.namespaceUri().toString(), xml.name().toString());
                properties.insert(name, propertyValue(xml));
            }
        } else xml.skipCurrentElement();
    }
    if (status.section(' ', 1, 1) != "200") properties.clear();
    return properties;
}
QString textProperty(icalcomponent *event, icalproperty_kind kind) {
    auto *p = icalcomponent_get_first_property(event, kind);
    if (!p) return {};
    const char *text = icalproperty_get_value_as_string(p);
    return text ? QString::fromUtf8(text) : QString();
}
QDateTime dateTime(icalcomponent *root, icalproperty *property, icaltimetype time) {
    if (!icaltime_is_valid_time(time) || icaltime_is_null_time(time))
        throw std::runtime_error("Invalid calendar date");
    if (time.is_date) return QDateTime(QDate(time.year,time.month,time.day), QTime(0,0), configuredZone());
    icaltimezone *zone = icaltimezone_get_utc_timezone();
    if (!icaltime_is_utc(time)) {
        auto *parameter = icalproperty_get_first_parameter(property, ICAL_TZID_PARAMETER);
        const auto defaultZone=configuredZone().id();
        const char *tzid = parameter ? icalparameter_get_tzid(parameter) : defaultZone.constData();
        zone = icalcomponent_get_timezone(root, tzid);
        if (!zone && QTimeZone(QByteArray(tzid)).isValid()) zone = icaltimezone_get_builtin_timezone(tzid);
        if (!zone) throw std::runtime_error("Unknown calendar timezone");
    }
    return QDateTime::fromSecsSinceEpoch(icaltime_as_timet_with_zone(time, zone), configuredZone());
}
}

QString davProperty(const QString &name) { return key(Dav, name); }
QString calProperty(const QString &name) { return key(Cal, name); }

QString canonicalCalendarColour(const QString &colour) {
    static const QRegularExpression valid("\\A#[0-9A-Fa-f]{6}(?:[0-9A-Fa-f]{2})?\\z");
    if(valid.match(colour).hasMatch())return colour.toUpper();
    return {};
}

QString calendarIdentity(const Calendar &calendar) {
    return QString::fromLatin1(QCryptographicHash::hash(
        calendar.url.toEncoded(QUrl::FullyEncoded), QCryptographicHash::Sha256).toHex());
}

QJsonObject calendarEventJson(const Event &event){
    QString id;if(CalendarIdPattern.match(event.calendarId).hasMatch())id=event.calendarId;
    return {{"uid",event.uid.left(240)},{"title",event.title},{"location",event.location},
            {"calendar",event.calendar},{"start",event.start.toString(Qt::ISODate)},
            {"end",event.end.toString(Qt::ISODate)},{"allDay",event.allDay},
            {"calendarId",id},
            {"colour",canonicalCalendarColour(event.colour)}};
}

std::optional<Event> calendarEventFromJson(const QJsonObject &value){
    const auto start=QDateTime::fromString(value["start"].toString(),Qt::ISODate).toTimeZone(configuredZone());
    const auto end=QDateTime::fromString(value["end"].toString(),Qt::ISODate).toTimeZone(configuredZone());
    if(!start.isValid()||!end.isValid()||end<start)return std::nullopt;
    const auto candidateId=value["calendarId"].toString();
    QString id;if(CalendarIdPattern.match(candidateId).hasMatch())id=candidateId;
    return Event{value["uid"].toString().left(240),value["title"].toString().left(500),
                 value["location"].toString().left(500),value["calendar"].toString().left(100),
                 start,end,value["allDay"].toBool(),id,
                 canonicalCalendarColour(value["colour"].toString())};
}

bool sameCalendarSelection(const QList<Calendar> &left,const QList<Calendar> &right){
    if(left.size()!=right.size())return false;
    QSet<QUrl> leftUrls,rightUrls;
    for(const auto &calendar:left)leftUrls.insert(calendar.url);
    for(const auto &calendar:right)rightUrls.insert(calendar.url);
    return leftUrls.size()==left.size()&&rightUrls.size()==right.size()&&leftUrls==rightUrls;
}

bool permittedCalendarUrl(const QUrl &url) {
    static const QRegularExpression hosts("^(caldav\\.icloud\\.com|p[0-9]{1,3}-caldav\\.icloud\\.com)$");
    return url.isValid() && url.scheme() == "https" && url.port(443) == 443 &&
           url.userInfo().isEmpty() && url.fragment().isEmpty() &&
           hosts.match(url.host().toLower()).hasMatch();
}
QUrl calendarHref(const QUrl &base, const QString &href) {
    if (href.isEmpty()) throw std::runtime_error("Missing calendar address");
    QUrl result = base.resolved(QUrl(href));
    if (!permittedCalendarUrl(result)) throw std::runtime_error("Untrusted calendar address");
    return result;
}

QList<DavResponse> parseDav(const QByteArray &bytes) {
    if (bytes.size() > 4 * 1024 * 1024) throw std::runtime_error("Calendar response too large");
    QXmlStreamReader xml(bytes);
    xml.setEntityExpansionLimit(1024);
    QList<DavResponse> responses;
    if (!xml.readNextStartElement() || !named(xml, Dav, "multistatus"))
        throw std::runtime_error("Invalid calendar response");
    while (xml.readNextStartElement()) {
        if (!named(xml, Dav, "response")) { xml.skipCurrentElement(); continue; }
        DavResponse response;
        while (xml.readNextStartElement()) {
            if (named(xml, Dav, "href")) response.href = xml.readElementText();
            else if (named(xml, Dav, "propstat")) response.properties.insert(propstat(xml));
            else xml.skipCurrentElement();
        }
        responses << response;
        if (responses.size() > 5000) throw std::runtime_error("Too many calendar resources");
    }
    // Complete the document, so malformed trailing input is rejected too.
    while (!xml.atEnd()) xml.readNext();
    if (xml.hasError()) throw std::runtime_error("Malformed calendar response");
    return responses;
}

QList<Event> parseExpandedCalendar(const QByteArray &bytes, const QString &calendar) {
    if (bytes.size() > 4 * 1024 * 1024 || bytes.contains('\0'))
        throw std::runtime_error("Invalid calendar payload");
    using Component = std::unique_ptr<icalcomponent, decltype(&icalcomponent_free)>;
    Component root(icalparser_parse_string(bytes.constData()), icalcomponent_free);
    if (!root || icalcomponent_isa(root.get()) != ICAL_VCALENDAR_COMPONENT ||
        icalcomponent_count_errors(root.get()) != 0)
        throw std::runtime_error("Malformed iCalendar data");
    QList<Event> events;
    for (auto *item = icalcomponent_get_first_component(root.get(), ICAL_VEVENT_COMPONENT);
         item; item = icalcomponent_get_next_component(root.get(), ICAL_VEVENT_COMPONENT)) {
        if (icalcomponent_get_status(item) == ICAL_STATUS_CANCELLED) continue;
        if (icalcomponent_get_first_property(item, ICAL_RRULE_PROPERTY) ||
            icalcomponent_get_first_property(item, ICAL_RDATE_PROPERTY))
            throw std::runtime_error("Calendar server did not expand recurring events");
        auto *start = icalcomponent_get_first_property(item, ICAL_DTSTART_PROPERTY);
        if (!start) throw std::runtime_error("Calendar event has no start");
        auto startTime = icalproperty_get_dtstart(start);
        Event event;
        event.allDay = startTime.is_date;
        event.start = dateTime(root.get(), start, startTime);
        auto *end = icalcomponent_get_first_property(item, ICAL_DTEND_PROPERTY);
        if (end) event.end = dateTime(root.get(), end, icalproperty_get_dtend(end));
        else if (icalcomponent_get_first_property(item, ICAL_DURATION_PROPERTY))
            event.end = event.start.addSecs(icaldurationtype_as_seconds(icalcomponent_get_duration(item)));
        else event.end = event.allDay ? event.start.addDays(1) : event.start;
        if (event.end < event.start) throw std::runtime_error("Invalid event duration");
        event.uid = textProperty(item, ICAL_UID_PROPERTY);
        event.title = textProperty(item, ICAL_SUMMARY_PROPERTY).left(240);
        if (event.title.isEmpty()) event.title = "Untitled event";
        event.location = textProperty(item, ICAL_LOCATION_PROPERTY).left(180);
        event.calendar = calendar.left(100);
        events << event;
        if (events.size() > 5000) throw std::runtime_error("Too many calendar events");
    }
    return events;
}

bool Event::onDate(QDate date) const {
    const QDate last = end > start ? end.addSecs(-1).date() : start.date();
    return date >= start.date() && date <= last;
}
QString Event::timeLabel() const {
    return allDay ? QString("All day") : start.toString("h:mm AP");
}
