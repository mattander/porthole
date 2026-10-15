#include "typography.h"
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
#include <QFontInfo>
#include <QPlainTextEdit>
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
    void initTestCase(){QVERIFY(configureInterfaceTypography());QCOMPARE(QFontInfo(QApplication::font()).family(),QString("Poppins"));}
    void checkboxesToggleFromTheirLabelAndEmptyRowSpaceExactlyOnce(){
        TouchCheckBox box("A calendar or setting");box.resize(620,56);box.show();QTest::qWait(10);QSignalSpy changes(&box,&QCheckBox::toggled);
        for(const auto point:{QPoint(12,28),QPoint(120,28),QPoint(610,28)}){
            const bool previous=box.isChecked();const auto count=changes.count();QTest::mouseClick(&box,Qt::LeftButton,Qt::NoModifier,point);
            QCOMPARE(box.isChecked(),!previous);QCOMPARE(changes.count(),count+1);
        }
        box.setFocus();const auto count=changes.count();QTest::keyClick(&box,Qt::Key_Space);QCOMPARE(changes.count(),count+1);
        box.setEnabled(false);QTest::mouseClick(&box,Qt::LeftButton,Qt::NoModifier,QPoint(120,28));QCOMPARE(changes.count(),count+1);
    }
    void calendarPickerRowsPreserveIdentityAndEnforceSelectionLimits(){
        QList<Calendar> available;for(int i=0;i<18;++i)available.append(Calendar{QUrl("googlecalendar:/fixture-"+QString::number(i)),i==0?QString("School & lunch"):QString("Calendar %1").arg(i),"#FFFF00"});
        for(const auto size:{QSize(1280,800),QSize(800,1280)}){
            CalendarPicker picker(available,{available[1]});picker.resize(size);picker.show();QTest::qWait(10);
            const auto rows=picker.findChildren<QCheckBox*>();QCOMPARE(rows.size(),18);QVERIFY(rows[1]->isChecked());QVERIFY(!rows[0]->isChecked());QCOMPARE(rows[0]->accessibleName(),QString("School & lunch"));
            for(auto *row:rows)QVERIFY(row->height()>=56);
            QSignalSpy changes(rows[0],&QCheckBox::toggled);QTest::mouseClick(rows[0],Qt::LeftButton,Qt::NoModifier,QPoint(110,28));QCOMPARE(changes.count(),1);QVERIFY(rows[0]->isChecked());
            QTest::mouseClick(rows[0],Qt::LeftButton,Qt::NoModifier,QPoint(rows[0]->width()-8,28));QCOMPARE(changes.count(),2);QVERIFY(!rows[0]->isChecked());
            auto *buttons=picker.findChild<QDialogButtonBox*>();QVERIFY(buttons);auto *ok=buttons->button(QDialogButtonBox::Ok);QVERIFY(ok->isEnabled());
            rows[1]->setChecked(false);QVERIFY(!ok->isEnabled());for(int i=0;i<16;++i)rows[i]->setChecked(true);QVERIFY(ok->isEnabled());rows[16]->setChecked(true);QVERIFY(!ok->isEnabled());rows[16]->setChecked(false);
            QCOMPARE(picker.chosenCalendars().size(),16);QCOMPARE(picker.chosenCalendars().first().url,available.first().url);
            QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);QCOMPARE(picker.result(),int(QDialog::Rejected));
        }
    }
    void fontLicenceIsReadableOnTheFrame(){
        QWidget parent;parent.resize(800,1280);WifiController wifi(nullptr,true);SettingsPane pane(&wifi,{},&parent);pane.show();pane.findChild<QTabWidget*>()->setCurrentIndex(2);
        QTimer::singleShot(0,&pane,[&]{
            auto *dialog=QApplication::activeModalWidget();QVERIFY(dialog);QTimer::singleShot(3000,dialog,&QDialog::close);
            auto *licence=dialog->findChild<QPlainTextEdit*>("fontLicenceText");QVERIFY(licence);QVERIFY(licence->isReadOnly());QVERIFY(licence->toPlainText().contains("SIL OPEN FONT LICENSE Version 1.1"));QVERIFY(licence->toPlainText().contains("Poppins Project Authors"));dialog->close();
        });QTest::mouseClick(named(pane,"Font licences"),Qt::LeftButton);
    }
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
    void sunriseSettingReflectsStateWithoutEmittingUserChanges(){
        QWidget parent;parent.resize(1280,800);WifiController wifi(nullptr,true);int changes=0;bool enabled=false;SettingsActions actions;actions.followSun=[&](bool value){++changes;enabled=value;};
        SettingsPane pane(&wifi,actions,&parent);pane.show();pane.findChild<QTabWidget*>()->setCurrentIndex(2);
        auto *follow=pane.findChild<QCheckBox*>("followSun");auto *status=pane.findChild<QLabel*>("sunThemeStatus");QVERIFY(follow);QVERIFY(status);
        pane.updateAppearance(true,"Light until sunset at 6:35 PM");QCOMPARE(changes,0);QVERIFY(follow->isChecked());QVERIFY(status->isVisible());QVERIFY(named(pane,"Light")->isEnabled());QVERIFY(named(pane,"Dark")->isEnabled());
        QTest::mouseClick(follow,Qt::LeftButton);QCOMPARE(changes,1);QVERIFY(!enabled);
        pane.updateAppearance(false,{});QCOMPARE(changes,1);QVERIFY(!status->isVisible());
        for(const auto size:{QSize(1280,800),QSize(800,1280)}){pane.resize(size);pane.updateAppearance(true,"Waiting for today's sun times. Keeping the current theme.");QTest::qWait(10);QVERIFY(follow->height()>=48);QVERIFY(pane.rect().contains(QRect(follow->mapTo(&pane,QPoint()),follow->size())));}
    }
    void googleControlsAreIndependentAndFitBothOrientations(){
        QWidget parent;parent.resize(1280,800);WifiController wifi(nullptr,true);int setup=0,choose=0,forget=0;SettingsActions actions;actions.setupGoogle=[&]{++setup;};actions.chooseGoogle=[&]{++choose;};actions.forgetGoogle=[&]{++forget;};SettingsPane pane(&wifi,actions,&parent);pane.show();QTest::qWait(20);
        const auto tabs=pane.findChildren<QTabWidget*>();QCOMPARE(tabs.size(),2);tabs[0]->setCurrentIndex(1);tabs[1]->setCurrentIndex(1);QVERIFY(!named(pane,"Forget Google")->isEnabled());
        pane.updateGoogle({{"authenticated",true},{"status","Google connected"},{"available",QJsonArray{QJsonObject{{"url","googlecalendar:/fixture"},{"name","Work"}}}}});
        for(const auto size:{QSize(1280,800),QSize(800,1280)}){pane.resize(size);QTest::qWait(20);QCOMPARE(pane.size(),size);for(auto *button:tabs[1]->currentWidget()->findChildren<QPushButton*>()){QVERIFY(pane.rect().contains(QRect(button->mapTo(&pane,QPoint()),button->size())));QVERIFY(button->height()>=44);}}
        QTest::mouseClick(named(pane,"Connect Google"),Qt::LeftButton);QTest::mouseClick(named(pane,"Choose Google calendars"),Qt::LeftButton);QTest::mouseClick(named(pane,"Forget Google"),Qt::LeftButton);QCOMPARE(setup,1);QCOMPARE(choose,1);QCOMPARE(forget,1);
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
    void softwareControlsUseCallbacksAndSafeStates(){
        QWidget parent;parent.resize(1280,800);WifiController wifi(nullptr,true);int checks=0,toggles=0;bool enabled=true;
        SettingsActions actions;actions.checkUpdates=[&]{++checks;};actions.automaticUpdates=[&](bool value){++toggles;enabled=value;};SettingsPane pane(&wifi,actions,&parent);pane.show();pane.findChild<QTabWidget*>()->setCurrentIndex(3);
        pane.updateTimes(QDateTime::currentDateTimeUtc(),{}, {},true,true);auto *updated=pane.findChild<QLabel*>("dataUpdated");QVERIFY(updated);QVERIFY(updated->text().contains("Calendars · "));QVERIFY(updated->text().contains("Weather · — (stale)"));QVERIFY(updated->text().contains("Briefing · —"));QVERIFY(updated->text().contains("fictional"));
        auto *button=pane.findChild<QPushButton*>("checkUpdates");auto *automatic=pane.findChild<QCheckBox*>("automaticUpdates");QVERIFY(button&&automatic);QVERIFY(!button->isEnabled());
        pane.updateSoftware(SoftwareUpdate{UpdatePhase::Current,UpdateFailure::None,true,"2026.10.14.1",{}});QCOMPARE(toggles,0);QVERIFY(button->isEnabled());QVERIFY(automatic->isChecked());
        QTest::mouseClick(button,Qt::LeftButton);QCOMPARE(checks,1);QTest::mouseClick(automatic,Qt::LeftButton);QCOMPARE(toggles,1);QVERIFY(!enabled);
        pane.updateSoftware(SoftwareUpdate{UpdatePhase::Installing,UpdateFailure::None,false,"2026.10.14.1",{}});QVERIFY(!button->isEnabled());QCOMPARE(toggles,1);
        pane.updateSoftware(SoftwareUpdate{UpdatePhase::RolledBack,UpdateFailure::Health,false,"2026.10.14.1",{}});QVERIFY(pane.findChild<QLabel*>("softwareStatus")->text().contains("Previous version restored"));QVERIFY(button->isEnabled());
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
