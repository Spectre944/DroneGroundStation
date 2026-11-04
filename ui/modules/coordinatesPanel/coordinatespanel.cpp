#include "coordinatespanel.h"
#include "ui_coordinatespanel.h"

CoordinatesPanel::CoordinatesPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CoordinatesPanel)
{
    ui->setupUi(this);
}

CoordinatesPanel::~CoordinatesPanel()
{
    delete ui;
}
