#pragma once
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <optional>

enum class AppearanceMode {Manual,AmbientLight,SunSchedule};
AppearanceMode appearanceMode(const QSettings &settings);
void saveAppearanceMode(QSettings &settings,AppearanceMode mode,bool dark);

// Require continuously dim/bright readings, with a neutral band in between.
class AmbientTheme {
    struct Candidate {bool dark;qint64 since,last;};
    std::optional<Candidate> candidate;
public:
    std::optional<bool> observe(std::optional<double> lux,qint64 monotonicMs,bool dark);
    void reset(){candidate.reset();}
};
struct BacklightState {
    int level,maximum;
    bool writable;
    int percent()const;
};
class Backlight {
    QString directory;
public:
    explicit Backlight(QString directory="/sys/class/backlight/backlight");
    std::optional<BacklightState> state()const;
    std::optional<BacklightState> setLevel(int level)const;
    std::optional<BacklightState> setPercent(int percent)const;
};

// A bounded subprocess keeps a slow/missing kernel sensor off the UI thread.
// Only processed IIO illuminance inputs (lux) are used, never raw intensity.
class AmbientSensor:public QObject {
    Q_OBJECT
    QString directory;
    QProcess process;
    QTimer timeout;
    bool timedOut=false;
public:
    explicit AmbientSensor(QObject *parent=nullptr,QString directory="/sys/bus/iio/devices");
    void sample();
signals:
    void measured(double lux);
    void unavailable();
};
