#include "devicepage.h"
#include "ui_devicepage.h"

DevicePage::DevicePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DevicePage)
{
    ui->setupUi(this);

    сheckpointModel = new SimpleTableModel(this);
    сheckpointModel->setHeaderDataFromList(QStringList() << "Параметри" << "AirMarWX200" << "LCD4" << "БДБГ-Т/МКС-УМ");
    ui->tableView->setModel(сheckpointModel);
    ui->tableView->setFont(QFont("Consolas"));
    ui->tableView->setTabKeyNavigation(false);
    ui->tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setWordWrap(false);

    ui->tableControl->setTableName("Поточні дані");
    ui->tableControl->setIcon(QPixmap(":/icons/png/old/file-storage.png"));
    ui->tableControl->setNavigatorBtnVisible(false);
    ui->tableControl->setButtonEnabled(TableControl::WidgetType::Clear, false);

    connect(ui->tableControl, &TableControl::clearRequested, сheckpointModel, &SimpleTableModel::clearData);
    connect(ui->tableControl, &TableControl::adjustRequested, this, [this]() {
        ui->tableView->resizeColumnsToContents();
        ui->tableView->resizeRowsToContents();
    });

    connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
        if (current.isValid()) {
            int rowIndex = current.row();

            if (!mapThreadRequest) {
                emit tableThreadClicked(rowIndex);
            }

            mapThreadRequest = false;
        }
    });


    // ui->gridLayoutDevices->addWidget(, 1, 0);
    // ui->gridLayoutDevices->addWidget(, 1, 1);


    //--------------------------------------------------------------------//
    //                        Тестовая секция                             //
    //--------------------------------------------------------------------//

    // testData();

    //--------------------------------------------------------------------//
    //                        Тестовая секция                             //
    //--------------------------------------------------------------------//
}

DevicePage::~DevicePage()
{
    delete ui;
}


void DevicePage::onBdbgDataGenerated(const bdbgData &data)
{
    // if(widgetBdbg)
    //     widgetBdbg->updateData(data);
}


void DevicePage::onBdbgDerThresholdUpdated(QVariant value)
{
    // if(widgetBdbg)
    //     widgetBdbg->updateDerThreshold(value);
}

void DevicePage::onReconStarted()
{

}

void DevicePage::onReconEnded(startReconParams)
{

    сheckpointModel->clearData();
}

void DevicePage::onReconToggled(bool status)
{

}

void DevicePage::onReconModeChanged(PROGRAMM_SETTINGS::ReconMode mode)
{

}

void DevicePage::onLastSavePointRecived(CoordPairD savePoint)
{

}

void DevicePage::onReconDurationExceeded()
{

}

void DevicePage::onDatabaseDataAddedDzura(gpsData gps, airmarData airmar, bdbgData bdbg, mksuData mksu, lcdData lcd)
{
    QDateTime currentTime = QDateTime::currentDateTime();

    QString pm = QString("Координати\n"
                         "ш: %1     \n"
                         "д: %2     \n"
                         "\n"
                         "Час:  %3  \n"
                         "Дата: %4")
                        .arg(gps.coord.first)
                        .arg(gps.coord.second)
                        .arg(currentTime.time().toString("hh:mm:ss"))
                        .arg(currentTime.date().toString("yyyy-MM-dd"));

    QStringList linesAm;
    if(airmar.failureFlag == false){
        linesAm = {
            QString("Швидк. вітру:  — "),
            QString("Температура: . — "),
            QString("Вологість: ... — "),
            QString("Тиск: ........ — "),
            QString("Точка роси: .. — "),
            QString("Напрям (скор): — ")
        };
    }
    else {
        linesAm = {
            QString("Швидк. вітру:  %1 м/с").arg(airmar.validWindDirect ? QString::number(airmar.windSpeed) : "—"),
            QString("Температура: . %1 °C").arg(airmar.temperature),
            QString("Вологість: ... %1 %").arg(airmar.humidity),
            QString("Тиск: ........ %1 Па").arg(airmar.pressure),
            QString("Точка роси: .. %1 °C").arg(airmar.dewPoint),
            QString("Напрям (скор): %1 °").arg(airmar.validWindDirect ? QString::number(airmar.windDirectCorrect) : "—")
        };
    }

    QString am = linesAm.join("\n");


// QString::number(d, 'f', 2)
    QString bdmk = QString("ПАЕД БДБГ-Т     \n"
                           "  Лівий  БД: %1 \n"
                           "  Правий БД: %2 \n"
                           "ПАЕД МКС-УМ: %3 \n"
                           "Доза: ...... %4 \n"
                           "Час накоп.:  %5 \n"
                           "             гг:хх:сс")
                       .arg(bdbg.PED.size() > 0 ? RadiationUtils::formatDoseRate(bdbg.PED.at(0)) : "—")
                       .arg(bdbg.PED.size() > 1 ? RadiationUtils::formatDoseRate(bdbg.PED.at(1)) : "—")
                       .arg(mksu.failureFlag ? RadiationUtils::formatDoseRate(mksu.PED) : "—")
                       .arg(mksu.failureFlag ? RadiationUtils::formatDoseValue(mksu.Dose) : "—")
                       .arg(mksu.failureFlag ? TimeUtils::formatSecToHHMMSS(mksu.accumDoseTime) : "—");




    сheckpointModel->addRow(QVector<QVariant>()
                            << SimpleTableModel::CellData(pm)  // GPS данные - нормальные
                            << SimpleTableModel::CellData(am)  // AirMar данные - нормальные
                            // << SimpleTableModel::CellData(lc, lcdHasError)  // LCD с флагами
                            << SimpleTableModel::CellData(bdmk));  // Радиационные данные - нормальные
    emit ui->tableControl->adjustRequested();

}

void DevicePage::onThreatClicked(int index)
{
    mapThreadRequest = true;
    ui->tableView->selectRow(index);
}

void DevicePage::testData()
{
    QString sv = QString("ПЕР.Д.: ..... %1 \n"
                         "ЗАД.Д.: ..... %2 \n"
                         "ЛІВ.Д.: ..... %3 \n"
                         "ПР.Д.:  ..... %4 \n"
                         "Макс. ПАЕД: . %5 \n"
                         "Валідність: . %6 \n");

    QString sr = QString("ПАЕД: ......... %1 \n"
                         "ATOM: ......... %2 \n"
                         "Помилка(РАД): . %3 \n"
                         "BIO: .......... %4 \n"
                         "As: ........... %5 \n"
                         "HNO: .......... %6 \n"
                         "P: ............ %7 \n"
                         "S: ............ %8 \n"
                         "CH: ........... %9 \n"
                         "Помилка(ХІМ): . %10 \n");

    QString cp = QString("Назва газу: .. %1 \n"
                         "Концентрація:  %2 \n"
                         "Помилка: ..... %3 \n");


    QString am = QString("Швидк. вітру: . %1 \n"
                         "Напрям (кор): . %2 \n"
                         "Тиск: ......... %3 \n"
                         "Температура: .. %4 \n"
                         "Вологість: .... %5 \n"
                         "Точка роси: ... %6 \n"
                         "Компас: ....... %7 \n"
                         "Помилка: ...... %8 \n");

    QString pm = QString("Час виміру: . %1 \n"
                         "Широта: ..... %2 \n"
                         "Довгота: .... %3 \n");


    QString sa = QString("Збереження: ... %1");

    for(int i = 0; i < 30; i++){
        сheckpointModel->addRow(QVector<QVariant>() << sv << sr << cp << am << pm << sa);
    }
}
