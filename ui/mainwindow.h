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
class DroneCommandsWindow;
class SdImport;
class PageBar;

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
    // Кнопка боковой панели (PageBar) или пункт меню «Вигляд» переключает страницу;
    // повторное нажатие на открытую — сворачивает стек.
    struct Page {
        QAction *action = nullptr;  // кнопка панели / пункт «Вигляд» (checkable)
        QWidget *widget = nullptr;
    };

    void setupMap();
    void setupPages();
    void setupMenu();
    void setupAlarms();
    void setupCore();       ///< создание ядра и подключение его сигналов

    /**
        @brief Добавить страницу: в стек, на боковую панель и в меню «Вигляд».
        @param svgPath — значок (bootstrap SVG из ресурсов)
        @param bottom  — кнопка прижата к низу панели (для «Налаштування»)
        @note  Горячая клавиша — Ctrl+<номер страницы>.
    */
    void addPage(const QString &title, QWidget *widget, const QString &svgPath, bool bottom = false);
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
    PageBar        *pageBar      = nullptr;     ///< кнопки страниц у левого края окна
    QAction        *actCollapse  = nullptr;     ///< «Вигляд → Згорнути панель» (Ctrl+0)

    /** @brief Меню «Політ» и «Дані»: команды, которые нужны с любой страницы. */
    QAction        *actFinishRecon = nullptr;   ///< «Закінчити розвідку» — ID08
    quint32         finishReconSeq = 0;         ///< номер отправленной ID08 (0 — не ждём)
    QAction        *actCommands = nullptr;      ///< «Команди дрону…» — окно ID05–ID14 (Р2-4)
    QAction        *actSdImport = nullptr;      ///< «Імпорт з SD-карти…» (Р2-6)
    QAction        *actFinishFlight = nullptr;  ///< «Завершити політ» — DroneCore::finishSession
    QAction        *actReturnLive = nullptr;    ///< «Повернутися до поточного» — видно при просмотре
    QAction        *actOpenFlight = nullptr;    ///< «Відкрити політ…» — список полётов в «База даних»
    void updateFlightActions();                 ///< доступность пунктов полёта
    DroneCommandsWindow  *commandsWindow = nullptr;
    SdImport       *sdImportWindow = nullptr;

    /**
        @brief Полёты (Р2-2): текущий полёт и просмотр прошлого.
        @note  Все данные полёта приходят одним DroneCore::sessionData. Он означает либо
               «открыт прошлый полёт» (просмотр), либо — если мы сами перечитали текущий
               при «Повернутися до поточного» — возврат журнала и тревог к живому полёту.
    */
    SessionInfo     currentSession;             ///< id = 0 — полёта нет
    bool            viewingSession = false;
    bool            restoringLive = false;      ///< ждём sessionData текущего полёта для возврата
    bool            openLatestAfterList = false;///< импорт SD: открыть последний полёт после listSessions
    void openFlight(qint64 sessionId);
    void returnToCurrentFlight();
    void applySessionView(const SessionData &d);
    void applyLiveRestore(const SessionData &d);
    QList<int>      lastSizes;              // ширины до сворачивания стека

    ReconPage       *reconPage       = nullptr;
    DatabasePage    *databasePage    = nullptr;
    InstrumentsPage *instrumentsPage = nullptr;
    SettingsPage    *settingsPage    = nullptr;

    IndicatorPanel *indicatorPanel = nullptr;
    AlarmDrawer    *alarmDrawer    = nullptr;

    // --- ядро ---
    DroneCore *core = nullptr;          ///< фасад ядра; удаляется вместе с окном

};
#endif // MAINWINDOW_H
