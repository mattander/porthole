#pragma once
#include "wifi.h"
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonObject>
#include <functional>

struct SettingsActions {
    std::function<void()> signIn,signOut,changeCalendars,useLive,useSample,refresh;
    std::function<void(bool)> theme;
    std::function<void(bool,QString,int)> briefing;
    std::function<void()> refreshBriefing,forgetBriefingKey,clearDiagnosticHistory;
    std::function<void(QJsonObject)> profile;
};
class SettingsPane : public QDialog {
public:
    SettingsPane(WifiController *wifi,const SettingsActions &actions,QWidget *parent);
    void updateState(const QString &calendar,const QString &weather,const QStringList &calendars,bool dark,bool demo,bool hasAccount=false,bool canChoose=false);
    void updateProfile(const QJsonObject &profile);
    void updateBriefing(const QJsonObject &state);
    void updateDiagnostics(const QJsonObject &state);
private:
    QLabel *calendarStatus,*weatherStatus,*calendarNames,*modeStatus;
    QPushButton *lightButton,*darkButton,*signOutButton,*chooseButton;
    QLabel *briefingStatus,*briefingSpending;
    QCheckBox *briefingEnabled;
    QLineEdit *briefingPriorities;
    QComboBox *briefingBudget;
    bool briefingInitialized=false;
    QLabel *diagnosticState,*diagnosticSources,*diagnosticHealth,*diagnosticHistory;
    QJsonObject lastDiagnostics,lastProfile;
    QLabel *profileInfo;
    QLineEdit *householdContext;
};
