#pragma once
#include "data.h"
#include <optional>
#include <variant>
struct CalendarNotConfigured{};
struct CalendarUnavailable{};
struct CheckedCalendarData{QList<Event> events;QDateTime checked;};
using CalendarSource=std::variant<CalendarNotConfigured,CalendarUnavailable,CheckedCalendarData>;
QList<Event> mergeCalendarEvents(QList<Event> apple,const QList<Event> &google);
std::optional<CheckedCalendarData> combineCalendarSources(const CalendarSource &apple,const CalendarSource &google,QDateTime since);
