#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>

enum class DataSource {Wifi,Calendar,Weather,Summary};
enum class IssueCode {ConnectionLost,WifiConnection,WifiScan,CalendarUnavailable,CalendarSignIn,WeatherUnavailable,InvalidForecast,SummaryUnavailable,SummaryKey,SummaryStorage};
struct DiagnosticIssue {DataSource source;IssueCode code;QDateTime first,last;bool resolved=false;};
class Diagnostics {
public:
    void fail(DataSource source,IssueCode code);
    void recover(DataSource source);
    void clearResolved();
    int activeCount()const;
    bool hasActive(DataSource source)const;
    QJsonObject snapshot()const;
private:
    QList<DiagnosticIssue> history;
};
QString sourceName(DataSource source);
QString issueMessage(IssueCode code);
QString issueAction(IssueCode code);
QJsonObject deviceHealth();
QJsonObject safeDiagnostics(const QJsonObject &state);
