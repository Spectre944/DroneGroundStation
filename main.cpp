#include "mainwindow.h"

#include <QGuiApplication>
#include <QApplication>
#include <QStyle>
#include <QSplashScreen>
#include <QTranslator>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    QPixmap pixmap(":/icons/png/old/SplashScreenTisa.png");
    QSplashScreen splash(pixmap);
    splash.show();
    //splash.showMessage("Loaded modules", Qt::AlignRight, Qt::white);

    // a.setWindowIcon(QIcon(":/icons/png/old/atom-2.png"));
    // fusion   windowsvista    macOS   windows
    // a.setStyle("fusion");   // For win10 - fusion, for win11 - windows

    // Get programm path where it launched
    QString programPath = QCoreApplication::applicationDirPath();

    // Class for storing settings
    Settings& settings = Settings::instance(programPath);

    QFont appFont = settings.font(FONT_SIZE::MEDIUM);
    a.setFont(appFont);

    // Мова інтерфейсу (Р2-16): «uk» — исходные тексты + перевод стандартных окон Qt,
    // «en» — английский перевод программы (:/i18n/..._en.qm, собирается из translations/*.ts).
    // Меняется в «Налаштування → Інтерфейс», действует после перезапуска.
    const QString language = settings.getValue("interface/language", QStringLiteral("uk")).toString();
    QTranslator qtTranslator;
    QTranslator appTranslator;
    if (language == QLatin1String("en")) {
        if (appTranslator.load(QStringLiteral(":/i18n/dronegroundstation_en.qm")))
            a.installTranslator(&appTranslator);
    } else if (qtTranslator.load(QStringLiteral(":/locale/qtbase_uk.qm"))) {
        a.installTranslator(&qtTranslator);
    }

    MainWindow w;
    w.setFont(appFont);
    w.show();
    splash.finish(&w);

    return a.exec();
}
