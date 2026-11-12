#include "searchbar.h"
#include "map/mapcore.h"
#include "ui_searchbar.h"

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SearchBar)
{
    ui->setupUi(this);

    setupUI();
    fillData();
    connections();

}

SearchBar::~SearchBar()
{
    delete ui;
}

void SearchBar::setupUI()
{
    //-------------------------------------------------------------------------//

    radType = new QAction("Радіація", this);
    bioType = new QAction("Біологія", this);
    chemType = new QAction("Хімія", this);

    radType->setCheckable(true);
    bioType->setCheckable(true);
    chemType->setCheckable(true);

    QList<QAction*> typeActionList;

    typeActionList << radType;
#ifdef BRDM
    typeActionList << bioType;
#endif
    typeActionList << chemType;

    ui->pushButtonType->addActions(typeActionList);
    ui->pushButtonType->setInitialText("Тип загроз");

    //-------------------------------------------------------------------------//

    ui->pushButtonFlag->setInitialText("Тип точки");

    pointFlag = new QAction("Точки", this);
    flagFlag = new QAction("Флаги", this);
    routeFlag = new QAction("Маршрути", this);
    objectFlag = new QAction("Об'єкти", this);
    sampleFlag = new QAction("Зразки", this);
    calcZoneFlag = new QAction("Розраховані області", this);
    factZoneFlag = new QAction("Фактичні межі зараженя", this);

    pointFlag       ->setCheckable(true);
    flagFlag        ->setCheckable(true);
    routeFlag       ->setCheckable(true);
    objectFlag      ->setCheckable(true);
    sampleFlag      ->setCheckable(true);
    calcZoneFlag    ->setCheckable(true);
    factZoneFlag    ->setCheckable(true);

    QList<QAction*> flagActionList;
    flagActionList << pointFlag << flagFlag << routeFlag << objectFlag << sampleFlag << calcZoneFlag << factZoneFlag;

    ui->pushButtonFlag->addActions(flagActionList);

    //-------------------------------------------------------------------------//
    ui->pushButtonDate->setButtonType(DropdownButtonType::SINGLE);
    ui->pushButtonDate->setInitialText("Час пошуку");

    allTime = new QAction("За весь час");
    lastHour = new QAction("За останню годину");
    lastDay = new QAction("За останній день");
    lastWeek = new QAction("За останній тиждень");
    lastMonth = new QAction("За останній місяць");
    lastYear = new QAction("За останній рік");
    lastOptional = new QAction("За період...");

    QList<QAction*> dateSearch;
    dateSearch << allTime << lastHour << lastDay << lastWeek << lastMonth << lastYear << lastOptional;

    ui->pushButtonDate->addActions(dateSearch);

    //-------------------------------------------------------------------------//

    ui->pushButtonSquare->setButtonType(DropdownButtonType::SINGLE);
    ui->pushButtonSquare->setInitialText("Площа пошуку");

    areaAll = new QAction("На всій площі");
    area1km = new QAction("В радіусі 1 км");
    area10km = new QAction("В радіусі 10 км");
    area50km = new QAction("В радіусі 50 км");
    area100km = new QAction("В радіусі 100 км");
    area500km = new QAction("В радіусі 500 км");
    areaOptional = new QAction("В заданій області...");

    QList<QAction*> squareSearch;
    squareSearch << areaAll << area1km << area10km << area50km << area100km << area500km <<  areaOptional;
    ui->pushButtonSquare->addActions(squareSearch);

    //-------------------------------------------------------------------------//
    ui->pushButtonOptions->setInitialText("Опції");

    sortByTimeNewFirst = new QAction("Сортувати по часу ↓ (нові спочатку)", this);
    sortByTimeOldFirst = new QAction("Сортувати по часу ↑ (старі спочатку)", this);
    // sortByDistance     = new QAction("Сортувати по відстані ↑ (ближні спочатку)", this);
    sortByConcetration = new QAction("Сортувати по концентрації хімії ↓ (великі спочатку)", this);
    sortByRadiation    = new QAction("Сортувати по концентрації радіації ↓ (великі спочатку)", this);

    sortByTimeNewFirst->setCheckable(true);
    sortByTimeOldFirst->setCheckable(true);
    // sortByDistance->setCheckable(true);
    sortByConcetration->setCheckable(true);
    sortByRadiation->setCheckable(true);

    sortByTimeNewFirst->setChecked(true);

    QActionGroup* optionGroup = new QActionGroup(this);
    optionGroup->setExclusive(true);

    optionGroup->addAction(sortByTimeNewFirst);
    optionGroup->addAction(sortByTimeOldFirst);
    // optionGroup->addAction(sortByDistance);
    optionGroup->addAction(sortByConcetration);
    optionGroup->addAction(sortByRadiation);


    QList<QAction*> optionActionList;
    optionActionList << sortByTimeNewFirst << sortByTimeOldFirst << sortByConcetration << sortByRadiation;

    ui->pushButtonOptions->addActions(optionActionList);

    ddDialog = new DateDateDialog(this);
    asDialog = new AreaSearchDialog(this);

    ui->pushButtonFlag->setFocus();

    setTabOrder({
        ui->pushButtonType,
        ui->pushButtonFlag,
        ui->pushButtonDate,
        ui->pushButtonSquare,
        ui->pushButtonOptions,
        ui->pushButtonSearch
    });
}

