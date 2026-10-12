#include <QCoreApplication>
#include <QTranslator>
#include <cstdio>
#include "nm/util/NmTr.hpp"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (!nm::useChineseUi("auto", "zh_CN") || !nm::useChineseUi("auto", "zh_TW")
            || !nm::useChineseUi("auto", "zh_HK") || !nm::useChineseUi("auto", "ZH_CN")
            || nm::useChineseUi("auto", "en_US") || nm::useChineseUi("auto", "ja_JP")
            || nm::useChineseUi("auto", "fr_FR") || nm::useChineseUi("auto", "C")
            || nm::useChineseUi("en", "zh_CN") || !nm::useChineseUi("zh", "en_US"))
        return 7;
    QTranslator translator;
    if (!translator.load("translations/NeteaseMusic_en.qm"))
        return 1;
    const QString chinese = nm::nmFromHex("E69CAAE59CA8E692ADE694BE");
    if (NmTr("E69CAAE59CA8E692ADE694BE") != chinese)
        return 2;
    app.installTranslator(&translator);
    if (NmTr("E69CAAE59CA8E692ADE694BE") != QLatin1String("Nothing is playing"))
        return 3;
    const QByteArray search = nm::nmFromHex("E6909CE7B4A2").toUtf8();
    if (QCoreApplication::translate("main", search.constData(), 0, QCoreApplication::UnicodeUTF8)
            != QLatin1String("Search"))
        return 4;
    if (NmTr("253120E58886E9929FE5898D").arg(3) != QLatin1String("3 minutes ago"))
        return 5;
    app.removeTranslator(&translator);
    if (NmTr("E69CAAE59CA8E692ADE694BE") != chinese)
        return 6;
    std::puts("PASS: Qt4 English catalog, QML context, placeholders and Chinese fallback");
    return 0;
}
