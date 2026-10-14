#include "calendar-sources.h"
#include <algorithm>
QList<Event> mergeCalendarEvents(QList<Event> apple,const QList<Event> &google){
    apple.append(google);std::stable_sort(apple.begin(),apple.end(),[](const Event &a,const Event &b){if(a.allDay!=b.allDay)return a.allDay;return a.start<b.start;});return apple;
}
std::optional<CheckedCalendarData> combineCalendarSources(const CalendarSource &apple,const CalendarSource &google,QDateTime since){
    std::optional<CheckedCalendarData> result;
    for(const auto *source:{&apple,&google}){
        if(std::holds_alternative<CalendarNotConfigured>(*source))continue;
        if(std::holds_alternative<CalendarUnavailable>(*source))return std::nullopt;
        if(const auto *data=std::get_if<CheckedCalendarData>(source)){
            if(!data->checked.isValid()||(since.isValid()&&data->checked<since))return std::nullopt;
            if(!result)result=*data;
            else {result->events=mergeCalendarEvents(result->events,data->events);result->checked=std::min(result->checked,data->checked);}
        }
    }
    return result;
}
