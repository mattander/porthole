#include "graphics-diagnostics.h"
#include <QApplication>
#include <QJsonDocument>
#include <QLabel>
#include <QLoggingCategory>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QtTest>

class GraphicsTests:public QObject {
    Q_OBJECT
private slots:
    void recordsFitTheUnchangedLegacyExporter(){
        const auto record=graphicsDiagnosticRecord(GraphicsMetric::UiGapMs,250);QVERIFY(record);
        QCOMPARE(record->size(),4);QCOMPARE((*record)["event"].toString(),QString("calendar_ready"));
        QCOMPARE((*record)["http"].toInt(),570);QCOMPARE((*record)["network"].toInt(),250);
        QVERIFY(QJsonDocument(*record).toJson(QJsonDocument::Compact).size()<180);
        QVERIFY(!graphicsDiagnosticRecord(GraphicsMetric::UiGapMs,-1));QVERIFY(!graphicsDiagnosticRecord(GraphicsMetric::UiGapMs,1000));
    }
    void rawWarningsOnlyBecomeFixedCategories(){
        QVERIFY(!graphicsWarningCode("qt.network","private@example.invalid password=secret"));
        QCOMPARE(graphicsWarningCode("qt.qpa.wayland","Buffer release timed out: secret").value_or(0),1);
        QCOMPARE(graphicsWarningCode("qt.qpa.wayland","Buffer creation failed for private title").value_or(0),2);
        QCOMPARE(graphicsWarningCode("default","EGL_BAD_ALLOC private data").value_or(0),3);
        QCOMPARE(graphicsWarningCode("qt.qpa.backingstore","Paint failed").value_or(0),4);
        QCOMPARE(graphicsKernelFaultMask("sun4i-drm DSI FIFO underflow"),1);
        QCOMPARE(graphicsKernelFaultMask("lima GPU fault and reset"),20);
        QCOMPARE(graphicsKernelFaultMask("private calendar request failed"),0);
        QCOMPARE(graphicsKernelFaultMask("drm selected default mode"),0);
    }
    void scrollWindowsExportActivityAndTimerStalls(){
        QList<QPair<GraphicsMetric,int>> records;
        GraphicsDiagnostics diagnostics(true,nullptr,[&](GraphicsMetric metric,int value){records.append({metric,value});});
        QScrollArea area;auto *content=new QWidget;auto *layout=new QVBoxLayout(content);
        for(int i=0;i<30;++i){auto *row=new QLabel(QString("Fictional row %1").arg(i));row->setMinimumHeight(60);layout->addWidget(row);}
        area.setWidgetResizable(true);area.setWidget(content);area.resize(400,240);area.show();diagnostics.watch(area);
        QTest::qWait(70);QVERIFY(area.verticalScrollBar()->maximum()>0);records.clear();
        area.verticalScrollBar()->setValue(120);QTest::qWait(60);QTest::qSleep(180);QTest::qWait(60);
        diagnostics.flush();
        const auto value=[&](GraphicsMetric metric){for(const auto &record:records)if(record.first==metric)return record.second;return -1;};
        QCOMPARE(value(GraphicsMetric::ScrollMoves),1);QVERIFY(value(GraphicsMetric::ViewportPaints)>0);
        QVERIFY(value(GraphicsMetric::UiGapMs)>=150);QCOMPARE(value(GraphicsMetric::OvershootPx),0);
        for(const auto &record:records)QVERIFY(record.second>=0&&record.second<=999);
        records.clear();diagnostics.flush();QVERIFY(records.isEmpty());
        area.verticalScrollBar()->setValue(140);area.viewport()->move(area.viewport()->pos()+QPoint(0,35));
        QTest::qWait(10);diagnostics.flush();QCOMPARE(value(GraphicsMetric::OvershootPx),35);
    }
    void qtGraphicsWarningIsCollectedWithoutText(){
        QList<int> warningRecords;
        GraphicsDiagnostics diagnostics(true,nullptr,[&](GraphicsMetric metric,int value){if(metric==GraphicsMetric::QtWarning)warningRecords.append(value);});
        QLoggingCategory category("qt.qpa.wayland");
        qCWarning(category)<<"Buffer creation failed: private-title secret-token";
        diagnostics.flush();QCOMPARE(warningRecords,QList<int>{201});
        diagnostics.flush();QCOMPARE(warningRecords.size(),1);
    }
    void demoDoesNotExportOrSpawnCollectors(){
        int records=0;GraphicsDiagnostics diagnostics(false,nullptr,[&](GraphicsMetric,int){++records;});
        QScrollArea area;auto *content=new QWidget;content->setMinimumSize(300,2000);area.setWidget(content);area.resize(400,240);area.show();diagnostics.watch(area);
        area.verticalScrollBar()->setValue(120);QTest::qWait(70);diagnostics.flush();QCOMPARE(records,0);
    }
};
QTEST_MAIN(GraphicsTests)
#include "graphics-diagnostics-tests.moc"
