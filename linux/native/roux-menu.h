#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QPushButton>
#include <QUrl>
#include <optional>

struct RouxMeal { QDate date; QString title; };
struct RouxMenu { QDateTime updatedAt; QString timezone; QList<RouxMeal> meals; };
std::optional<RouxMenu> parseRouxMenu(const QByteArray &bytes, QDateTime now);
QJsonObject rouxMenuJson(const RouxMenu &menu);
struct RouxConnection { QUrl url; QByteArray token; };
std::optional<RouxConnection> loadRouxConnection(const QString &path);
class RouxMenuCard : public QPushButton {
public:
    explicit RouxMenuCard(bool demo, QWidget *parent=nullptr);
    void refresh();
    void updateDisplay();
    void setMenuForPreview(const RouxMenu &value);
private:
    void showWeek();
    bool demoMode=false;
    bool failed=false;
    QNetworkAccessManager network{this};
    QPointer<QNetworkReply> pending;
    std::optional<RouxConnection> connection;
    std::optional<RouxMenu> menu;
    QDateTime fetchedAt;
    QString cachePath;
    void loadCache();
    void saveCache();
};
