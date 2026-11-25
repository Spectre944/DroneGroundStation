#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QSplitter>
#include <QStackedWidget>

#include "map/mappicker.h"
#include "ui/pages/recon/reconpage.h"
#include "ui/pages/database/databasepage.h"
#include "ui/pages/instruments/instrumentspage.h"
#include "ui/pages/settings/settingspage.h"
#include "ui/modules/alarmCenter/alarmdrawer.h"
#include "ui/modules/notifier/notifier.h"
#include "ui/modules/alarmCenter/alarmcenter.h"
#include "ui/modules/eventLog/eventlog.h"
#include "ui/modules/spectrumView/spectrumview.h"
#include "classes/drone_core.h"
#include "ui/modules/coreTexts/coretexts.h"
#include "ui/windows/dronecommands/dronecommandswindow.h"
#include "ui/windows/sdimport/sdimport.h"
#include <memory>
#include <limits>

namespace {
// Путь к карте, если в настройках ещё ничего не выбрано (как было захардкожено раньше)
const char DEFAULT_MAP_FILE[] = "D://Tiles//tiles_ukr_9-16.db";

const char KEY_SPLITTER[] = "ui/mainSplitter";      // ширины [страница, карта] между запусками

constexpr int PAGE_MIN_WIDTH     = 360;   // уже — поля страниц начинают сжиматься
constexpr int PAGE_DEFAULT_WIDTH = 480;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupMap();
    setupPages();
    setupMenu();
    setupAlarms();
    restoreSplitter();
    setupCore();
}


MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSplitter();
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------------

void MainWindow::setupMap()
{
    mapWidget = new MapWidget(this);
    m_draw.setwindow(mapWidget);

    QString mapFile = Settings::instance().getValue(SettingsPage::KEY_MAP_FILE).toString();
    if (mapFile.isEmpty())
        mapFile = DEFAULT_MAP_FILE;
    mapWidget->setMapFile(mapFile);

    // Страницы слева, карта справа, между ними — перетаскиваемый разделитель (как в РХБ)
    pageStack = new QStackedWidget(this);
    pageStack->setMinimumWidth(PAGE_MIN_WIDTH);

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(pageStack);
    mainSplitter->addWidget(mapWidget);
    mainSplitter->setStretchFactor(0, 0);       // при изменении окна растёт карта, страница сохраняет ширину
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setCollapsible(0, false);     // сворачиваем сами — через меню
    mainSplitter->setCollapsible(1, false);
    mainSplitter->setHandleWidth(6);

    ui->horizontalLayout->addWidget(mainSplitter);

    // "Выбрать на карте" для полей координат (TextEditCoordinates и т.п.)
    mapPicker = new MapPicker(mapWidget, this);
}

void MainWindow::setupPages()
{
    reconPage       = new ReconPage();
    databasePage    = new DatabasePage();
    instrumentsPage = new InstrumentsPage();
    settingsPage    = new SettingsPage();

    pageActions = new QActionGroup(this);
    pageActions->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

    addPage("Розвідка",     reconPage);
    addPage("База даних",   databasePage);
    addPage("Прилади",      instrumentsPage);
    addPage("Налаштування", settingsPage);

    // --- связи страниц с картой ---
    connect(settingsPage, &SettingsPage::mapFileSelected, mapWidget, &MapWidget::setMapFile);

    connect(databasePage, &DatabasePage::archivePointsLoaded,          mapWidget, &MapWidget::setArchivePoints);
    connect(databasePage, &DatabasePage::clearArchiveMapDataRequested, mapWidget, &MapWidget::clearArchiveThreadLayerData);
    connect(databasePage, &DatabasePage::archiveTableThreatClicked,    mapWidget, &MapWidget::selectArchivePoint);
    connect(databasePage, &DatabasePage::querySearchAreaFormed,        mapWidget, &MapWidget::activeSearchAreaRecived);
    connect(mapWidget, &MapWidget::archiveThreatClicked,  databasePage, &DatabasePage::onArchiveThreatClicked);
    connect(mapWidget, &MapWidget::centerPositionChanged, databasePage, &DatabasePage::onCenterMarkerChanged);
}

