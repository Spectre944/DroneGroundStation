#include "databasepage.h"
#include "ui_databasepage.h"

DatabasePage::DatabasePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DatabasePage)
{
    ui->setupUi(this);

    setupUI();
    fillData();
    connections();

    //--------------------------------------------------------------------//
    //                        Тестовая секция                             //
    //--------------------------------------------------------------------//

    testData();

    //--------------------------------------------------------------------//
    //                        Тестовая секция                             //
    //--------------------------------------------------------------------//


}

DatabasePage::~DatabasePage()
{
    delete ui;
}

void DatabasePage::updateTableData(const QVector<QVector<QVariant>> &data)
{
    simplelModel->clearData();
    simplelModel->updateData(data);
    emit ui->tableControl->adjustRequested();
}

void DatabasePage::updateMapData(const QVector<QVector<QVariant>> &data)
{
    archiveDangers.clear();
    archiveDangers.reserve(data.size()); // Оптимизация памяти

    QVector<smarker> markers;
    markers.reserve(data.size());

    tsmarker tmp;
    foreach (const QVector<QVariant> &point, data) {
        tmp.pt.latt = point.at(1).toDouble();
        tmp.pt.longt = point.at(2).toDouble();
        tmp.type = point.at(3).toInt();
        archiveDangers.push_back(tmp);
        markers.push_back(tmp);
    }

    // Один сигнал со всеми данными
    emit archiveDataLoaded(markers);
}

void DatabasePage::onCenterMarkerChanged(smappoint centerPosition)
{
    ui->widgetSerach->onCenterMarkerChanged(centerPosition);
}

void DatabasePage::onArchiveThreatClicked(int index)
{
    mapThreadRequest = true;
    simpleTableView->selectRow(index);
}

void DatabasePage::onArchiveDBOpened(QString name)
{
    if(!name.isEmpty())
        ui->tableControl->setTableName(QString("База даних %1").arg(name));
}

void DatabasePage::setupUI()
{
    ui->tableControl->setTableName("База даних");
    ui->tableControl->setIcon(QPixmap(":/icons/png/old/databaseColor.png"));
    ui->tableControl->setNavigatorBtnVisible(false);

#ifdef DZURA
    simplelModel = new SimpleTableModel(this);
    simplelModel->setHeaderDataFromList(QStringList() << "ID" << "Параметри" << "AirMarWX200" << "LCD4" << "БДБГ-Т/МКС-УМ");
    simpleTableView = new QTableView(this);
    simpleTableView->setModel(simplelModel);
    simpleTableView->setFont(QFont("Consolas"));
    simpleTableView->setAlternatingRowColors(true);
    simpleTableView->setTabKeyNavigation(false);
    simpleTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    simpleTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableLayout->addWidget(simpleTableView);

    // Hide flags type for Dzura (because it only one type of savepoints)
    ui->widgetSerach->getWidget(SearchBar::WidgetType::Flag)->setVisible(false);
#endif

#ifdef BRDM
    stickyModel = new StickyTableModel(this);
    stickyTableView = new StickyTableView(this);
    stickyTableView->setModel(stickyModel);
    stickyTableView->setFont(QFont("Consolas"));
    stickyTableView->setAlternatingRowColors(true);
    stickyTableView->setTabKeyNavigation(false);
    stickyTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableLayout->addWidget(stickyTableView);
    tableNavigator = new TableNavigator(stickyTableView, this);
    tableNavigator->updateSectionList();
#endif

    ui->tableControl->setFocus();

    setTabOrder({
        ui->widgetSerach->getWidget(SearchBar::WidgetType::Type),
        ui->widgetSerach->getWidget(SearchBar::WidgetType::Flag),
        ui->widgetSerach->getWidget(SearchBar::WidgetType::Date),
        ui->widgetSerach->getWidget(SearchBar::WidgetType::Square),
        ui->widgetSerach->getWidget(SearchBar::WidgetType::Options),
        ui->widgetSerach->getWidget(SearchBar::WidgetType::Search),
        ui->tableControl->getWidget(TableControl::WidgetType::Clear),
        ui->tableControl->getWidget(TableControl::WidgetType::Navigator),
        ui->tableControl->getWidget(TableControl::WidgetType::Adjust),
    });
}

void DatabasePage::fillData()
{

}

