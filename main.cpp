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

    QTranslator translator;
    if(!translator.load(":/locale/qtbase_uk.qm"))
        qDebug() << "Fail to load default translation";
    a.installTranslator(&translator);

    MainWindow w;
    w.setFont(appFont);
    w.show();
    splash.finish(&w);

    return a.exec();
}
