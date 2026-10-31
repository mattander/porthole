#include "graphics-diagnostics.h"
#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QFileInfo>
#include <QJsonDocument>
#include <QScreen>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <array>
#include <atomic>
#include <cstdio>

namespace {
std::array<std::atomic<unsigned>,7> warnings{};
std::atomic<QtMessageHandler> chainedHandler{nullptr};
void graphicsMessage(QtMsgType type,const QMessageLogContext &context,const QString &message){
    if(type==QtWarningMsg||type==QtCriticalMsg||type==QtFatalMsg){
        if(const auto code=graphicsWarningCode(QString::fromLatin1(context.category?context.category:""),message))
            warnings.at(*code-1).fetch_add(1,std::memory_order_relaxed);
    }
    if(const auto handler=chainedHandler.load(std::memory_order_relaxed))handler(type,context,message);
    else {const auto formatted=qFormatLogMessage(type,context,message).toLocal8Bit();std::fprintf(stderr,"%s\n",formatted.constData());}
}
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
    }
    Q_UNREACHABLE();
}
int stateCode(QScroller::State state){
    switch(state){
    case QScroller::Inactive:return 0;
    case QScroller::Pressed:return 1;
    case QScroller::Dragging:return 2;
    case QScroller::Scrolling:return 3;
    }
    Q_UNREACHABLE();
}
}
std::optional<QJsonObject> graphicsDiagnosticRecord(GraphicsMetric metric,int value){
    if(value<0||value>999)return std::nullopt;
    return QJsonObject{{"v",1},{"event","calendar_ready"},{"http",wireCode(metric)},{"network",value}};
}
std::optional<int> graphicsWarningCode(const QString &category,const QString &message){
    const auto name=category.toLower(),text=message.toLower();
    const bool graphics=name.startsWith("qt.qpa")||name.startsWith("qt.wayland")||name.startsWith("qt.scenegraph")||name.startsWith("qt.rhi")||
        text.contains("qwayland")||text.contains("qbackingstore")||text.contains("egl_bad_");
    if(!graphics)return std::nullopt;
    if(text.contains("timeout")||text.contains("timed out"))return 1;
    if(text.contains("buffer"))return 2;
    if(text.contains("egl")||text.contains("opengl"))return 3;
    if(text.contains("backing")||text.contains("paint"))return 4;
    if(text.contains("protocol")||text.contains("connection"))return 5;
    if(text.contains("allocation")||text.contains("memory"))return 6;
    return 7;
}
int graphicsKernelFaultMask(const QByteArray &message){
    const auto text=message.toLower();
    if(!text.contains("drm")&&!text.contains("lima")&&!text.contains("gpu")&&!text.contains("dsi")&&!text.contains("tcon"))return 0;
    int mask=0;
    if(text.contains("underrun")||text.contains("underflow"))mask|=1;
    if(text.contains("timeout")||text.contains("timed out"))mask|=2;
    static const QRegularExpression fault("\\b(fault|faults|hang|hung)\\b");
    if(fault.match(QString::fromUtf8(text)).hasMatch())mask|=4;
    if(text.contains("error")||text.contains("failed"))mask|=8;
    if(text.contains("reset"))mask|=16;
    return mask;
}
GraphicsDiagnostics::GraphicsDiagnostics(bool active,QObject *parent,Sink output):QObject(parent),enabled(active),sink(std::move(output)){
    if(!enabled)return;
    if(!sink)sink=[](GraphicsMetric metric,int value){
        if(const auto record=graphicsDiagnosticRecord(metric,value))
            qInfo().noquote()<<"PORTHOLE_DIAGNOSTIC"<<QJsonDocument(*record).toJson(QJsonDocument::Compact);
    };
    for(auto &count:warnings)count.store(0,std::memory_order_relaxed);
    previous=qInstallMessageHandler(graphicsMessage);chainedHandler=previous;
    clock.start();
    connect(&tick,&QTimer::timeout,this,[this]{
        const auto now=clock.elapsed();if(scrollClock.isValid()){const int gap=qMin(qint64(999),now-lastTick);maxGap=qMax(maxGap,gap);}lastTick=now;
    });tick.start(50);
    connect(&flushTimer,&QTimer::timeout,this,&GraphicsDiagnostics::flush);flushTimer.start(5000);
    quietTimer.setSingleShot(true);connect(&quietTimer,&QTimer::timeout,this,&GraphicsDiagnostics::flush);
    connect(&heartbeat,&QTimer::timeout,this,[this]{emitMetric(GraphicsMetric::Protocol,1);emitMetric(GraphicsMetric::AliveSeconds,qMin(qint64(999),clock.elapsed()/1000));});heartbeat.start(15*60*1000);
    QTimer::singleShot(60000,this,[this]{emitMetric(GraphicsMetric::AliveSeconds,qMin(qint64(999),clock.elapsed()/1000));});
    kernelDeadline.setSingleShot(true);connect(&kernelDeadline,&QTimer::timeout,this,[this]{kernel.kill();});
    connect(&kernel,&QProcess::readyReadStandardOutput,this,[this]{
        kernel.setReadChannel(QProcess::StandardOutput);
        kernelBytes+=kernel.read(65537-kernelBytes.size());
        if(kernelBytes.size()>65536){kernelOverflow=true;kernel.kill();}
    });
    connect(&kernel,&QProcess::readyReadStandardError,this,[this]{kernelErrors+=kernel.readAllStandardError().left(4096-kernelErrors.size());});
    connect(&kernel,&QProcess::finished,this,&GraphicsDiagnostics::finishKernel);
    connect(&kernel,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){kernelDeadline.stop();if(lastKernelAccess!=3){lastKernelAccess=3;emitMetric(GraphicsMetric::KernelAccess,3);}}
    });
    connect(&kernelTimer,&QTimer::timeout,this,&GraphicsDiagnostics::readKernel);kernelTimer.start(60000);
    QTimer::singleShot(0,this,[this]{reportRuntime();readKernel();});
}
GraphicsDiagnostics::~GraphicsDiagnostics(){
    if(!enabled)return;
    kernel.disconnect(this);if(kernel.state()!=QProcess::NotRunning){kernel.kill();kernel.waitForFinished(100);}
    qInstallMessageHandler(previous);chainedHandler=nullptr;
}
void GraphicsDiagnostics::emitMetric(GraphicsMetric metric,int value){sink(metric,qBound(0,value,999));}
void GraphicsDiagnostics::reportRuntime(){
    emitMetric(GraphicsMetric::Protocol,1);
    const auto platform=QApplication::platformName();emitMetric(GraphicsMetric::Platform,platform=="wayland"?1:platform=="xcb"?2:platform=="offscreen"?3:0);
    const auto parts=QString::fromLatin1(qVersion()).split('.');bool valid=false;
    const int minor=parts.size()>1?parts[1].toInt(&valid):0;if(valid)emitMetric(GraphicsMetric::QtMinor,minor);
    if(auto *screen=QApplication::primaryScreen()){
        emitMetric(GraphicsMetric::ScreenWidth10px,qRound(screen->size().width()/10.0));
        emitMetric(GraphicsMetric::ScreenHeight10px,qRound(screen->size().height()/10.0));emitMetric(GraphicsMetric::RefreshHz,qRound(screen->refreshRate()));
    }
    const QFileInfo driver("/sys/class/drm/renderD128/device/driver");
    emitMetric(GraphicsMetric::GpuDriver,!driver.exists()?0:QFileInfo(driver.canonicalFilePath()).fileName()=="lima"?1:2);
}
void GraphicsDiagnostics::watch(QScrollArea &area){
    if(!enabled)return;
    auto *viewport=area.viewport();
    if(areas.contains(viewport))return;
    areas.insert(viewport,&area);viewportOrigins.insert(viewport,viewport->pos());viewport->installEventFilter(this);
    connect(viewport,&QObject::destroyed,this,[this,viewport]{areas.remove(viewport);viewportOrigins.remove(viewport);});
    if(auto *content=area.widget()){
        areas.insert(content,&area);content->installEventFilter(this);
        connect(content,&QObject::destroyed,this,[this,content]{areas.remove(content);});
    }
    const auto changed=[this,&area]{
        beginScroll();++moves;
        const auto content=area.widget();if(content){
            const int top=content->y(),minimum=qMin(0,area.viewport()->height()-content->height());
            maxOvershoot=qMax(maxOvershoot,qMax(top,qMax(0,minimum-top)));
        }
    };
    connect(area.verticalScrollBar(),&QScrollBar::valueChanged,this,changed);
    connect(area.horizontalScrollBar(),&QScrollBar::valueChanged,this,changed);
    connect(QScroller::scroller(viewport),&QScroller::stateChanged,this,[this,viewport](QScroller::State state){
        if(state==QScroller::Pressed)viewportOrigins[viewport]=areas[viewport]->viewport()->pos();
        if(state!=QScroller::Inactive)beginScroll();
        emitMetric(GraphicsMetric::ScrollState,stateCode(state));
    });
}
void GraphicsDiagnostics::beginScroll(){
    if(!scrollClock.isValid()){scrollClock.start();maxGap=0;paints=0;maxOvershoot=0;}
    quietTimer.start(1000);
}
bool GraphicsDiagnostics::eventFilter(QObject *object,QEvent *event){
    if(auto *area=areas.value(object,nullptr)){
        if(object==area->viewport()&&event->type()==QEvent::Move){
            if(scrollClock.isValid()||QScroller::scroller(area->viewport())->state()!=QScroller::Inactive){
                beginScroll();maxOvershoot=qMax(maxOvershoot,(area->viewport()->pos()-viewportOrigins.value(object)).manhattanLength());
            }else viewportOrigins[object]=area->viewport()->pos();
        }
        if(scrollClock.isValid()&&event->type()==QEvent::Paint)++paints;
    }
    return QObject::eventFilter(object,event);
}
void GraphicsDiagnostics::flush(){
    if(!enabled)return;
    if(scrollClock.isValid()){
        emitMetric(GraphicsMetric::ScrollDuration10ms,qRound(scrollClock.elapsed()/10.0));
        emitMetric(GraphicsMetric::ScrollMoves,moves);emitMetric(GraphicsMetric::ViewportPaints,paints);
        emitMetric(GraphicsMetric::UiGapMs,maxGap);emitMetric(GraphicsMetric::OvershootPx,maxOvershoot);
        scrollClock.invalidate();moves=paints=maxGap=maxOvershoot=0;
    }
    int code=0;
    for(auto &count:warnings){++code;const auto total=count.exchange(0,std::memory_order_relaxed);if(total)emitMetric(GraphicsMetric::QtWarning,code*100+qMin(total,99U));}
}
void GraphicsDiagnostics::readKernel(){
    if(kernel.state()!=QProcess::NotRunning)return;
    kernelBytes.clear();kernelErrors.clear();kernelOverflow=false;
    kernel.setProgram("/usr/bin/journalctl");
    kernel.setArguments({"--kernel","--since=-1min","--no-pager","--output=cat","-n","100","--grep=(?i)(drm|lima|gpu|dsi|tcon)"});
    kernel.start();kernelDeadline.start(2000);
}
void GraphicsDiagnostics::finishKernel(int exitCode,QProcess::ExitStatus status){
    kernelDeadline.stop();kernelErrors+=kernel.readAllStandardError().left(4096-kernelErrors.size());
    kernel.setReadChannel(QProcess::StandardOutput);kernelBytes+=kernel.read(65537-kernelBytes.size());
    const bool denied=kernelErrors.contains("permission")||kernelErrors.contains("Permission")||kernelErrors.contains("No journal files")||kernelErrors.contains("not seeing messages");
    const int access=denied?2:status!=QProcess::NormalExit||kernelOverflow||kernelBytes.size()>65536||exitCode>1?4:exitCode==1?5:1;
    if(access!=lastKernelAccess){lastKernelAccess=access;emitMetric(GraphicsMetric::KernelAccess,access);}
    if(access==1){int mask=0,count=0;for(const auto &line:kernelBytes.split('\n'))if(const int fault=graphicsKernelFaultMask(line)){mask|=fault;++count;}
        if(count){emitMetric(GraphicsMetric::KernelFaultMask,mask);emitMetric(GraphicsMetric::KernelFaultCount,count);}
    }
    kernelBytes.fill('\0');kernelBytes.clear();kernelErrors.fill('\0');kernelErrors.clear();
}
