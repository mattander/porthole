#include "typography.h"
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QResource>

// Keep the resource symbol in the global namespace when linked from a static library.
void initializeFontResources(){Q_INIT_RESOURCE(fonts);}

bool configureInterfaceTypography(){
    static const bool loaded=[] {
        initializeFontResources();
        for(const auto *path:{":/fonts/Poppins-Regular.ttf",":/fonts/Poppins-SemiBold.ttf",":/fonts/Poppins-Bold.ttf"}){
            const int id=QFontDatabase::addApplicationFont(QString::fromLatin1(path));
            if(id<0||!QFontDatabase::applicationFontFamilies(id).contains("Poppins"))return false;
        }
        const int emoji=QFontDatabase::addApplicationFont(":/fonts/Noto-COLRv1.ttf");
        if(emoji<0||!QFontDatabase::applicationFontFamilies(emoji).contains("Noto Color Emoji"))return false;
        QFontDatabase::setApplicationEmojiFontFamilies({"Noto Color Emoji"});return true;
    }();
    if(loaded)QGuiApplication::setFont(interfaceFont(18));
    return loaded;
}
QFont interfaceFont(int pixels,QFont::Weight weight){
    QFont result("Poppins");result.setPixelSize(pixels);result.setWeight(weight);return result;
}
QString interfaceFontLicence(){
    initializeFontResources();QString result;
    for(const auto *path:{":/fonts/OFL.txt",":/fonts/Noto-Emoji-OFL.txt"}){
        QFile file(QString::fromLatin1(path));if(!file.open(QIODevice::ReadOnly))return {};
        if(!result.isEmpty())result+="\n\n";
        if(QString::fromLatin1(path).endsWith("Noto-Emoji-OFL.txt"))result+="Noto Color Emoji\nCopyright 2022 Google Inc.\n\n";
        result+=QString::fromUtf8(file.readAll());
    }
    return result;
}
