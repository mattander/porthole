#pragma once
#include <QColor>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QTextLayout>
#include <optional>

std::optional<QColor> calendarSourceColour(const QString &source);
QColor compositeCalendarColour(const QColor &source, const QColor &background);
double calendarContrast(const QColor &foreground, const QColor &background);
QColor calendarPhraseTint(const QString &source, const QColor &background, const QColor &foreground);
QList<QTextLayout::FormatRange> briefingFormats(const QString &text, const QJsonArray &segments,
    const QJsonArray &calendars, const QColor &background, const QColor &foreground);
QString briefingSourceDescription(const QString &text, const QJsonArray &segments, const QJsonArray &calendars);
struct BriefingCalendarKey {QString name;QColor colour;};
QList<BriefingCalendarKey> briefingCalendarKey(const QString &text, const QJsonArray &segments, const QJsonArray &calendars);