void SearchBar::fillData()
{

}

void SearchBar::connections()
{
    connect(ui->pushButtonType->getMenuPtr(), &QMenu::triggered, this, &SearchBar::getDangerTypeFilter);
    connect(ui->pushButtonDate->getMenuPtr(), &QMenu::triggered, this, &SearchBar::getTimeFilter);
    connect(ui->pushButtonSquare->getMenuPtr(), &QMenu::triggered, this, &SearchBar::getDistanceFilter);
    connect(areaOptional, &QAction::triggered, asDialog, &AreaSearchDialog::show);
    connect(lastOptional, &QAction::triggered, ddDialog, &DateDateDialog::show);
    connect(ddDialog, &DateDateDialog::dateDateReady, this, [this](qint64 fromSeconds, qint64 toSeconds){
        timeFrom = fromSeconds;
        timeTo = toSeconds;
    });
    connect(asDialog, &AreaSearchDialog::areaReady, this, [this](QString areaString){
        areaCoordinates.clear();
        areaCoordinates = areaString;
    });

    connect(ui->pushButtonSearch, &QPushButton::clicked, this, &SearchBar::getSelectedFilter);
    connect(ui->pushButtonSearch, &QPushButton::clicked, this, &SearchBar::createQuery);


}

void SearchBar::getSelectedFilter()
{
    // QStringList pointFilter = ui->pushButtonType->getSelectedActions();
    // QStringList flagFilter = ui->pushButtonFlag->getSelectedActions();
}

void SearchBar::getDangerTypeFilter()
{
    // Сбрасываем все типы
    dangerType = 0;

    QList<QAction*> checkedActions = ui->pushButtonType->checkedActions();

    foreach(QAction* action, checkedActions)
    {
        if (action == radType) {
            dangerType |= 1;  // код для радиации
        }
        if (action == chemType) {
            dangerType |= 2;  // код для химии
        }
        if (action == bioType) {
            dangerType |= 4;  // код для биологии
        }
    }

}