void DatabasePage::connections()
{
    connect(ui->widgetSerach, &SearchBar::tableQueryRequested, this, &DatabasePage::tableQueryRequested);
    connect(ui->widgetSerach, &SearchBar::mapQueryRequested, this, &DatabasePage::mapQueryRequested);
    connect(ui->widgetSerach, &SearchBar::querySearchAreaFormed, this, &DatabasePage::querySearchAreaFormed);

#ifdef DZURA
    connect(ui->tableControl, &TableControl::clearRequested, simplelModel, &SimpleTableModel::clearData);
    connect(ui->tableControl, &TableControl::clearRequested, this, &DatabasePage::clearArchiveMapDataRequested);
    connect(ui->tableControl, &TableControl::adjustRequested, this, [this]() {
        simpleTableView->resizeColumnsToContents();
        simpleTableView->resizeRowsToContents();
    });

    connect(simpleTableView->selectionModel(), &QItemSelectionModel::currentChanged,  this, [this](const QModelIndex &current, const QModelIndex &) {
        if (current.isValid()) {
            int rowIndex = current.row();

            if (!mapThreadRequest) {
                emit archiveTableThreatClicked(rowIndex);
            }

            mapThreadRequest = false;
        }
    });

#endif

#ifdef BRDM
    connect(ui->tableControl, &TableControl::clearRequested, stickyModel, &StickyTableModel::clearData);
    connect(ui->tableControl, &TableControl::adjustRequested, this, [this]() {
        stickyTableView->resizeColumnsToContents();
        stickyTableView->resizeRowsToContents();
    });

    connect(ui->tableControl, &TableControl::navigationPanelRequested, this, [this](){
        tableNavigator->updateSectionList();
        tableNavigator->show();
    });
#endif

}

