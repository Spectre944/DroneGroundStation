#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>

#include "map/mapwidget.h"
#include <backend/settings/settings.h>

#include "ui/windows/overlay/overlaycontainer.h"

class QAction;
class MapPicker;
class ReconPage;
class DatabasePage;
class InstrumentsPage;
class SettingsPage;
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
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // Боковая панель поверх карты: левая 1/3 окна на всю высоту.
    // Страница остаётся обычным виджетом - при необходимости её легко
    // перенести в QStackedWidget, ничего не меняя внутри неё.
    struct SidePanel {
        QAction          *action  = nullptr;   // пункт меню (checkable)
        OverlayContainer *overlay = nullptr;
        QWidget          *page    = nullptr;
    };

    void setupMap();
    void setupPages();
    void setupMenu();
    void setupCore();       ///< создание ядра и подключение его сигналов

    SidePanel *addSidePanel(const QString &title, QWidget *page);
    void togglePanel(SidePanel *panel);
    void showPanel(SidePanel *panel);
    void updateMapInsets();

    Ui::MainWindow *ui;

    MapWidget *mapWidget = nullptr;
    MapPicker *mapPicker = nullptr;

    ReconPage    *reconPage    = nullptr;
    DatabasePage *databasePage = nullptr;
    InstrumentsPage *instrumentsPage = nullptr;
    SettingsPage *settingsPage = nullptr;

    QList<SidePanel *> panels;
    SidePanel *reconPanel    = nullptr;
    SidePanel *databasePanel = nullptr;
    SidePanel *instrumentsPanel = nullptr;
    SidePanel *settingsPanel = nullptr;

    // --- ядро ---
    DroneCore *core      = nullptr;     ///< фасад ядра; удаляется вместе с окном
    QLabel    *linkLabel = nullptr;     ///< строка состояния: связь
    QLabel    *modeLabel = nullptr;     ///< строка состояния: режим логгера

};
#endif // MAINWINDOW_H
