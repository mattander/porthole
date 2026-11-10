#include "roux-menu.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
class RouxTests:public QObject{
    Q_OBJECT
private slots:
    void valid(){const auto now=QDateTime::currentDateTimeUtc();RouxMenu source{now,"UTC",{{now.date(),"Frittata"}}};const auto parsed=parseRouxMenu(QJsonDocument(rouxMenuJson(source)).toJson(),now);QVERIFY(parsed);QCOMPARE(parsed->meals.size(),1);QCOMPARE(parsed->meals.first().title,"Frittata");}
    void rejectsMalformed(){const auto now=QDateTime::currentDateTimeUtc();QVERIFY(!parseRouxMenu("[]",now));QVERIFY(!parseRouxMenu(QByteArray(32769,'x'),now));QVERIFY(!parseRouxMenu("{}",now));RouxMenu source{now.addSecs(600),"UTC",{}};QVERIFY(!parseRouxMenu(QJsonDocument(rouxMenuJson(source)).toJson(),now));source.updatedAt=now;source.timezone="No/Such";QVERIFY(!parseRouxMenu(QJsonDocument(rouxMenuJson(source)).toJson(),now));}
    void duplicateDates(){const auto now=QDateTime::currentDateTimeUtc();RouxMenu source{now,"UTC",{{now.date(),"One"},{now.date(),"Two"}}};QVERIFY(!parseRouxMenu(QJsonDocument(rouxMenuJson(source)).toJson(),now));}
    void controls(){const auto now=QDateTime::currentDateTimeUtc();RouxMenu source{now,"UTC",{{now.date(),"Dinner\nHidden"}}};QVERIFY(!parseRouxMenu(QJsonDocument(rouxMenuJson(source)).toJson(),now));}
    void connectionBoundary(){QTemporaryDir dir;const auto path=dir.filePath("roux.json");auto write=[&](const QString &url){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);f.write(QJsonDocument(QJsonObject{{"url",url},{"token",QString(48,'a')}}).toJson());f.close();};write("https://example.test/v1/porthole/menu");QVERIFY(loadRouxConnection(path));write("http://example.test/v1/porthole/menu");QVERIFY(!loadRouxConnection(path));write("https://example.test/v1/porthole/menu?secret=x");QVERIFY(!loadRouxConnection(path));write("https://user:password@example.test/v1/porthole/menu");QVERIFY(!loadRouxConnection(path));write("https://example.test/v1/porthole/menu");QFile::setPermissions(path,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ReadOther);QVERIFY(!loadRouxConnection(path));}
    void preview(){RouxMenuCard card(true);QVERIFY(card.isHidden());const auto now=QDateTime::currentDateTimeUtc();card.setMenuForPreview({now,"UTC",{{now.date(),"Frittata"}}});QVERIFY(!card.isHidden());QVERIFY(card.text().contains("Frittata"));QVERIFY(card.text().contains("Sample"));}
};
QTEST_MAIN(RouxTests)
#include "roux-menu-tests.moc"
