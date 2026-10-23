#include "diagnostic-log.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QMap>

QString diagnosticEventName(DiagnosticEvent event){
    switch(event){
    case DiagnosticEvent::CalendarRequestTimeout:return "calendar_request_timeout";
    case DiagnosticEvent::CalendarOperationTimeout:return "calendar_operation_timeout";
    case DiagnosticEvent::CalendarRequestFailed:return "calendar_request_failed";
    case DiagnosticEvent::CalendarResponseInvalid:return "calendar_response_invalid";
    case DiagnosticEvent::CalendarReady:return "calendar_ready";
    case DiagnosticEvent::CalendarFetchComplete:return "calendar_fetch_complete";
    case DiagnosticEvent::UpcomingUnavailable:return "upcoming_unavailable";
    case DiagnosticEvent::UpcomingReady:return "upcoming_ready";
    case DiagnosticEvent::UpcomingSelectionMissing:return "upcoming_selection_missing";
    case DiagnosticEvent::BriefingWaitingCalendar:return "briefing_waiting_calendar";
    case DiagnosticEvent::BriefingWaitingWeather:return "briefing_waiting_weather";
    case DiagnosticEvent::BriefingRequestStarted:return "briefing_request_started";
    case DiagnosticEvent::BriefingRequestFailed:return "briefing_request_failed";
    case DiagnosticEvent::BriefingReplyInvalid:return "briefing_reply_invalid";
    case DiagnosticEvent::BriefingComplete:return "briefing_complete";
    }
    Q_UNREACHABLE();
}
void diagnosticEvent(DiagnosticEvent event,int httpStatus,int networkError){
    static QMap<DiagnosticEvent,QElapsedTimer> last;
    if(event==DiagnosticEvent::UpcomingUnavailable||event==DiagnosticEvent::UpcomingSelectionMissing)last.remove(DiagnosticEvent::UpcomingReady);
    if(event==DiagnosticEvent::CalendarRequestTimeout||event==DiagnosticEvent::CalendarOperationTimeout||event==DiagnosticEvent::CalendarRequestFailed||event==DiagnosticEvent::CalendarResponseInvalid)last.remove(DiagnosticEvent::CalendarFetchComplete);
    // Sample successful background checks; retain every failure and recovery transition.
    if(event==DiagnosticEvent::CalendarFetchComplete||event==DiagnosticEvent::UpcomingReady){
        auto &checked=last[event];if(checked.isValid()&&checked.elapsed()<15*60*1000)return;checked.start();
    }
    // Only fixed event names and bounded numbers cross this logging boundary.
    const QJsonObject value{{"v",1},{"event",diagnosticEventName(event)},
        {"http",qBound(0,httpStatus,599)},{"network",qBound(0,networkError,999)}};
    qInfo().noquote()<<"PORTHOLE_DIAGNOSTIC"<<QJsonDocument(value).toJson(QJsonDocument::Compact);
}