void DatabasePage::testData()
{

    if(stickyModel){

        QStringList dataHeaders;
        QVector<QVariant> rowData;
        QVector<QVector<QVariant>> tableData;
        QColor cellColor(138, 168, 219);

        //---------------------------------------------------------------------------//
        //                               Точки                                       //
        //---------------------------------------------------------------------------//

        dataHeaders = {"ID", "Параметри", "AirMarWX200", "LCD4", "СВНГ-Т/СРХБР-Т"};

        QString param = QString("Координати\n"
                                "д: 24.53634\n"
                                "ш: 54.25334\n"
                                "\n"
                                "Дата та час\n"
                                "04.06.25 17.00\n"
                                "\n"
                                "Засіб доставки\n"
                                "Невідомо\n"
                                "\n"
                                "Загрози\n"
                                "Рад: + \n"
                                "Хім: - \n"
                                "Біо: - ");

        QString am = QString("Швидк. вітру: ... 1.2 м/с \n"
                             "Температура: .... 24.2 °C \n"
                             "Вологість: ...... 43 % \n"
                             "Тиск: ........... 1054 Па \n"
                             "Точка роси: ..... 14.2 °C \n"
                             "Напрям (скор): .. 256° \n"
                             "\n"
                             "Стійкість пов.: . S \n"
                             );

        QString lcd = QString("Тип газу: ... Загальноотруйний\n"
                              "Концентрація: 1.2 мкг/м³\n"
                              "\n"
                              "Доза: ....... 43 мкг-хв/м³\n");

        QString sr = QString("Макс. ПАЕД СВНГ: .... 1.2 мкЗв/г \n"
                             "Макс. ПАЕД СРХБР: ... 0.4 мкЗв/г \n"
                             "Біочастинки: ........ + \n"
                             "Вуглеводень: ........ 10% \n"
                             "Миш'як: ............. 0.01 мкг/м³ \n"
                             "Нітратна кислота: ... 0.02 мкг/м³ \n"
                             "Фосфор: ............. 0.04 мкг/м³ \n"
                             "Сірка: .............. 0.04 мкг/м³ \n");


        rowData     = {0, param, am, lcd, sr};
        tableData.clear();
        for(int i = 0; i < 20; i++){ tableData.append(rowData); }
        stickyModel->addSection("Точки", dataHeaders, tableData, cellColor);

        //---------------------------------------------------------------------------//
        //                              Маршрути                                     //
        //---------------------------------------------------------------------------//

        QString paramWay = QString("Координати\n"
                                "д: 24.53634\n"
                                "ш: 54.25334\n"
                                "\n"
                                "Дата та час\n"
                                "04.06.25 17.00");

        QString amWay = QString("Швидк. вітру: ... 1.2 м/с \n"
                             "Температура: .... 24.2 °C \n"
                             "Вологість: ...... 43 % \n"
                             "Тиск: ........... 1054 Па \n"
                             "Точка роси: ..... 14.2 °C \n"
                             "Напрям (скор): .. 256° \n"
                             "\n"
                             "Стійкість пов.: . S \n"
                             );

        QString lcdWay = QString("Тип газу: ... Загальноотруйний\n"
                              "Концентрація: 1.2 мкг/м³\n"
                              "\n"
                              "Доза: ....... 43 мкг-хв/м³\n");

        QString srWay = QString("Макс. ПАЕД СВНГ: .... 1.2 мкЗв/г \n"
                             "Макс. ПАЕД СРХБР: ... 0.4 мкЗв/г \n"
                             "Біочастинки: ........ + \n"
                             "Вуглеводень: ........ 10% \n"
                             "Миш'як: ............. 0.01 мкг/м³ \n"
                             "Нітратна кислота: ... 0.02 мкг/м³ \n"
                             "Фосфор: ............. 0.04 мкг/м³ \n"
                             "Сірка: .............. 0.04 мкг/м³ \n");


        dataHeaders = {"ID", "Параметри", "AirMarWX200", "LCD4", "СВНГ/СРХБР"};
        rowData     = {0, paramWay, amWay, lcdWay, srWay};
        tableData.clear();
        for(int i = 0; i < 20; i++){ tableData.append(rowData); }
        stickyModel->addSection("Маршрут", dataHeaders, tableData, cellColor.darker(100));

        //---------------------------------------------------------------------------//
        //                              Проби                                        //
        //---------------------------------------------------------------------------//

        QString paramSam = QString("Координати\n"
                                   "д: 24.53634\n"
                                   "ш: 54.25334\n"
                                   "\n"
                                   "Дата та час\n"
                                   "04.06.25 17.00\n"
                                   "\n"
                                   "Тип зразка\n"
                                   "Рад: + \n"
                                   "Хім: - \n"
                                   "Біо: - \n"
                                   "\n"
                                   "Тип грунту\n"
                                   "Сипучий\n");

        QString amSam = QString("Швидк. вітру: ... 1.2 м/с \n"
                                "Температура: .... 24.2 °C \n"
                                "Вологість: ...... 43 % \n"
                                "Тиск: ........... 1054 Па \n"
                                "Точка роси: ..... 14.2 °C \n"
                                "Напрям (скор): .. 256° \n"
                                "\n"
                                "Стійкість пов.: . S \n"
                                );

        QString lcdSam = QString("Тип газу: ... Загальноотруйний\n"
                                 "Концентрація: 1.2 мкг/м³\n"
                                 "Доза: ....... 43 мкг-хв/м³\n"
                                 "\n"
                                 "Макс. ПАЕД СВНГ: .... 1.2 мкЗв/г \n"
                                 "Макс. ПАЕД СРХБР: ... 0.4 мкЗв/г \n"
                                 "Біочастинки: ........ + \n"
                                 "Вуглеводень: ........ 10% \n"
                                 "Миш'як: ............. 0.01 мкг/м³ \n"
                                 "Нітратна кислота: ... 0.02 мкг/м³ \n"
                                 "Фосфор: ............. 0.04 мкг/м³ \n"
                                 "Сірка: .............. 0.04 мкг/м³ \n"
                                 "");

        QString comment = QString("Зразок відібрано біля ЧАЕС");



        dataHeaders = {"ID", "Параметри", "Погодні умови", "Тип небезпеки", "Коментар командира"};
        rowData     = {0, paramSam, amSam, lcdSam, comment};
        tableData.clear();
        for(int i = 0; i < 5; i++){ tableData.append(rowData); }
        stickyModel->addSection("Проби", dataHeaders, tableData, cellColor.darker(120));

        //---------------------------------------------------------------------------//
        //                              Об`єкти                                      //
        //---------------------------------------------------------------------------//

        QString paramObj = QString("Координати\n"
                                   "д: 24.53634\n"
                                   "ш: 54.25334\n"
                                   "\n"
                                   "Дата та час\n"
                                   "04.06.25 17.00");

        QString typeObj = QString("Хімічно небезпечні об'єкти");

        QString dangerRadiusObj = QString("20 км");

        QString infoObj = QString("Описова інформація про об'єкт");

        dataHeaders = {"ID", "Параметри", "Тип об'єкту", "Радіус небезпеки", "Описова інформація"};
        rowData     = {0, paramObj, typeObj, dangerRadiusObj, infoObj};
        tableData.clear();
        for(int i = 0; i < 20; i++){ tableData.append(rowData); }
        stickyModel->addSection("Об`єкти", dataHeaders, tableData, cellColor.darker(140));

        //---------------------------------------------------------------------------//
        //                              Області                                      //
        //---------------------------------------------------------------------------//

        dataHeaders = {"ID", "Широта", "Довгота", "Площа\nобласті\nвисокої\nнебезпеки", "Площа\nобласті\nсередньої\nнебезпеки", "Площа\nобласті\nдовгого\nперебування", "Швидкість\nвітру", "Напрям\nвітру", "Тип\nзагрози", "Інтенсивність\nзагрози", "Потужність\nвибуху", "Назва елементу", "Стійкість елем\n(хім)", "Тип токсичності\n(хім)", "Інформація"};
        rowData     = {"1", "50.0811", "23.7731", "2 km", "10 km", "100 km", "2 м/с", "22°", "Радіаційна небезпека", "---", "50 км", "Аміак", "Стійкий", "Задушливої дії", ""};
        tableData.clear();
        for(int i = 0; i < 100; i++){ tableData.append(rowData); }
        stickyModel->addSection("Області", dataHeaders, tableData, cellColor.darker(160));

        //---------------------------------------------------------------------------//
        //                                                                           //
        //---------------------------------------------------------------------------//

    }



}
