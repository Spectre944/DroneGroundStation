#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    mapWidget = new MapWidget(this);
    m_draw.setwindow(mapWidget);

    mapWidget->setMapFile(QString("D://Tiles//tiles_ukr_9-16.db"));

    ui->horizontalLayout->addWidget(mapWidget);

    // --- Левое меню: прижато слева, на всю высоту с отступами 10 ---
    leftMenu    = new LeftMenu(this);
    leftOverlay = new OverlayContainer(mapWidget);
    leftOverlay->setContentWidget(leftMenu);
    leftOverlay->setAnchor(Qt::AlignLeft | Qt::AlignVCenter);
    leftOverlay->setStretch(Qt::Vertical);
    leftOverlay->setOuterMargins({10, 10, 10, 10});
    leftOverlay->setContentMargins(5, 5, 5, 5);
    leftOverlay->show();

    // --- Панель координат / полигонов: сверху по центру, размер по содержимому ---
    coordinatesPanel = new CoordinatesPanel(this);
    coordOverlay = new OverlayContainer(mapWidget);
    coordOverlay->setContentWidget(coordinatesPanel);
    coordOverlay->setAnchor(Qt::AlignHCenter | Qt::AlignTop);
    coordOverlay->setOuterMargins({90, 10, 90, 10});   // слева запас под левое меню
    coordOverlay->setContentMargins(10, 5, 10, 5);
    coordOverlay->show();

    // // --- Страницы: по центру, 60% x 70% карты, не больше 1000x800, модально ---
    // pagesStack  = new QStackedWidget();
    // pagesStack->addWidget(devicePage);     // 0
    // pagesStack->addWidget(databasePage);   // 1
    // pagesStack->addWidget(cbrnPage);       // 2
    // pagesStack->addWidget(reconPage);      // 3
    // pagesStack->addWidget(settingsPage);   // 4

    // pageOverlay = new OverlayContainer(mapWidget);
    // pageOverlay->setContentWidget(pagesStack);
    // pageOverlay->setAnchor(Qt::AlignCenter);
    // pageOverlay->setRelativeSize(0.6, 0.7);
    // pageOverlay->setMaximumSize(1000, 800);
    // pageOverlay->setOuterMargins({90, 20, 20, 20});
    // pageOverlay->setBorderRadius(16);
    // pageOverlay->setModal(true);
    // pageOverlay->hide();

    // connect(leftMenu, &LeftMenu::changePage, this, [=](int index) {
    //     static const QStringList titles = {"Прилади", "База даних", "Донесення", "Розвідка", "Налаштування"};
    //     pagesStack->setCurrentIndex(index);
    //     pageOverlay->setTitle(titles.value(index));
    //     pageOverlay->showOverlay();
    // });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{

}
