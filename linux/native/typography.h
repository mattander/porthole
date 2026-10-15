#pragma once
#include <QFont>
#include <QString>

bool configureInterfaceTypography();
QFont interfaceFont(int pixels,QFont::Weight weight=QFont::Normal);
QString interfaceFontLicence();
