#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QEvent>
#include <QMenuBar>

#include "map/mappicker.h"
#include "ui/pages/recon/reconpage.h"
#include "ui/pages/database/databasepage.h"
#include "ui/pages/instruments/instrumentspage.h"
#include "ui/pages/settings/settingspage.h"
#include "ui/modules/notifier/notifier.h"
#include "classes/drone_core.h"

#include <QLabel>
#include <QStatusBar>

namespace {
// Путь к карте, если в настройках ещё ничего не выбрано (как было захардкожено раньше)
const char DEFAULT_MAP_FILE[] = "D://Tiles//tiles_ukr_9-16.db";

constexpr int    PANEL_MARGIN = 10;          // отступ панелей от краёв карты
constexpr qreal  PANEL_WIDTH  = 1.0 / 3.0;   // доля ширины карты под боковую страницу
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupMap();
    setupPages();
    setupMenu();
    setupCore();
}


MainWindow::~MainWindow()
{
    qDeleteAll(panels);
    panels.clear();
    delete ui;
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

    ui->horizontalLayout->addWidget(mapWidget);

    // "Выбрать на карте" для полей координат (TextEditCoordinates и т.п.)
    mapPicker = new MapPicker(mapWidget, this);
}

void MainWindow::setupPages()
{
    reconPage    = new ReconPage();
    databasePage = new DatabasePage();
    instrumentsPage = new InstrumentsPage();
    settingsPage = new SettingsPage();

    reconPanel    = addSidePanel("Розвідка",     reconPage);
    databasePanel = addSidePanel("База даних",   databasePage);
    instrumentsPanel = addSidePanel("Прилади",   instrumentsPage);
    settingsPanel = addSidePanel("Налаштування", settingsPage);

    // --- связи страниц с картой ---
    connect(settingsPage, &SettingsPage::mapFileSelected, mapWidget, &MapWidget::setMapFile);

    connect(databasePage, &DatabasePage::archiveDataLoaded,            mapWidget, &MapWidget::setupdArchiveThreads);
    connect(databasePage, &DatabasePage::clearArchiveMapDataRequested, mapWidget, &MapWidget::clearArchiveThreadLayerData);
    connect(databasePage, &DatabasePage::archiveTableThreatClicked,    mapWidget, &MapWidget::markSelectedArchiveDanger);
    connect(databasePage, &DatabasePage::querySearchAreaFormed,        mapWidget, &MapWidget::activeSearchAreaRecived);
    connect(mapWidget, &MapWidget::archiveThreatClicked,  databasePage, &DatabasePage::onArchiveThreatClicked);
    connect(mapWidget, &MapWidget::centerPositionChanged, databasePage, &DatabasePage::onCenterMarkerChanged);
}

void MainWindow::setupMenu()
{
    QMenuBar *bar = menuBar();
    for (SidePanel *panel : std::as_const(panels))
        bar->addAction(panel->action);
}

// ---------------------------------------------------------------------------
//  Ядро (DroneCore)
// ---------------------------------------------------------------------------

