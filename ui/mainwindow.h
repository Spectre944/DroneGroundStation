#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>

#include "map/mapwidget.h"
#include <backend/settings/settings.h>

#include "ui/modules/indicatorPanel/indicatorpanel.h"

class QAction;
class QActionGroup;
class QSplitter;
class QStackedWidget;
class MapPicker;
class ReconPage;
class DatabasePage;
class InstrumentsPage;
class SettingsPage;
class AlarmDrawer;
class DroneCore;
class QLabel;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // Страница слева от карты (как в РХБ): QSplitter [ QStackedWidget | карта ].
    // Пункт меню переключает страницу; повторное нажатие на открытую — сворачивает стек.
    struct Page {
        QAction *action = nullptr;  // пункт меню (checkable)
        QWidget *widget = nullptr;
    };

    void setupMap();
    void setupPages();
    void setupMenu();
    void setupAlarms();
    void setupCore();       ///< создание ядра и подключение его сигналов

    void addPage(const QString &title, QWidget *widget);
    void togglePage(int index);
    void showPage(int index);
    void hidePages();

    void restoreSplitter();
    void saveSplitter();

    Ui::MainWindow *ui;

    MapWidget *mapWidget = nullptr;
    MapPicker *mapPicker = nullptr;

    QSplitter      *mainSplitter = nullptr;
    QStackedWidget *pageStack    = nullptr;
    QActionGroup   *pageActions  = nullptr;
    QList<Page>     pages;
    QList<int>      lastSizes;              // ширины до сворачивания стека

    ReconPage       *reconPage       = nullptr;
    DatabasePage    *databasePage    = nullptr;
    InstrumentsPage *instrumentsPage = nullptr;
    SettingsPage    *settingsPage    = nullptr;

    IndicatorPanel *indicatorPanel = nullptr;
    AlarmDrawer    *alarmDrawer    = nullptr;

    // --- ядро ---
    DroneCore *core      = nullptr;     ///< фасад ядра; удаляется вместе с окном
    QLabel    *linkLabel = nullptr;     ///< строка состояния: связь
    QLabel    *modeLabel = nullptr;     ///< строка состояния: режим логгера

};
#endif // MAINWINDOW_H
