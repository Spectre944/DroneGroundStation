#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QMenuBar>
#include <QSplitter>
#include <QStackedWidget>

#include "map/mappicker.h"
#include "ui/pages/recon/reconpage.h"
#include "ui/pages/database/databasepage.h"
#include "ui/pages/instruments/instrumentspage.h"
#include "ui/pages/settings/settingspage.h"
#include "ui/modules/alarmCenter/alarmdrawer.h"
#include "ui/modules/notifier/notifier.h"
#include "classes/drone_core.h"

#include <QLabel>
#include <QStatusBar>

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
    for (const Page &page : std::as_const(pages))
        bar->addAction(page.action);

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