/**
    @brief Создание ядра и подключение его сигналов к карте, строке состояния
           и уведомлениям.
    @note  Сейчас за DroneCore стоит заглушка ядра (тестовые данные).
           Тексты — временные, до локализации (Ф15).
*/
void MainWindow::setupCore()
{
    core = new DroneCore(this);     // родитель — окно: удаляется вместе с ним, деструктор остановит поток ядра

    // ---- строка состояния: режим логгера и связь ----
    modeLabel = new QLabel(this);
    linkLabel = new QLabel(this);
    statusBar()->addPermanentWidget(modeLabel);
    statusBar()->addPermanentWidget(linkLabel);

    connect(core, &DroneCore::linkStatusChanged, this, [this](const LinkStatus &s) {
        QString text;
        switch (s.state) {
        case LinkState::Closed:  text = "Порт закрито";             break;
        case LinkState::Waiting: text = "Очікування даних";         break;
        case LinkState::Ok:      text = "Зв'язок є";                break;
        case LinkState::Silent:  text = "Радіомовчання";            break;
        case LinkState::Lost:    text = "Зв'язок втрачено";         break;
        }
        if (s.secondsSinceLastPacket >= 0)
            text += QString(" | %1 с тому").arg(s.secondsSinceLastPacket);
        linkLabel->setText(QString("%1: %2").arg(s.portName, text));
    });

    connect(core, &DroneCore::loggerStatusChanged, this, [this](const LoggerStatus &s) {
        QString text;
        switch (s.mode) {
        case LoggerMode::Unknown: text = "Невідомо";    break;
        case LoggerMode::Test:    text = "Перевірка";   break;
        case LoggerMode::Expect:  text = "Очікування";  break;
        case LoggerMode::Work:    text = "Розвідка";    break;
        case LoggerMode::Return:  text = "Повернення";  break;
        case LoggerMode::Error:   text = "Аварія";      break;
        }
        if (s.spectrumStage == SpectrumStage::Preparing)  text += " | підготовка спектра";
        if (s.spectrumStage == SpectrumStage::Collecting) text += " | набір спектра";
        modeLabel->setText("Режим: " + text);
    });

    // ---- карта: дрон и путь ----
    connect(core, &DroneCore::routePoint, this, [this](const RoutePoint &p) {
        mapWidget->updateDronePosition(p.lat, p.lon);
    });

    // ---- карта: точки измерений ----
    // Значок: 0 — обычная точка, 1 — радиация (выше порога предупреждения).
    // Порог пока по умолчанию из CoreSettings — до подключения настроек.
    const float warnLevel = CoreSettings().warnLevel;
    connect(core, &DroneCore::measurement, this, [this, warnLevel](const Measurement &m) {
        smarker mark;
        mark.pt.latt  = m.lat;
        mark.pt.longt = m.lon;
        mark.type     = (m.shownPaed() >= warnLevel) ? 1 : 0;
        mark.is_sel   = false;
        mapWidget->appendActiveThread(mark);
    });

    // ---- уведомления: тревоги ----
    connect(core, &DroneCore::alarm, this, [](const Alarm &a) {
        QString text;
        switch (a.type) {
        case AlarmType::DoseRate:     text = QString("ПАЕД %1 мкЗв/год").arg(a.value, 0, 'f', 2); break;
        case AlarmType::LoggerError:  text = "Аварія дрона";            break;
        case AlarmType::SdError:      text = "Помилка SD-карти";         break;
        case AlarmType::LinkLost:     text = "Зв'язок втрачено";         break;
        case AlarmType::ModeMismatch: text = "Режим логгера не збігається"; break;
        }
        if (a.level == AlarmLevel::Alarm)
            NotificationManager::showError("ТРИВОГА: " + text);
        else
            NotificationManager::showWarning("Попередження: " + text);
    });

    // ---- события: в строку состояния на 5 с (журнала пока нет) ----
    connect(core, &DroneCore::event, this, [this](const CoreEvent &e) {
        statusBar()->showMessage(QString("Подія 0x%1").arg(int(e.code), 2, 16, QChar('0')), 5000);
    });

    // ---- ошибки ядра ----
    connect(core, &DroneCore::coreError, this, [](const QString &source, const QString &text) {
        NotificationManager::showError(source + ": " + text);
    });

    // ---- запуск ----
    core->applySettings(CoreSettings());        // настройки по умолчанию — до подключения страницы настроек
    core->openPort("STUB");                      // временно: пока нет выбора COM-порта; заглушка порт не открывает
}


// ---------------------------------------------------------------------------
//  Боковые страницы
// ---------------------------------------------------------------------------

MainWindow::SidePanel *MainWindow::addSidePanel(const QString &title, QWidget *page)
{
    auto *panel = new SidePanel;
    panel->page = page;

    panel->overlay = new OverlayContainer(mapWidget);
    panel->overlay->setContentWidget(page);
    panel->overlay->setTitle(title);                                  // шапка с кнопкой ✕
    panel->overlay->setAnchor(Qt::AlignLeft | Qt::AlignTop);
    panel->overlay->setRelativeSize(PANEL_WIDTH, 0);                  // 1/3 ширины ...
    panel->overlay->setStretch(Qt::Vertical);                         // ... на всю высоту
    panel->overlay->setOuterMargins({PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN});
    panel->overlay->setContentMargins(10, 0, 10, 10);
    panel->overlay->hide();
    panel->overlay->installEventFilter(this);   // следим за показом/размером -> updateMapInsets()

    panel->action = new QAction(title, this);
    panel->action->setCheckable(true);

    connect(panel->action, &QAction::triggered, this, [this, panel] { togglePanel(panel); });
    // ✕ в шапке закрывает страницу - снимаем галочку с пункта меню
    connect(panel->overlay, &OverlayContainer::closed, this, [panel] { panel->action->setChecked(false); });

    panels.append(panel);
    return panel;
}

void MainWindow::togglePanel(SidePanel *panel)
{
    if (panel->overlay->isVisible())
        panel->overlay->hideOverlay();
    else
        showPanel(panel);
}

// Слева одновременно открыта только одна страница
void MainWindow::showPanel(SidePanel *panel)
{
    for (SidePanel *other : std::as_const(panels))
        if (other != panel)
            other->overlay->hideOverlay();

    panel->overlay->showOverlay();
    panel->action->setChecked(true);
}

// Открытая слева страница не должна закрывать кнопки карты
void MainWindow::updateMapInsets()
{
    int inset = 0;
    for (SidePanel *panel : std::as_const(panels))
        if (panel->overlay->isVisible())
            inset = qMax(inset, panel->overlay->geometry().right() + 1);
    mapWidget->setLeftInset(inset);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::Resize:
    case QEvent::Move:
        for (SidePanel *panel : std::as_const(panels)) {
            if (watched == panel->overlay) {
                updateMapInsets();
                break;
            }
        }
        break;
    default:
        break;
    }
    return QMainWindow::eventFilter(watched, event);
}
