#pragma once
#include <QString>
#include <QJsonObject>
#include <optional>

enum class DiagnosticEvent {
    CalendarRequestTimeout, CalendarOperationTimeout, CalendarRequestFailed,
    CalendarResponseInvalid, CalendarReady, CalendarFetchComplete,
    UpcomingUnavailable, UpcomingReady, UpcomingSelectionMissing,
    BriefingWaitingCalendar, BriefingWaitingWeather, BriefingRequestStarted,
    BriefingRequestFailed, BriefingReplyInvalid, BriefingComplete
};
QString diagnosticEventName(DiagnosticEvent event);
void diagnosticEvent(DiagnosticEvent event,int httpStatus=0,int networkError=0);

enum class GraphicsMetric {
    Protocol, Platform, QtMinor, ScreenWidth10px, ScreenHeight10px, RefreshHz,
    ScrollState, ScrollDuration10ms, ScrollMoves, ViewportPaints, UiGapMs,
    OvershootPx, QtWarning, KernelAccess, KernelFaultMask, KernelFaultCount, GpuDriver, AliveSeconds, BackendProtocol
};
// Reserved numeric packets survive the already-enrolled, fixed Python exporter.
// No free text, paths, screen pixels or household data enter this transport.
std::optional<QJsonObject> graphicsDiagnosticRecord(GraphicsMetric metric,int value);
struct GraphicsSample {GraphicsMetric metric;int value;};
std::optional<QJsonObject> graphicsDiagnosticRequest(GraphicsMetric metric,int value);
std::optional<GraphicsSample> parseGraphicsDiagnosticRequest(const QJsonObject &request);
void writeGraphicsDiagnostic(GraphicsMetric metric,int value);
