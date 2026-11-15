#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QEvent>
#include <QMenuBar>

#include "map/mappicker.h"
#include "ui/pages/recon/reconpage.h"
#include "ui/pages/database/databasepage.h"
#include "ui/pages/settings/settingspage.h"

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
    settingsPage = new SettingsPage();

    reconPanel    = addSidePanel("Розвідка",     reconPage);
    databasePanel = addSidePanel("База даних",   databasePage);
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