void MainWindow::setupMenu()
{
    QMenuBar *bar = menuBar();
    for (const Page &page : std::as_const(pages))
        bar->addAction(page.action);

    // «Дії» — команды логгеру; доступность — по DroneCore::allowedCommandsChanged (setupCore)
    QMenu *actions = bar->addMenu("Дії");
    actFinishRecon = actions->addAction("Закінчити розвідку");
    actFinishRecon->setToolTip("ID08: логер закриває файл області і переходить у «Повернення»");
    actFinishRecon->setEnabled(false);

    // Окно команд ID05–ID14 (Р2-4) — пока отдельное окно, место на странице выберем потом
    commandsWindow = new DroneCommandsWindow(this);
    commandsWindow->hide();
    actCommands = actions->addAction("Команди дрону…");
    connect(actCommands, &QAction::triggered, this, [this] {
        commandsWindow->show();
        commandsWindow->raise();
        commandsWindow->activateWindow();
    });

    // Импорт SD (Р2-6)
    actions->addSeparator();
    sdImportWindow = new SdImport(this);
    sdImportWindow->hide();
    actSdImport = actions->addAction("Імпорт з SD-карти…");
    connect(actSdImport, &QAction::triggered, this, [this] {
        sdImportWindow->show();
        sdImportWindow->raise();
        sdImportWindow->activateWindow();
    });

    indicatorPanel = new IndicatorPanel(bar);
    bar->setCornerWidget(indicatorPanel, Qt::TopRightCorner);
}

void MainWindow::setupAlarms()
{
    // Шторка тревог у правого края карты
    alarmDrawer = new AlarmDrawer(mapWidget);

    // TODO (шаг 3, заглушка ядра): подключить к DroneCore
    //   connect(core, &DroneCore::alarm,             alarmDrawer->center(), &AlarmCenter::onAlarm);
    //   connect(core, &DroneCore::alarmAcknowledged, alarmDrawer->center(), &AlarmCenter::onAlarmAcknowledged);
    //   connect(alarmDrawer->center(), &AlarmCenter::acknowledgeRequested, core, &DroneCore::acknowledgeAlarm);
    //   connect(alarmDrawer->center(), &AlarmCenter::showOnMapRequested, mapWidget, ...центрировать карту...);
}

// ---------------------------------------------------------------------------
//  Ядро (DroneCore)
// ---------------------------------------------------------------------------

