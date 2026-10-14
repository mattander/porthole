#include "profile.h"
#include "keyboard.h"
#include "settings.h"
#include "diagnostics.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QDialogButtonBox>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTimer>
#include <QApplication>
#include <QtTest>

namespace {
QPushButton *named(QWidget &widget,const QString &name){
    for(auto *button:widget.findChildren<QPushButton*>())if(button->text()==name)return button;
    return nullptr;
}
}
class SettingsTests:public QObject {
    Q_OBJECT
private slots:
    void layoutsAndActions(){
        QWidget parent;parent.resize(1280,800);WifiController wifi(nullptr,true);
        int signedIn=0,signedOut=0,live=0,sample=0,refresh=0;bool dark=false;
        SettingsActions actions{[&]{++signedIn;},[&]{++signedOut;},[]{},[&]{++live;},[&]{++sample;},[&]{++refresh;},[&](bool value){dark=value;}};
        SettingsPane pane(&wifi,actions,&parent);pane.updateState("Disconnected","Offline",{},false,true);pane.show();QTest::qWait(30);
        auto *tabs=pane.findChild<QTabWidget*>();QVERIFY(tabs);QCOMPARE(tabs->count(),6);
        for(const auto size:{QSize(1280,800),QSize(800,1280)}){
            pane.resize(size);QTest::qWait(20);QCOMPARE(pane.size(),size);
            for(int i=0;i<6;++i){tabs->setCurrentIndex(i);QTest::qWait(10);
                for(auto *button:tabs->currentWidget()->findChildren<QPushButton*>())if(button->isVisible()){
                    const auto rect=QRect(button->mapTo(&pane,QPoint()),button->size());
                    if(i<3)QVERIFY2(pane.rect().contains(rect),qPrintable(button->text()));QVERIFY(button->height()>=44);
                }
            }
        }
        tabs->setCurrentIndex(1);QTest::mouseClick(named(pane,"Connect iCloud"),Qt::LeftButton);QCOMPARE(signedIn,1);
        QVERIFY(!named(pane,"Forget account")->isEnabled());pane.updateState("Connected","Offline",{"Family"},false,false);
        QTest::mouseClick(named(pane,"Forget account"),Qt::LeftButton);QCOMPARE(signedOut,1);
        tabs->setCurrentIndex(2);QTest::mouseClick(named(pane,"Dark"),Qt::LeftButton);QVERIFY(dark);
        QTest::mouseClick(named(pane,"Light"),Qt::LeftButton);QVERIFY(!dark);
        tabs->setCurrentIndex(3);QTest::mouseClick(named(pane,"Use live data"),Qt::LeftButton);QCOMPARE(live,1);
        QTest::mouseClick(named(pane,"View sample data"),Qt::LeftButton);QCOMPARE(sample,1);
        QTest::mouseClick(named(pane,"Refresh live data"),Qt::LeftButton);QCOMPARE(refresh,1);
    }
    void profileEditorValidatesAndPreservesHouseholdContext(){
        QWidget parent;parent.resize(1280,800);WifiController wifi(nullptr,true);QJsonObject saved;int calls=0;SettingsActions actions;actions.profile=[&](QJsonObject value){saved=value;++calls;};SettingsPane pane(&wifi,actions,&parent);
        auto initial=HouseholdProfile().json();initial["familyContext"]="Student One's school days matter";pane.updateProfile(initial);pane.show();auto *tabs=pane.findChild<QTabWidget*>();tabs->setCurrentIndex(3);
        for(const auto size:{QSize(1280,800),QSize(800,1280)}){
            pane.resize(size);QTimer::singleShot(0,&pane,[&]{
                auto *dialog=QApplication::activeModalWidget();QVERIFY(dialog);QTimer::singleShot(3000,dialog,[dialog]{dialog->close();});QTest::qWait(30);QCOMPARE(dialog->size(),size);
                for(auto *button:dialog->findChildren<QPushButton*>()){const auto rect=QRect(button->mapTo(dialog,QPoint()),button->size());QVERIFY2(dialog->rect().contains(rect),qPrintable(QString("%1: %2,%3 %4x%5 in %6x%7").arg(button->text()).arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).arg(dialog->width()).arg(dialog->height())));QVERIFY(button->height()>=44);}
                auto *label=dialog->findChild<QLineEdit*>("profileLocation"),*zone=dialog->findChild<QLineEdit*>("profileTimezone"),*lat=dialog->findChild<QLineEdit*>("profileLatitude"),*lon=dialog->findChild<QLineEdit*>("profileLongitude");QVERIFY(label&&zone&&lat&&lon);
                label->setText("Example City");zone->setText("Bad/Timezone");lat->setText("51.5");lon->setText("-0.12");const auto before=calls;QTest::mouseClick(named(*dialog,"Save profile"),Qt::LeftButton);QCOMPARE(calls,before);QVERIFY(!dialog->findChild<QLabel*>("profileError")->text().isEmpty());
                zone->setText("Europe/London");QTest::mouseClick(named(*dialog,"Save profile"),Qt::LeftButton);
            });QTest::mouseClick(named(pane,"Edit location and timezone"),Qt::LeftButton);QCOMPARE(saved["familyContext"].toString(),initial["familyContext"].toString());QCOMPARE(saved["timezone"].toString(),QString("Europe/London"));
        }
        QCOMPARE(calls,2);
    }
    void diagnosticPaneShowsErrorsAndCopiesOnlySafeState(){
        QWidget parent;parent.resize(1280,800);WifiController wifi(nullptr,true);SettingsActions actions;actions.refresh=[]{};
        SettingsPane pane(&wifi,actions,&parent);Diagnostics diagnostics;diagnostics.fail(DataSource::Weather,IssueCode::WeatherUnavailable);auto state=diagnostics.snapshot();state["password"]="private-secret";pane.updateDiagnostics(state);pane.show();
        auto *tabs=pane.findChild<QTabWidget*>();tabs->setCurrentIndex(5);QTest::qWait(20);QVERIFY(pane.findChild<QLabel*>("diagnosticState")->text().contains("1 issue"));
        QTest::mouseClick(named(pane,"Copy safe diagnostics"),Qt::LeftButton);const auto copied=QGuiApplication::clipboard()->text();QVERIFY(copied.contains("Weather refresh failed"));QVERIFY(!copied.contains("private-secret"));
        diagnostics.recover(DataSource::Weather);pane.updateDiagnostics(diagnostics.snapshot());QCOMPARE(pane.findChild<QLabel*>("diagnosticState")->text(),QString("No active issues"));
    }
    void icloudPasswordVisibilityAndRememberDefault(){
        SignIn dialog(nullptr);dialog.show();QTest::qWait(20);
        QCOMPARE(dialog.password->echoMode(),QLineEdit::Password);QVERIFY(dialog.remember->isChecked());
        QCheckBox *show=nullptr;for(auto *box:dialog.findChildren<QCheckBox*>())if(box->text()=="Show password")show=box;
        QVERIFY(show);QTest::mouseClick(show,Qt::LeftButton);QCOMPARE(dialog.password->echoMode(),QLineEdit::Normal);
        QTest::mouseClick(show,Qt::LeftButton);QCOMPARE(dialog.password->echoMode(),QLineEdit::Password);
    }
    void touchKeyboardCaseAndSymbols(){
        WifiPassword dialog("Test network",nullptr);dialog.show();QTest::qWait(20);
        QTest::mouseClick(named(dialog,"Shift"),Qt::LeftButton);QTest::mouseClick(named(dialog,"A"),Qt::LeftButton);
        QTest::mouseClick(named(dialog,"Symbols"),Qt::LeftButton);QTest::mouseClick(named(dialog,"@"),Qt::LeftButton);
        QTest::mouseClick(named(dialog,"Space"),Qt::LeftButton);QCOMPARE(dialog.password->text(),QString("A@ "));
        QTest::mouseClick(named(dialog,"Backspace"),Qt::LeftButton);QCOMPARE(dialog.password->text(),QString("A@"));
        QCOMPARE(dialog.password->echoMode(),QLineEdit::Password);
    }
    void passwordBoundaries(){
        for(const QString &value:{QString("short"),QString(64,'g'),QString(64,'a'),QString("Valid test passphrase")}){
            WifiPassword dialog("Test network",nullptr);QSignalSpy done(&dialog,&QDialog::finished);dialog.show();QTest::qWait(10);dialog.password->setText(value);
            QTest::mouseClick(named(dialog,"Join network"),Qt::LeftButton);
            const bool valid=value==QString(64,'a')||value=="Valid test passphrase";QCOMPARE(done.count(),valid?1:0);
            if(valid)QCOMPARE(done.first().first().toInt(),int(QDialog::Accepted));
        }
    }
};
QTEST_MAIN(SettingsTests)
#include "ui-tests.moc"