void SearchBar::createQuery()
{

QString tableQuery = QString::fromLocal8Bit(
    u8R"(
SELECT
    k.id,
    ('Координати' || CHAR(10) ||
     'ш: ' || ROUND(k.lat, 4) || CHAR(10) ||
     'д: ' || ROUND(k.lon, 4) || CHAR(10) || CHAR(10) ||
     'Час:  ' || time(k.datetime, 'unixepoch', 'localtime') || CHAR(10) ||
     'Дата: ' || strftime('%d.%m.%Y', k.datetime, 'unixepoch', 'localtime')) AS coords_time,

    COALESCE(
        'Швидк. вітру:  ' || CASE WHEN a.w_speed = -1 THEN '—' ELSE ROUND(a.w_speed, 1) || ' м/с' END || CHAR(10) ||
        'Температура: . ' || ROUND(a.tempr, 2)      || ' °C'   || CHAR(10) ||
        'Вологість: ... ' || a.vlagnost             || ' %'    || CHAR(10) ||
        'Тиск: ........ ' || a.davlenie             || ' Па'   || CHAR(10) ||
        'Точка роси: .. ' || ROUND(a.t_rosy, 2)     || ' °C'   || CHAR(10) ||
        'Напрям (скор): ' || CASE WHEN a.w_direct = -1 THEN '—' ELSE a.w_direct || ' °' END,
        'Дані відсутні'
    ) AS weather_data,

    COALESCE(
        GROUP_CONCAT(
            'Тип агента: . ' || COALESCE(at.type, 'Невідомий тип') || CHAR(10) ||
            'Назва: ...... ' || COALESCE(at.name, 'Невідома') || CHAR(10) ||
            'Концентрація: ' || ROUND(l.conc, 2)        || ' мкг/м³'        || CHAR(10) ||
            'Доза: ....... ' || ROUND(l.dose, 2)        || ' мкг-хв/м³',
            CHAR(10) || CHAR(10)  -- Двойной перенос строки между газами
        ),
        'Дані відсутні'
    ) AS gas_data,

    COALESCE(
        'ПАЕД БДБГ-Т:  ' || CASE WHEN b.ped IS NULL THEN '—' ELSE ROUND(b.ped, 2) || ' мкЗв/г' END || CHAR(10) ||
        'ПАЕД МКС-УМ:  ' || CASE WHEN m.ped IS NULL THEN '—' ELSE ROUND(m.ped, 2) || ' мкЗв/г' END || CHAR(10) ||
        'Доза: ....... ' || CASE WHEN m.dose IS NULL THEN '—' ELSE ROUND(m.dose, 2) || ' мкЗв' END || CHAR(10) ||
        'Час накоп.: . ' || CASE WHEN m.accum_time IS NULL THEN '—' ELSE time(m.accum_time, 'unixepoch') END || CHAR(10) ||
        '              гг:хх:сс',
        'Дані відсутні'
    ) AS radiation_data,

    -- Добавляем поля для сортировки и фильтрации
    -- k.dangerType,
    k.datetime

FROM keys k
LEFT JOIN airmar a ON k.id = a.id
LEFT JOIN lcd4  l ON k.id = l.id
LEFT JOIN lcd_agent_lib at ON l.type_agent = at.id
LEFT JOIN mksu  m ON k.id = m.id
LEFT JOIN ( SELECT id, MAX(ped) AS ped FROM bdbg GROUP BY id ) b ON k.id = b.id

WHERE 1=1
    )"
        );

    // Добавляем фильтры динамически WHERE (dangerType & ?) > 0
    if (dangerType > 0) {
        tableQuery += QString(" AND (k.dangerType & %1) > 0").arg(dangerType);
    }

    if (timeFrom > 0) {
        tableQuery += QString(" AND k.datetime >= %1").arg(timeFrom);
    }

    if (timeTo > 0) {
        tableQuery += QString(" AND k.datetime <= %1").arg(timeTo);
    }

    if (radiusMeters > 0) {
        QString circlePolygon = CirclePolygonGenerator::generateCirclePolygonAccurate( centerLat, centerLon, radiusMeters, 16);
        tableQuery += QString(" AND pointInAreaForSQL(k.lat, k.lon, '%1') = 1").arg(circlePolygon);
    }

    if (searchInArea && !areaCoordinates.isEmpty()) {
        tableQuery += QString(" AND pointInAreaForSQL(k.lat, k.lon, '%1') = 1") .arg(areaCoordinates);
    }

    tableQuery += " GROUP BY k.id, k.lon, k.lat, k.datetime, k.dangerType";
    tableQuery += getSortFilter();

    emit tableQueryRequested(tableQuery);