/**
    @brief Создание ядра и подключение его сигналов к панелям окна.
    @note  Сейчас за DroneCore стоит заглушка ядра (порт «STUB», тестовые данные).
           Каждый блок — одна панель: что она получает от ядра и что отправляет в него.
*/
void MainWindow::setupCore()
{
    core = new DroneCore(this);     // родитель — окно: удаляется вместе с ним, деструктор остановит поток ядра

    // ---- панель индикаторов (верхний правый угол): связь, режим, спектр, радиомолчание, батарея, полёт ----
    connect(core, &DroneCore::linkStatusChanged,   indicatorPanel, &IndicatorPanel::onLinkStatus);
    connect(core, &DroneCore::loggerStatusChanged, indicatorPanel, &IndicatorPanel::onLoggerStatus);
    connect(core, &DroneCore::sessionStarted,      indicatorPanel, &IndicatorPanel::onSessionStarted);
    connect(core, &DroneCore::sessionFinished,     indicatorPanel, &IndicatorPanel::onSessionFinished);
    // sessionData (просмотр прошлого полёта) — см. блок «полёты (Р2-2)» ниже

    // ---- страница «Налаштування»: радиомодем, приборы логгера, окно предполётной проверки ----
    connect(core, &DroneCore::linkStatusChanged,      settingsPage, &SettingsPage::onLinkStatus);
    connect(core, &DroneCore::loggerStatusChanged,    settingsPage, &SettingsPage::onLoggerStatus);
    connect(core, &DroneCore::routePoint,             settingsPage, &SettingsPage::onRoutePoint);
    connect(core, &DroneCore::allowedCommandsChanged, settingsPage, &SettingsPage::onAllowedCommands);
    connect(settingsPage, &SettingsPage::commandRequested, core, [this](const CommandRequest &r) {
        core->sendCommand(r);                               // ID01 / ID02 / ID03 из окна проверки
    });
    // приборы: пришёл пакет прибора — «Останнє оновлення» с нуля
    connect(core, &DroneCore::measurement,     settingsPage, &SettingsPage::onMeasurement);
    connect(core, &DroneCore::spectrumUpdated, settingsPage, &SettingsPage::onSpectrum);
    // радиомодем (Р2-1): список портов, «Підключити» / «Відключити» в окне прибора
    settingsPage->setPortProvider([this] { return core->availablePorts(); });
    connect(settingsPage, &SettingsPage::portOpenRequested,  core, &DroneCore::openPort);
    connect(settingsPage, &SettingsPage::portCloseRequested, core, &DroneCore::closePort);
    // настройки ядра (Р2-5, часть): пороги ПАЕД, сетка — после «Зберегти»
    connect(settingsPage, &SettingsPage::coreSettingsChanged, core, [this](const CoreSettings &s) {
        core->applySettings(s);
        mapWidget->setDoseThresholds(s.warnLevel, s.alarmLevel);
    });

    // ---- страница «Розвідка» (Р2-3): область, проверка ядром, ID04 ----
    connect(mapWidget, &MapWidget::reconAssigned, reconPage, &ReconPage::onAreaAssigned);
    connect(mapWidget, &MapWidget::reconCleared,  reconPage, &ReconPage::onAreaCleared);
    connect(mapWidget, &MapWidget::reconAssigned, core, [this](const QVector<smappoint> &pts, bool isRoute) {
        QVector<GeoPoint> v;
        v.reserve(pts.size());
        for (const smappoint &p : pts) {
            GeoPoint g;
            g.lat = p.latt;
            g.lon = p.longt;
            v.append(g);
        }
        core->checkArea(v, isRoute);                        // ответ — areaChecked
    });
    connect(reconPage, &ReconPage::areaEdited, mapWidget, &MapWidget::setReconArea);
    connect(core, &DroneCore::areaChecked, reconPage, &ReconPage::onAreaChecked);
    connect(core, &DroneCore::areaChecked, mapWidget, [this](const AreaCheck &c) {
        mapWidget->setReconCheck(c.ok, c.vertex, CoreTexts::areaError(c));
    });
    connect(core, &DroneCore::allowedCommandsChanged, reconPage, &ReconPage::onAllowedCommands);
    connect(core, &DroneCore::linkStatusChanged,      reconPage, &ReconPage::onLinkStatus);
    connect(core, &DroneCore::loggerStatusChanged,    reconPage, &ReconPage::onLoggerStatus);
    connect(core, &DroneCore::commandFinished,        reconPage, &ReconPage::onCommandFinished);
    connect(core, &DroneCore::sessionStarted,  reconPage, &ReconPage::onSessionStarted);
    connect(core, &DroneCore::sessionFinished, reconPage, &ReconPage::onSessionFinished);
    connect(reconPage, &ReconPage::startFlightRequested,  core, &DroneCore::startSession);
    connect(reconPage, &ReconPage::finishFlightRequested, core, &DroneCore::finishSession);
    connect(reconPage, &ReconPage::sendRequested, core, [this](const CommandRequest &r) {
        reconPage->onCommandSent(core->sendCommand(r));
    });
    connect(settingsPage, &SettingsPage::reconDefaultsChanged, reconPage, &ReconPage::setDefaults);
    // «Підготувати для розвідки» на карте — сразу открыть страницу, где область отправляется
    connect(mapWidget, &MapWidget::reconPrepareRequested, this, [this] {
        showPage(pageStack->indexOf(reconPage));
    });
    // Логер принял ID04 — на карте «Активна область» (сплошная), черновик больше не рисуется
    connect(core, &DroneCore::commandFinished, mapWidget, [this](const CommandOutcome &o) {
        if (o.request.id == 4 && o.result == CommandResult::Done)
            mapWidget->setActiveReconArea(o.request, o.finishTime);
    });
    // «Дії → Закінчити розвідку» (ID08): после «Виконано» активная область снимается с карты
    auto finishState = std::make_shared<QPair<bool, bool>>(false, false);   // ID08 разрешена, ждём ответ
    auto updateFinish = [this, finishState] {
        actFinishRecon->setEnabled(finishState->first && !finishState->second && finishReconSeq == 0);
    };
    connect(core, &DroneCore::allowedCommandsChanged, this, [finishState, updateFinish](const QList<quint8> &ids) {
        finishState->first = ids.contains(quint8(8));
        updateFinish();
    });
    connect(core, &DroneCore::linkStatusChanged, this, [finishState, updateFinish](const LinkStatus &st) {
        finishState->second = st.commandPending;
        updateFinish();
    });
    connect(actFinishRecon, &QAction::triggered, this, [this, updateFinish] {
        const auto answer = QMessageBox::question(this, "Закінчити розвідку",
            "Логер закриє файл поточної області, зупинить вимірювання і перейде в режим «Повернення».\n\n"
            "Закінчити розвідку?");
        if (answer != QMessageBox::Yes)
            return;
        CommandRequest r;
        r.id = 8;
        // Номер ещё не известен: если итог придёт прямо внутри sendCommand (заглушка в
        // потоке интерфейса), обработчик узнает «свою» команду по этому значению
        finishReconSeq = std::numeric_limits<quint32>::max();
        const quint32 seq = core->sendCommand(r);
        if (finishReconSeq == std::numeric_limits<quint32>::max())
            finishReconSeq = seq;           // итог ещё не пришёл — ждём по номеру
        updateFinish();
    });
    connect(core, &DroneCore::commandFinished, this, [this, updateFinish](const CommandOutcome &o) {
        if (o.request.id != 8)
            return;
        if (o.result == CommandResult::Done) {
            mapWidget->clearActiveReconArea();
            NotificationManager::showSuccess("Логер перейшов у режим «Повернення»", "Розвідку закінчено");
        }
        const bool ours = finishReconSeq != 0 &&
                          (o.request.seq == finishReconSeq || finishReconSeq == std::numeric_limits<quint32>::max());
        if (!ours)
            return updateFinish();
        if (o.result != CommandResult::Done)
            NotificationManager::showError(CoreTexts::outcome(o), "Розвідку не закінчено");
        finishReconSeq = 0;
        updateFinish();
    });

    // Порт открыт заново — ядро забывает принятую область (CoreEngine::openPort), карта тоже
    connect(core, &DroneCore::linkStatusChanged, mapWidget,
            [this, prev = std::make_shared<LinkState>(LinkState::Closed)](const LinkStatus &st) {
        if (*prev == LinkState::Closed && st.state != LinkState::Closed)
            mapWidget->clearActiveReconArea();
        *prev = st.state;
    });

    // ---- карта: дрон и путь, точки измерений, тепловая карта ----
    connect(core, &DroneCore::routePoint, mapWidget, [this](const RoutePoint &p) {
        mapWidget->updateDronePosition(p.lat, p.lon);
    });
    connect(core, &DroneCore::measurement, mapWidget, &MapWidget::onMeasurement);
    connect(core, &DroneCore::gridUpdated, mapWidget, &MapWidget::onGridUpdated);

    // ---- шторка тревог: тревоги, «Бачу», показать на карте (тосты показывает сама шторка) ----
    AlarmCenter *alarms = alarmDrawer->center();
    connect(core, &DroneCore::alarm,             alarms, &AlarmCenter::onAlarm);
    connect(core, &DroneCore::alarmAcknowledged, alarms, &AlarmCenter::onAlarmAcknowledged);
    connect(alarms, &AlarmCenter::acknowledgeRequested, core, &DroneCore::acknowledgeAlarm);
    connect(alarms, &AlarmCenter::showOnMapRequested, mapWidget, [this](double lat, double lon) {
        smappoint pt;
        pt.latt  = lat;
        pt.longt = lon;
        mapWidget->setCenter(pt);
    });

    // ---- страница «Прилади»: журнал событий и спектр ----
    // Виджеты создаёт InstrumentsPage; доступа к ним снаружи пока нет — находим по типу.
    if (auto *log = instrumentsPage->findChild<EventLog *>()) {
        connect(core, &DroneCore::event,           log, &EventLog::onEvent);
        connect(core, &DroneCore::commandFinished, log, &EventLog::onCommandFinished);
        connect(core, &DroneCore::alarm,           log, &EventLog::onAlarm);
        connect(core, &DroneCore::coreError,       log, &EventLog::onCoreError);
    }
    if (auto *spectrum = instrumentsPage->findChild<SpectrumView *>()) {
        connect(core, &DroneCore::spectrumUpdated,  spectrum, &SpectrumView::setSpectrum);
        connect(core, &DroneCore::spectrumFinished, spectrum, &SpectrumView::setSpectrum);
    }

    // ---- окно команд ID05–ID14 (Р2-4) ----
    connect(core, &DroneCore::allowedCommandsChanged, commandsWindow, &DroneCommandsWindow::onAllowedCommands);
    connect(core, &DroneCore::linkStatusChanged,      commandsWindow, &DroneCommandsWindow::onLinkStatus);
    connect(core, &DroneCore::loggerStatusChanged,    commandsWindow, &DroneCommandsWindow::onLoggerStatus);
    connect(core, &DroneCore::commandFinished,        commandsWindow, &DroneCommandsWindow::onCommandFinished);
    connect(commandsWindow, &DroneCommandsWindow::sendRequested, core, [this](const CommandRequest &r) {
        commandsWindow->onCommandSent(core->sendCommand(r));
    });

    // ---- импорт SD (Р2-6) ----
    connect(sdImportWindow, &SdImport::importRequested, core, &DroneCore::importSd);
    connect(core, &DroneCore::sdProgress, sdImportWindow, &SdImport::onSdProgress);
    connect(core, &DroneCore::sdImported, sdImportWindow, &SdImport::onSdImported);
    connect(sdImportWindow, &SdImport::openFlightRequested, this, [this] {
        // Данные SD добавлены в текущий полёт, а если его нет — в последний
        if (currentSession.id > 0) {
            openFlight(currentSession.id);
        } else {
            openLatestAfterList = true;
            core->listSessions();
        }
    });

    // ---- полёты (Р2-2): список в «База даних», просмотр прошлого на карте ----
    connect(databasePage, &DatabasePage::flightsRequested, core, &DroneCore::listSessions);
    connect(databasePage, &DatabasePage::pointsRequested,  core, &DroneCore::queryArchive);
    connect(core, &DroneCore::sessionsListed, databasePage, &DatabasePage::onSessionsListed);
    connect(core, &DroneCore::archiveResult,  databasePage, &DatabasePage::onArchiveResult);
    connect(databasePage, &DatabasePage::openFlightRequested, this, &MainWindow::openFlight);
    connect(core, &DroneCore::sessionsListed, this, [this](const QVector<SessionInfo> &list) {
        if (!openLatestAfterList)
            return;
        openLatestAfterList = false;
        if (!list.isEmpty())
            openFlight(list.first().id);                    // ядро даёт новые сверху
        else
            NotificationManager::showWarning("У базі ще немає польотів");
    });
    connect(core, &DroneCore::sessionStarted, this, [this](const SessionInfo &info) {
        currentSession = info;
        databasePage->setCurrentSession(info.id);
    });
    connect(core, &DroneCore::sessionFinished, this, [this](const SessionInfo &info) {
        if (info.id == currentSession.id) {
            currentSession = SessionInfo();
            databasePage->setCurrentSession(-1);
        }
    });
    connect(core, &DroneCore::sessionData, this, [this](const SessionData &d) {
        if (restoringLive && d.info.id == currentSession.id) {
            restoringLive = false;
            applyLiveRestore(d);
        } else {
            applySessionView(d);
        }
    });
    connect(mapWidget, &MapWidget::sessionViewCloseRequested, this, &MainWindow::returnToCurrentFlight);

    // ---- ошибки ядра (порт, база) — уведомлением ----
    connect(core, &DroneCore::coreError, this, [](const QString &source, const QString &text) {
        NotificationManager::showError(source + ": " + text);
    });

    // ---- запуск ----
    const CoreSettings settings = settingsPage->coreSettings();     // из файла настроек (Р2-5)
    mapWidget->setDoseThresholds(settings.warnLevel, settings.alarmLevel);
    core->applySettings(settings);

    // Порт — тот, что выбрали в прошлый раз (Р2-1). Его нет в системе — не открываем, сообщаем
    const QString port = settingsPage->savedPort();
    if (port == QLatin1String("STUB") || core->availablePorts().contains(port))
        core->openPort(port);
    else if (!port.isEmpty())
        NotificationManager::showWarning(QString("Порт %1 не знайдено. Виберіть порт: Налаштування → Радіомодем.").arg(port),
                                         "Радіомодем не підключено");
}

