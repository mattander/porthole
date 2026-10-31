#pragma once
#include "wifi.h"
#include "display-controls.h"
#include <QSlider>
#include "update-client.h"
#include <QDialog>
#include <QDateTime>
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
    std::function<void(AppearanceMode)> appearance;
    std::function<void(int)> brightness;
    std::function<void(bool,QString,int)> briefing;
    std::function<void()> refreshBriefing,forgetBriefingKey,clearDiagnosticHistory;
    std::function<void(QJsonObject)> profile;
    std::function<void()> checkUpdates;
    std::function<void(bool)> automaticUpdates;
    std::function<void()> setupGoogle,forgetGoogle,chooseGoogle;
};
class SettingsPane : public QDialog {
public:
    SettingsPane(WifiController *wifi,const SettingsActions &actions,QWidget *parent);
    void updateState(const QString &calendar,const QString &weather,const QStringList &calendars,bool dark,bool demo,bool hasAccount=false,bool canChoose=false);
    void updateGoogle(const QJsonObject &state);
    void updateProfile(const QJsonObject &profile);
    void updateBriefing(const QJsonObject &state);
    void updateDiagnostics(const QJsonObject &state);
    void updateSoftware(const std::optional<SoftwareUpdate> &state);
    void updateAppearance(AppearanceMode mode,const QString &status);
    void updateBrightness(const std::optional<BacklightState> &state,const QString &status={});
    void updateTimes(QDateTime calendar,QDateTime weather,QDateTime summary,bool weatherStale=false,bool sample=false,const QString &holidayStatus={});
private:
    QLabel *googleStatus;QPushButton *googleChoose,*googleForget;
    QLabel *calendarStatus,*weatherStatus,*calendarNames,*modeStatus;
    QPushButton *lightButton,*darkButton,*signOutButton,*chooseButton;
    QPushButton *manualMode,*ambientMode,*sunMode,*dimmer,*brighter;
    QSlider *brightness;
    QLabel *appearanceStatus,*brightnessValue,*brightnessStatus;
    QLabel *briefingStatus,*briefingSpending;
    QCheckBox *briefingEnabled;
    QLineEdit *briefingPriorities;
    QComboBox *briefingBudget;
    bool briefingInitialized=false;
    QLabel *diagnosticState,*diagnosticSources,*diagnosticHealth,*diagnosticHistory;
    QJsonObject lastDiagnostics,lastProfile;
    QLabel *profileInfo,*dataUpdated;
    QLineEdit *householdContext;
    QLabel *softwareStatus;
    QCheckBox *automaticUpdates;
    QPushButton *checkUpdates;
};
