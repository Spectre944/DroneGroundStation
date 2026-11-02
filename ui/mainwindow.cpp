#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Map widget to the right screen
    MapWidget *mapWidget = new MapWidget(this);
    m_draw.setwindow(mapWidget);

    mapWidget->setMapFile(QString("D://Tiles//tiles_ukr_9-16.db"));
    // Provider betwen TextEdit coordinates and map
    // CoordinatesManager::instance()->setMapReference(mapWidget);

    ui->horizontalLayout->addWidget(mapWidget);
}

MainWindow::~MainWindow()
{
    delete ui;
}