// ---------------------------------------------------------------------------
//  Страницы
// ---------------------------------------------------------------------------

void MainWindow::addPage(const QString &title, QWidget *widget)
{
    const int index = pageStack->addWidget(widget);

    Page page;
    page.widget = widget;
    page.action = new QAction(title, this);
    page.action->setCheckable(true);
    pageActions->addAction(page.action);
    connect(page.action, &QAction::triggered, this, [this, index] { togglePage(index); });

    pages.append(page);
}

void MainWindow::togglePage(int index)
{
    // Повторное нажатие на открытую страницу сворачивает стек — карта на всю ширину
    if (pageStack->isVisible() && pageStack->currentIndex() == index)
        hidePages();
    else
        showPage(index);
}

void MainWindow::showPage(int index)
{
    pageStack->setCurrentIndex(index);
    pages.at(index).action->setChecked(true);

    if (!pageStack->isVisible()) {
        pageStack->show();
        if (lastSizes.size() == 2 && lastSizes.at(0) > 0)
            mainSplitter->setSizes(lastSizes);
    }
}

void MainWindow::hidePages()
{
    if (pageStack->isVisible())
        lastSizes = mainSplitter->sizes();
    pageStack->hide();
    for (const Page &page : std::as_const(pages))
        page.action->setChecked(false);
}

