#pragma once
#include <QElapsedTimer>
#include "diagnostic-log.h"
#include <QJsonObject>
#include <QHash>
#include <QPoint>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QtLogging>
#include <functional>
#include <optional>

class QScrollArea;
std::optional<int> graphicsWarningCode(const QString &category,const QString &message);
int graphicsKernelFaultMask(const QByteArray &message);

class GraphicsDiagnostics:public QObject {
public:
    using Sink=std::function<void(GraphicsMetric,int)>;
    explicit GraphicsDiagnostics(bool enabled,QObject *parent=nullptr,Sink sink={});
    ~GraphicsDiagnostics()override;
    void watch(QScrollArea &area);
    void flush();
protected:
    bool eventFilter(QObject *object,QEvent *event)override;
private:
    bool enabled;
    Sink sink;
    QtMessageHandler previous=nullptr;
    QTimer tick,flushTimer,quietTimer,kernelTimer,kernelDeadline,heartbeat;
    QElapsedTimer clock,scrollClock;
    qint64 lastTick=0;
    int moves=0,paints=0,maxGap=0,maxOvershoot=0;
    QProcess kernel;
    QByteArray kernelBytes,kernelErrors;
    bool kernelOverflow=false;
    int lastKernelAccess=-1;
    QHash<QObject*,QScrollArea*> areas;
    QHash<QObject*,QPoint> viewportOrigins;
    void beginScroll();
    void emitMetric(GraphicsMetric metric,int value);
    void reportRuntime();
    void readKernel();
    void finishKernel(int exitCode,QProcess::ExitStatus status);
};