QString mapQuery = QString::fromLocal8Bit(
    u8R"(
SELECT
    k.id, k.lat, k.lon, k.dangerType
FROM keys k
LEFT JOIN airmar a ON k.id = a.id
LEFT JOIN lcd4  l ON k.id = l.id
LEFT JOIN lcd_agent_lib at ON l.type_agent = at.id
LEFT JOIN mksu  m ON k.id = m.id
LEFT JOIN bdbg  b ON k.id = b.id
WHERE 1=1
    )"
);

    // Добавляем фильтры динамически WHERE (dangerType & ?) > 0
    if (dangerType > 0) {
        mapQuery += QString(" AND (k.dangerType & %1) > 0").arg(dangerType);
    }

    if (timeFrom > 0) {
        mapQuery += QString(" AND k.datetime >= %1").arg(timeFrom);
    }

    if (timeTo > 0) {
        mapQuery += QString(" AND k.datetime <= %1").arg(timeTo);
    }

    if (radiusMeters > 0) {
        QString circlePolygon = CirclePolygonGenerator::generateCirclePolygonAccurate(centerLat, centerLon, radiusMeters, 16);
        mapQuery += QString(" AND pointInAreaForSQL(k.lat, k.lon, '%1') = 1").arg(circlePolygon);
        emit querySearchAreaFormed(circlePolygon);
    }
    else {
        emit querySearchAreaFormed(QString());  // Clear SearchArea section if searn in all map area
    }

    if (searchInArea && !areaCoordinates.isEmpty()) {
        mapQuery += QString(" AND pointInAreaForSQL(k.lat, k.lon, '%1') = 1") .arg(areaCoordinates);
        emit querySearchAreaFormed(areaCoordinates);
    }

    mapQuery += " GROUP BY k.id, k.lon, k.lat, k.datetime, k.dangerType";
    mapQuery += getSortFilter();

    emit mapQueryRequested(mapQuery);

}

void SearchBar::onCenterMarkerChanged(smappoint centerPosition)
{
    centerLat = centerPosition.latt;
    centerLon = centerPosition.longt;
}

void SearchBar::getTimeFilter(QAction *action)
{
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();

    timeFrom = 0;
    timeTo = 0;

    if (action == allTime) {
        // Без ограничений по времени
    }
    else if (action == lastHour) {
        timeFrom = currentTime - 3600;  // час назад
        timeTo = currentTime;
    }
    else if (action == lastDay) {
        timeFrom = currentTime - 86400;  // день назад
        timeTo = currentTime;
    }
    else if (action == lastWeek) {
        timeFrom = currentTime - 604800;  // неделя назад
        timeTo = currentTime;
    }
    else if (action == lastMonth) {
        timeFrom = currentTime - 2592000;  // месяц назад (30 дней)
        timeTo = currentTime;
    }
    else if (action == lastYear) {
        timeFrom = currentTime - 31536000;  // год назад
        timeTo = currentTime;
    }
    else if (action == lastOptional) {
        // Открываем диалог выбора периода
        // getTimeFilter();
        return;  // не вызываем createQuery() пока пользователь не выберет период
    }
}

void SearchBar::getDistanceFilter(QAction *action)
{
    radiusMeters = -1;  // сброс
    searchInArea = false;

    if (action == areaAll) {
        // Без ограничений
    }
    else if (action == area1km) {
        radiusMeters = 1000;
    }
    else if (action == area10km) {
        radiusMeters = 10000;
    }
    else if (action == area50km) {
        radiusMeters = 50000;
    }
    else if (action == area100km) {
        radiusMeters = 100000;
    }
    else if (action == area500km) {
        radiusMeters = 500000;
    }
    else if (action == areaOptional) {
        searchInArea = true;
        return;  // не вызываем createQuery() пока пользователь не выберет область
    }

}

QString SearchBar::getSortFilter()
{
    QString sortString;
    sortString.clear();

    if (sortByTimeNewFirst->isChecked()) {
        sortString = " ORDER BY k.datetime DESC";
    }
    else if (sortByTimeOldFirst->isChecked()) {
        sortString = " ORDER BY k.datetime ASC";
    }
    else if (sortByConcetration->isChecked()) {
        sortString = " ORDER BY MAX(l.conc) DESC";
    }
    else if (sortByRadiation->isChecked()) {
        sortString = " ORDER BY MAX(COALESCE(b.ped, m.ped)) DESC";
    }
    else {
        sortString = " ORDER BY k.datetime DESC";
    }
    return sortString;
}


QWidget *SearchBar::getWidget(WidgetType type) const
{
    switch (type) {
        case WidgetType::Type:    return ui->pushButtonType;
        case WidgetType::Flag:    return ui->pushButtonFlag;
        case WidgetType::Date:    return ui->pushButtonDate;
        case WidgetType::Square:  return ui->pushButtonSquare;
        case WidgetType::Options: return ui->pushButtonOptions;
        case WidgetType::Search:  return ui->pushButtonSearch;
    }
    return nullptr;
}

