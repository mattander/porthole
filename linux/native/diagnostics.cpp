#include "diagnostics.h"
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>

QString sourceName(DataSource source){switch(source){case DataSource::Wifi:return "Wi-Fi";case DataSource::Calendar:return "Calendars";case DataSource::Weather:return "Weather";case DataSource::Summary:return "AI briefing";}return {};}
QString issueMessage(IssueCode code){switch(code){
case IssueCode::ConnectionLost:return "Wi-Fi connection was lost";
case IssueCode::WifiConnection:return "Wi-Fi connection failed";
case IssueCode::WifiScan:return "Wi-Fi scan failed";
case IssueCode::CalendarUnavailable:return "Calendar refresh failed";
case IssueCode::CalendarSignIn:return "Calendar sign-in needs attention";
case IssueCode::WeatherUnavailable:return "Weather refresh failed";
case IssueCode::InvalidForecast:return "Weather response could not be used";
case IssueCode::SummaryUnavailable:return "AI briefing update failed";
case IssueCode::SummaryKey:return "OpenAI key was rejected";
case IssueCode::SummaryStorage:return "AI storage or spending record needs attention";
}return {};}
QString issueAction(IssueCode code){switch(code){
case IssueCode::ConnectionLost:return "Choose or reconnect your network in Wi-Fi. Saved data remains available.";
case IssueCode::WifiConnection:return "Check the network password, then try again.";
case IssueCode::WifiScan:return "Try Refresh networks in Wi-Fi.";
case IssueCode::CalendarSignIn:return "Check the affected account in Calendars. Replace the iCloud app-specific password or reconnect Google.";
case IssueCode::CalendarUnavailable:return "Check Wi-Fi and the frame’s clock. Calendar checks retry every minute.";
case IssueCode::WeatherUnavailable:case IssueCode::InvalidForecast:return "Kept the last forecast. Weather retries every 15 minutes.";
case IssueCode::SummaryKey:return "Replace the OpenAI key from your Mac over USB.";
case IssueCode::SummaryUnavailable:return "Kept the last briefing. Check the connection and OpenAI credit balance. Request limits still apply.";
case IssueCode::SummaryStorage:return "Paid requests are paused. Check the SD card and the AI settings.";
}return {};}
void Diagnostics::fail(DataSource source,IssueCode code){
    const auto now=QDateTime::currentDateTimeUtc();
    for(auto &issue:history)if(issue.source==source&&!issue.resolved){if(issue.code==code){issue.last=now;return;}issue.resolved=true;}
    history.prepend({source,code,now,now,false});while(history.size()>50){int index=history.size()-1;while(index>=0&&!history[index].resolved)--index;if(index<0)break;history.removeAt(index);}
}
void Diagnostics::recover(DataSource source){for(auto &issue:history)if(issue.source==source&&!issue.resolved)issue.resolved=true;}
void Diagnostics::clearResolved(){history.removeIf([](const DiagnosticIssue &issue){return issue.resolved;});}
bool Diagnostics::hasActive(DataSource source)const{for(const auto &issue:history)if(issue.source==source&&!issue.resolved)return true;return false;}
int Diagnostics::activeCount()const{int count=0;for(const auto &issue:history)if(!issue.resolved)++count;return count;}
QJsonObject Diagnostics::snapshot()const{
    QJsonArray issues;for(const auto &issue:history)issues.append(QJsonObject{{"service",sourceName(issue.source)},{"message",issueMessage(issue.code)},{"action",issueAction(issue.code)},{"first",issue.first.toString(Qt::ISODate)},{"last",issue.last.toString(Qt::ISODate)},{"resolved",issue.resolved}});
    return {{"issues",issues},{"active",activeCount()}};
}
QJsonObject deviceHealth(){
    QJsonObject value;QFile memory("/proc/meminfo");if(memory.open(QIODevice::ReadOnly)){const auto contents=QString::fromLatin1(memory.readAll());const auto match=QRegularExpression("(?:^|\\n)MemAvailable:\\s+(\\d+) kB").match(contents);if(match.hasMatch())value["availableMemoryMB"]=match.captured(1).toLongLong()/1024;}
    QFile uptime("/proc/uptime");if(uptime.open(QIODevice::ReadOnly)){bool ok=false;const auto seconds=uptime.readAll().split(' ').first().toDouble(&ok);if(ok&&seconds>=0)value["uptimeSeconds"]=qint64(seconds);}
    return value;
}
QJsonObject safeDiagnostics(const QJsonObject &state){
    const QStringList names{"Wi-Fi","iCloud","Google","Calendars","Weather","AI briefing"};
    const QStringList statuses{"Sample","Connected","Offline","Current state","Connecting","Not connected","Needs attention","Last calendar available","Last result available","Waiting","Off","Up to date","Stale · retry pending"};
    auto date=[](QString text){const auto parsed=QDateTime::fromString(text,Qt::ISODate);return parsed.isValid()?parsed.toString(Qt::ISODate):QString();};
    QJsonArray sources,issues;QJsonObject health;
    for(const auto &value:state["sources"].toArray()){
        const auto item=value.toObject();const auto name=item["name"].toString();if(!names.contains(name)||sources.size()>=5)continue;
        auto checked=item["checked"].toString();if(checked!="Current state"&&checked!="Sample")checked=date(checked);if(checked.isEmpty())checked="—";
        const auto status=item["status"].toString();sources.append(QJsonObject{{"name",name},{"status",statuses.contains(status)?status:QString("Unavailable")},{"checked",checked}});
    }
    int active=0;
    for(const auto &value:state["issues"].toArray()){
        if(issues.size()>=50)break;
        const auto item=value.toObject();if(!names.contains(item["service"].toString()))continue;
        for(IssueCode code:{IssueCode::ConnectionLost,IssueCode::WifiConnection,IssueCode::WifiScan,IssueCode::CalendarUnavailable,IssueCode::CalendarSignIn,IssueCode::WeatherUnavailable,IssueCode::InvalidForecast,IssueCode::SummaryUnavailable,IssueCode::SummaryKey,IssueCode::SummaryStorage}){
            if(item["message"].toString()!=issueMessage(code))continue;
            const bool resolved=item["resolved"].toBool();if(!resolved)++active;
            issues.append(QJsonObject{{"service",item["service"].toString()},{"message",issueMessage(code)},{"action",issueAction(code)},{"first",date(item["first"].toString())},{"last",date(item["last"].toString())},{"resolved",resolved}});break;
        }
    }
    for(const QString &key:{QString("availableMemoryMB"),QString("uptimeSeconds")})if(state["device"].toObject()[key].isDouble()){const auto value=state["device"].toObject()[key].toInteger(-1);if(value>=0)health[key]=value;}
    return {{"sources",sources},{"issues",issues},{"active",qMin(active,4)},{"device",health},{"spentMicros",qMax(qint64(0),state["spentMicros"].toInteger())}};
}
