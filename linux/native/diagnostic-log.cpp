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

namespace {
int wireCode(GraphicsMetric metric){
    switch(metric){
    case GraphicsMetric::Protocol:return 560;
    case GraphicsMetric::Platform:return 561;
    case GraphicsMetric::QtMinor:return 562;
    case GraphicsMetric::ScreenWidth10px:return 563;
    case GraphicsMetric::ScreenHeight10px:return 564;
    case GraphicsMetric::RefreshHz:return 565;
    case GraphicsMetric::ScrollState:return 566;
    case GraphicsMetric::ScrollDuration10ms:return 567;
    case GraphicsMetric::ScrollMoves:return 568;
    case GraphicsMetric::ViewportPaints:return 569;
    case GraphicsMetric::UiGapMs:return 570;
    case GraphicsMetric::OvershootPx:return 571;
    case GraphicsMetric::QtWarning:return 572;
    case GraphicsMetric::KernelAccess:return 573;
    case GraphicsMetric::KernelFaultMask:return 574;
    case GraphicsMetric::KernelFaultCount:return 575;
    case GraphicsMetric::GpuDriver:return 576;
    case GraphicsMetric::AliveSeconds:return 577;
    case GraphicsMetric::BackendProtocol:return 578;
    }
    Q_UNREACHABLE();
}
std::optional<GraphicsMetric> metricFromWireCode(int code){
    switch(code){
    case 560:return GraphicsMetric::Protocol;
    case 561:return GraphicsMetric::Platform;
    case 562:return GraphicsMetric::QtMinor;
    case 563:return GraphicsMetric::ScreenWidth10px;
    case 564:return GraphicsMetric::ScreenHeight10px;
    case 565:return GraphicsMetric::RefreshHz;
    case 566:return GraphicsMetric::ScrollState;
    case 567:return GraphicsMetric::ScrollDuration10ms;
    case 568:return GraphicsMetric::ScrollMoves;
    case 569:return GraphicsMetric::ViewportPaints;
    case 570:return GraphicsMetric::UiGapMs;
    case 571:return GraphicsMetric::OvershootPx;
    case 572:return GraphicsMetric::QtWarning;
    case 573:return GraphicsMetric::KernelAccess;
    case 574:return GraphicsMetric::KernelFaultMask;
    case 575:return GraphicsMetric::KernelFaultCount;
    case 576:return GraphicsMetric::GpuDriver;
    case 577:return GraphicsMetric::AliveSeconds;
    case 578:return GraphicsMetric::BackendProtocol;
    default:return std::nullopt;
    }
}
}
std::optional<QJsonObject> graphicsDiagnosticRecord(GraphicsMetric metric,int value){
    if(value<0||value>999)return std::nullopt;
    return QJsonObject{{"v",1},{"event","calendar_ready"},{"http",wireCode(metric)},{"network",value}};
}
std::optional<QJsonObject> graphicsDiagnosticRequest(GraphicsMetric metric,int value){
    if(value<0||value>999||metric==GraphicsMetric::BackendProtocol)return std::nullopt;
    return QJsonObject{{"type","graphicsDiagnostics"},{"metric",wireCode(metric)},{"value",value}};
}
std::optional<GraphicsSample> parseGraphicsDiagnosticRequest(const QJsonObject &request){
    if(request.size()!=3||request["type"]!=QJsonValue("graphicsDiagnostics"))return std::nullopt;
    const auto code=request["metric"],number=request["value"];
    const int metricCode=code.toInt(-1),value=number.toInt(-1);
    if(!code.isDouble()||code.toDouble()!=metricCode||!number.isDouble()||number.toDouble()!=value||value<0||value>999)return std::nullopt;
    const auto metric=metricFromWireCode(metricCode);
    if(!metric||*metric==GraphicsMetric::BackendProtocol)return std::nullopt;
    return GraphicsSample{*metric,value};
}
void writeGraphicsDiagnostic(GraphicsMetric metric,int value){
    if(const auto record=graphicsDiagnosticRecord(metric,value))
        qInfo().noquote()<<"PORTHOLE_DIAGNOSTIC"<<QJsonDocument(*record).toJson(QJsonDocument::Compact);
}