// ---------------------------------------------------------------------------
//  Ширина страницы между запусками
// ---------------------------------------------------------------------------

void MainWindow::restoreSplitter()
{
    // Хранится как «ширина страницы,ширина карты», например «480,1200»
    const QStringList parts = Settings::instance().getValue(KEY_SPLITTER).toString().split(',');
    int page = PAGE_DEFAULT_WIDTH, map = 1000;
    if (parts.size() == 2) {
        page = qMax(PAGE_MIN_WIDTH, parts.at(0).toInt());
        map  = qMax(1, parts.at(1).toInt());
    }
    lastSizes = {page, map};
    mainSplitter->setSizes(lastSizes);
    hidePages();    // при запуске — только карта, как было с боковыми панелями
}

void MainWindow::saveSplitter()
{
    // Ширина открытой страницы; если стек свёрнут — последней открытой
    if (pageStack->isVisible())
        lastSizes = mainSplitter->sizes();
    if (lastSizes.size() != 2)
        return;

    Settings::instance().setValue(KEY_SPLITTER, QStringLiteral("%1,%2").arg(lastSizes.at(0)).arg(lastSizes.at(1)));
    Settings::instance().saveSettings();
}

// ---------------------------------------------------------------------------
//  Полёты (Р2-2): просмотр прошлого полёта и возврат к текущему
// ---------------------------------------------------------------------------

