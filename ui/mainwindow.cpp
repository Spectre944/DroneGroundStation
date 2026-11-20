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
