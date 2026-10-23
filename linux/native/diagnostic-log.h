#pragma once
#include <QString>

enum class DiagnosticEvent {
    CalendarRequestTimeout, CalendarOperationTimeout, CalendarRequestFailed,
    CalendarResponseInvalid, CalendarReady, CalendarFetchComplete,
    UpcomingUnavailable, UpcomingReady, UpcomingSelectionMissing,
    BriefingWaitingCalendar, BriefingWaitingWeather, BriefingRequestStarted,
    BriefingRequestFailed, BriefingReplyInvalid, BriefingComplete
};
QString diagnosticEventName(DiagnosticEvent event);
void diagnosticEvent(DiagnosticEvent event,int httpStatus=0,int networkError=0);