void MainWindow::openFlight(qint64 sessionId)
{
    if (sessionId <= 0)
        return;
    restoringLive = false;
    core->openSession(sessionId);       // ответ — sessionData → applySessionView
}

/**
    @brief Открыт прошлый полёт: карта, журнал, тревоги, индикатор — его данные;
           отправка команд недоступна (readOnly). Живые данные при этом пишутся в базу.
*/
void MainWindow::applySessionView(const SessionData &d)
{
    viewingSession = true;
    mapWidget->clearArchiveThreadLayerData();        // маркеры поиска по архиву — не мешают полёту
    mapWidget->showSessionView(d);
    alarmDrawer->center()->setAlarms(d.alarms);
    if (auto *log = instrumentsPage->findChild<EventLog *>())
        log->loadSession(d);
    indicatorPanel->onSessionOpened(d.info);
    reconPage->setReadOnly(true);
    commandsWindow->setReadOnly(true);
    NotificationManager::showInfo(QString("Перегляд польоту «%1»").arg(d.info.name));
}

/**
    @brief «Повернутися до поточного»: карта — живые точки и сетка (MapWidget их копил);
           журнал и тревоги — из базы текущего полёта (перечитываем его), если он есть.
*/
void MainWindow::returnToCurrentFlight()
{
    if (!viewingSession)
        return;
    viewingSession = false;
    mapWidget->closeSessionView();
    reconPage->setReadOnly(false);
    commandsWindow->setReadOnly(false);

    if (currentSession.id > 0) {
        restoringLive = true;
        core->openSession(currentSession.id);     // ответ — sessionData → applyLiveRestore
        indicatorPanel->onSessionStarted(currentSession);
    } else {
        alarmDrawer->center()->setAlarms({});
        if (auto *log = instrumentsPage->findChild<EventLog *>())
            log->loadSession(SessionData());
        SessionInfo none;
        indicatorPanel->onSessionFinished(none);
    }
}

void MainWindow::applyLiveRestore(const SessionData &d)
{
    alarmDrawer->center()->setAlarms(d.alarms);
    if (auto *log = instrumentsPage->findChild<EventLog *>())
        log->loadSession(d);
    indicatorPanel->onSessionStarted(currentSession);
}
