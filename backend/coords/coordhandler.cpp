#include "coordhandler.h"

CoordHandler::CoordHandler(QObject *parent): QObject{parent}
{

}

QPair<bool, QString> CoordHandler::convertSingleCoord(const QString &coord, const CoordType &type)
{
    /**
        @brief Преобразование одной координаты в другую

    */
    QPair<bool, QString> pair(false, QString());

    QPair<bool, CoordType> test = CoordHandler::defining_type_coord(coord);
    if(!test.first)
        return pair;

    /**
        @brief В соответствии с типом выбранной координаты
              переводим её в пару (double, double) WGS84
     */
    CoordType local_type = test.second;

    QPair<bool, QPair<double, double>> wgs_pair;

    switch(local_type) {

    case CoordType::WGS84_String:
        wgs_pair = CoordHandler::Wgs84S_to_Wgs84P(coord);
        break;

    case CoordType::MGRS:
        wgs_pair = CoordHandler::MGRS_to_Wgs84P(coord);
        break;

    case CoordType::UTM:
        wgs_pair = CoordHandler::UTM_to_Wgs84P(coord);
        break;

    case CoordType::DMS:
        wgs_pair = CoordHandler::DMS_to_Wgs84P(coord);
        break;

    case CoordType::CK42:
        wgs_pair = CoordHandler::CK42_to_Wgs84P(coord);
        break;

    default:
        return pair;
    }

    /**
    @brief Координата не валидна
    */
    if(!wgs_pair.first)
        return pair;


    switch(type) {
        case CoordType::WGS84_String:
            pair = CoordHandler::Wgs84P_to_Wgs84S(wgs_pair.second);
            break;
        case CoordType::MGRS:
            pair = CoordHandler::Wgs84P_to_MGRS(wgs_pair.second);
            break;
        case CoordType::DMS:
            pair = CoordHandler::Wgs84P_to_DMS(wgs_pair.second);
            break;
        case CoordType::UTM:
            pair = CoordHandler::Wgs84P_to_UTM(wgs_pair.second);
            break;
        case CoordType::CK42:
            pair = CoordHandler::Wgs84P_to_CK42(wgs_pair.second);
            break;
        default:
            break;
    }

    return pair;
}

QPair<bool, QString> CoordHandler::convertPackageCoords(const QString &coord, const CoordType &type)
{

    /**
        @brief Преобразование пакета координат в другой пакет

    */
    QPair<bool, QString> pair(false, QString());

    QString _cleanStr = coord;
    _cleanStr.remove(' ');

    while (_cleanStr.endsWith('/'))
        _cleanStr.chop(1);

    QStringList coord_list = _cleanStr.split('/', Qt::SkipEmptyParts);

    if(coord_list.isEmpty())
        return pair;

    for (QString &s : coord_list) {
        s.remove(QChar(' '));
        s.remove(QChar('\n'));
    }


    /**
        @brief В defining_type_coord определяем тип координаты, парсинг значения
               координаты производим в функциях преобразования любого типа строковой
               координаты в пару WGS84
    */
    QPair<bool, CoordType> test = CoordHandler::defining_type_coord(coord_list.at(0));

    if(!test.first)
        return pair;

    CoordType local_type = test.second;
    QPair<bool, QList<QPair<double, double>>> wgs_pair_list = qMakePair(false, QList<QPair<double, double>>());

    switch(local_type) {

        case CoordType::WGS84_String:
            wgs_pair_list = CoordHandler::Wgs84S_to_Wgs84P_List(coord_list);
            break;
        case CoordType::MGRS:
            wgs_pair_list = CoordHandler::MGRS_to_Wgs84P_List(coord_list);
            break;
        case CoordType::UTM:
            wgs_pair_list = CoordHandler::UTM_to_Wgs84P_List(coord_list);
            break;

        case CoordType::DMS:
            wgs_pair_list = CoordHandler::DMS_to_Wgs84P_List(coord_list);
            break;

        case CoordType::CK42:
            wgs_pair_list = CoordHandler::CK42_to_Wgs84P_List(coord_list);
            break;

        default:
            return pair;
    }

    /**
    @brief Координата не валидна
    */
    if(!wgs_pair_list.first)
        return pair;

    switch(type) {
        case CoordType::WGS84_String:
            pair = CoordHandler::Wgs84P_to_Wgs84S_List(wgs_pair_list.second);
            break;
        case CoordType::MGRS:
            pair = CoordHandler::Wgs84P_to_MGRS_List(wgs_pair_list.second);
            break;
        case CoordType::DMS:
            pair = CoordHandler::Wgs84P_to_DMS_List(wgs_pair_list.second);
            break;
        case CoordType::UTM:
            pair = CoordHandler::Wgs84P_to_UTM_List(wgs_pair_list.second);
            break;
        case CoordType::CK42:
            pair = CoordHandler::Wgs84P_to_CK42S_List(wgs_pair_list.second);
            break;
        default:
            break;
    }

    return pair;

}

QPair<bool, QVector<smappoint>> CoordHandler::convertPackageCoordsToMapPoints(const QString &coord)
{
    /**
        @brief Преобразование пакета координат в QVector<smappoint> (всегда WGS84)
    */
    QPair<bool, QVector<smappoint>> result(false, QVector<smappoint>());

    QString _cleanStr = coord;
    _cleanStr.remove(' ');

    while (_cleanStr.endsWith('/'))
        _cleanStr.chop(1);

    QStringList coord_list = _cleanStr.split('/', Qt::SkipEmptyParts);

    if(coord_list.isEmpty())
        return result;

    for (QString &s : coord_list) {
        s.remove(QChar(' '));
        s.remove(QChar('\n'));
    }

    /**
        @brief В defining_type_coord определяем тип координаты, парсинг значения
               координаты производим в функциях преобразования любого типа строковой
               координаты в пару WGS84
    */
    QPair<bool, CoordType> test = CoordHandler::defining_type_coord(coord_list.at(0));

    if(!test.first)
        return result;

    CoordType local_type = test.second;
    QPair<bool, QList<QPair<double, double>>> wgs_pair_list = qMakePair(false, QList<QPair<double, double>>());

    switch(local_type) {
    case CoordType::WGS84_String:
        wgs_pair_list = CoordHandler::Wgs84S_to_Wgs84P_List(coord_list);
        break;
    case CoordType::MGRS:
        wgs_pair_list = CoordHandler::MGRS_to_Wgs84P_List(coord_list);
        break;
    case CoordType::UTM:
        wgs_pair_list = CoordHandler::UTM_to_Wgs84P_List(coord_list);
        break;
    case CoordType::DMS:
        wgs_pair_list = CoordHandler::DMS_to_Wgs84P_List(coord_list);
        break;
    case CoordType::CK42:
        wgs_pair_list = CoordHandler::CK42_to_Wgs84P_List(coord_list);
        break;
    default:
        return result;
    }

    /**
    @brief Координата не валидна
    */
    if(!wgs_pair_list.first)
        return result;

    // Преобразуем QList<QPair<double, double>> в QVector<smappoint>
    QVector<smappoint> points;
    for(const QPair<double, double> &coordPair : wgs_pair_list.second) {
        smappoint point;
        point.latt = coordPair.first;   // latitude
        point.longt = coordPair.second; // longitude
        points.append(point);
    }

    result.first = true;
    result.second = points;

    return result;
}

QList<bool> CoordHandler::pointInArea(const QString &point_coord, const QString &area_coords)
{
    /**
        @brief Формат list:
                list[0](bool)      - валидность 1 координаты
                list[1](bool)      - валидность 2 координаты
                list[2](bool)      - входит - не входит
    */


    QList<bool> list = {false, true, false};

    QString p_coord  = point_coord;
    QString a_coords = area_coords;

    p_coord = p_coord.trimmed();
    p_coord.remove(' ');
    while (p_coord.endsWith('/'))
        p_coord.chop(1);

    QPair<bool, CoordType> check_p_coord = CoordHandler::defining_type_coord(p_coord);

    if(!check_p_coord.first)
        return list;


    CoordType local_type = check_p_coord.second;
    QPair<bool, QPair<double, double>> wgs_pair;
    switch(local_type) {

        case CoordType::WGS84_String:
            wgs_pair = CoordHandler::Wgs84S_to_Wgs84P(p_coord);
            break;
        case CoordType::MGRS:
            wgs_pair = CoordHandler::MGRS_to_Wgs84P(p_coord);
            break;
        case CoordType::UTM:
            wgs_pair = CoordHandler::UTM_to_Wgs84P(p_coord);
            break;
        case CoordType::DMS:
            wgs_pair = CoordHandler::DMS_to_Wgs84P(p_coord);
            break;
        case CoordType::CK42:
            wgs_pair = CoordHandler::CK42_to_Wgs84P(p_coord);
            break;
        default:
            return list;
    }

    /**
        @brief Координата не валидна
    */
    if(!wgs_pair.first)
        return list;

    list[0] = true;
    list[1] = false;
    ///////   wgs_pair.second  - пара координат тестовой точки

    QString coord_str(area_coords);

    coord_str = coord_str.trimmed();
    coord_str.remove(' ');

    while (coord_str.endsWith('/'))
        coord_str.chop(1);

    QStringList coord_list = coord_str.split('/', Qt::SkipEmptyParts);

    if(coord_list.isEmpty())
        return list;

    for (QString &s : coord_list) {
        s.remove(QChar(' '));
        s.remove(QChar('\n'));
    }

    /**
        @brief В defining_type_coord определяем тип координаты, парсинг значения
               координаты производим в функциях преобразования любого типа строковой
               координаты в пару WGS84
    */

    check_p_coord = CoordHandler::defining_type_coord(coord_list.at(0));

    if(!check_p_coord.first)
        return list;

    QPair<bool, QList<QPair<double, double>>> wgs_pair_list = qMakePair(false, QList<QPair<double, double>>());

    local_type = check_p_coord.second;

    switch(local_type) {

        case CoordType::WGS84_String:
            wgs_pair_list = CoordHandler::Wgs84S_to_Wgs84P_List(coord_list);
            break;
        case CoordType::MGRS:
            wgs_pair_list = CoordHandler::MGRS_to_Wgs84P_List(coord_list);
            break;
        case CoordType::UTM:
            wgs_pair_list = CoordHandler::UTM_to_Wgs84P_List(coord_list);
            break;

        case CoordType::DMS:
            wgs_pair_list = CoordHandler::DMS_to_Wgs84P_List(coord_list);
            break;

        case CoordType::CK42:
            wgs_pair_list = CoordHandler::CK42_to_Wgs84P_List(coord_list);
            break;

        default:
            return list;
    }

    /**
        @brief Координата не валидна
    */
    if(!wgs_pair_list.first)
        return list;

    ///////   wgs_pair_list.second - список координат области

    list[1] = true;
    list[2] =  CoordHandler::check_Point_In_Area(wgs_pair_list.second, wgs_pair.second);

    return list;
}

void CoordHandler::pointInAreaForSQL(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 3) {
        sqlite3_result_error(context, "pointInAreaForSQL requires 3 arguments: lat, lon, area_coords_string", -1);
        return;
    }

    double lat = sqlite3_value_double(argv[0]);
    double lon = sqlite3_value_double(argv[1]);
    const char* area_coords_cstr = (const char*)sqlite3_value_text(argv[2]);

    // Проверяем на NULL или пустую строку области
    if (!area_coords_cstr || strlen(area_coords_cstr) == 0) {
        sqlite3_result_int(context, 0);
        return;
    }

    // Проверяем валидность координат точки
    // Широта должна быть в диапазоне [-90, 90], долгота [-180, 180]
    if (lat == 0.0 && lon == 0.0) {
        // Точка с нулевыми координатами - не входит в область
        sqlite3_result_int(context, 0);
        return;
    }

    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        // Невалидные координаты - не входит в область
        sqlite3_result_int(context, 0);
        return;
    }

    // Формируем строку координаты точки в формате WGS84
    QString point_coord = QString("WGS84:%1, %2//").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);

    // Преобразуем area_coords в QString
    QString area_coords = QString::fromUtf8(area_coords_cstr);

    try {
        // Используем существующую функцию pointInArea
        QList<bool> result = CoordHandler::pointInArea(point_coord, area_coords);

        // Проверяем результат более детально
        if (result.size() < 3) {
            // Недостаточно элементов в результате
            sqlite3_result_int(context, 0);
            return;
        }

        if (!result[0]) {
            // Координата точки не валидна - возвращаем 0 вместо ошибки
            sqlite3_result_int(context, 0);
            return;
        }

        if (!result[1]) {
            // Координаты области не валидны - возвращаем 0 вместо ошибки
            sqlite3_result_int(context, 0);
            return;
        }

        // Возвращаем результат проверки
        sqlite3_result_int(context, result[2] ? 1 : 0);
    }
    catch (...) {
        // Любое исключение - возвращаем 0 вместо ошибки
        sqlite3_result_int(context, 0);
    }

}

QVariantList CoordHandler::distanceBetweenPoints(const QString &point1, const QString &point2)
{
    /**
        @brief Расчет расстояния между 2 точками
        @return Формат QVariantList result
                result[0](bool)    -   валидность 1 координаты
                result[1](bool)    -   валидность 2 координаты
                result[2](double)  -   расстояние в метрах
                result[3](double)  -   расстояние в километрах
    */

    QVariantList res_list = {false, true, 0.0, 0.0};

    QString start_coord(point1);
    start_coord = start_coord.trimmed();
    start_coord.remove(' ');
    while (start_coord.endsWith('/'))
        start_coord.chop(1);

    QPair<bool, CoordType> test = CoordHandler::defining_type_coord(start_coord);

    if(!test.first)
        return res_list;

    CoordType local_type = test.second;
    QPair<bool, QPair<double, double>> wgs_pair;
    switch(local_type) {

        case CoordType::WGS84_String:
            wgs_pair = CoordHandler::Wgs84S_to_Wgs84P(start_coord);
            break;
        case CoordType::MGRS:
            wgs_pair = CoordHandler::MGRS_to_Wgs84P(start_coord);
            break;
        case CoordType::UTM:
            wgs_pair = CoordHandler::UTM_to_Wgs84P(start_coord);
            break;
        case CoordType::DMS:
            wgs_pair = CoordHandler::DMS_to_Wgs84P(start_coord);
            break;
        case CoordType::CK42:
            wgs_pair = CoordHandler::CK42_to_Wgs84P(start_coord);
            break;
        default:
            break;
    }

    /**
    @brief Координата не валидна
    */
    if(!wgs_pair.first)
        return res_list;

    //   point_1  - пара координат 1 тестовой точки
    QPair<double, double> point_1 = wgs_pair.second;


    start_coord = point2;
    start_coord = start_coord.trimmed();
    start_coord.remove(' ');
    while (start_coord.endsWith('/'))
        start_coord.chop(1);

    res_list[0] = true;
    res_list[1] = false;

    test = CoordHandler::defining_type_coord(start_coord);

    if(!test.first)
        return res_list;

    local_type = test.second;
    switch(local_type) {

        case CoordType::WGS84_String:
            wgs_pair = CoordHandler::Wgs84S_to_Wgs84P(start_coord);
            break;
        case CoordType::MGRS:
            wgs_pair = CoordHandler::MGRS_to_Wgs84P(start_coord);
            break;
        case CoordType::UTM:
            wgs_pair = CoordHandler::UTM_to_Wgs84P(start_coord);
            break;
        case CoordType::DMS:
            wgs_pair = CoordHandler::DMS_to_Wgs84P(start_coord);
            break;
        case CoordType::CK42:
            wgs_pair = CoordHandler::CK42_to_Wgs84P(start_coord);
            break;
        default:
            break;
    }

    /**
    @brief Координата не валидна
    */
    if(!wgs_pair.first)
        return res_list;

    //   point_2  - пара координат 2 тестовой точки
    QPair<double, double> point_2 = wgs_pair.second;

    double distance  =  CoordHandler::calc_Distance(point_1, point_2);
    distance = QString::number(distance, 'f', 1).toDouble();
    res_list[1] = true;
    res_list[2] = distance;
    distance /= 1000.0;
    distance = QString::number(distance, 'f', 2).toDouble();
    res_list[3] = distance;
    return res_list;
}

double CoordHandler::distanceInMetersBetweenPoints(const QPair<double, double> &point1, const QPair<double, double> &point2)
{
    return calc_Distance(point1, point2);
}

QVariantList CoordHandler::calculatePolygonArea(const QString &area)
{

    /**
        @brief Расчет площади региона
        @return Формат QVariantList result
                result[0](bool)    -   валидность списка координат
                result[1](bool)    -   площадь в километрахметрах квадратных
                result[2](double)  -   площадь в метрах квадратных
    */


    QVariantList res_list = {false, 0.0, 0.0};

    QString coord_str(area);

    coord_str.remove(' ');

    coord_str = coord_str.trimmed();

    while (coord_str.endsWith('/'))
        coord_str.chop(1);

    QStringList coord_list = coord_str.split('/', Qt::SkipEmptyParts);

    if(coord_list.isEmpty())
        return res_list;


    for (QString &s : coord_list) {
        s.remove(QChar(' '));
        s.remove(QChar('\n'));
    }

    /**
        @brief В defining_type_coord определяем тип координаты, парсинг значения
               координаты производим в функциях преобразования любого типа строковой
               координаты в пару WGS84
    */
    QPair<bool, CoordType> test = CoordHandler::defining_type_coord(coord_list.at(0));

    if(!test.first)
        return res_list;

    QPair<bool, QList<QPair<double, double>>> wgs_pair_list = qMakePair(false, QList<QPair<double, double>>());

     CoordType local_type = test.second;

    switch(local_type) {

        case CoordType::WGS84_String:
            wgs_pair_list = CoordHandler::Wgs84S_to_Wgs84P_List(coord_list);
            break;
        case CoordType::MGRS:
            wgs_pair_list = CoordHandler::MGRS_to_Wgs84P_List(coord_list);
            break;
        case CoordType::UTM:
            wgs_pair_list = CoordHandler::UTM_to_Wgs84P_List(coord_list);
            break;

        case CoordType::DMS:
            wgs_pair_list = CoordHandler::DMS_to_Wgs84P_List(coord_list);
            break;

        case CoordType::CK42:
            wgs_pair_list = CoordHandler::CK42_to_Wgs84P_List(coord_list);
            break;

        default:
            return res_list;
    }

    /**
    @brief Координаты не валидны
    */
    if(!wgs_pair_list.first)
        return res_list;

    res_list[0] = true;

    double _area_m = CoordHandler::calculateAreaWGS84(wgs_pair_list.second);
    double _area_km = _area_m  / 1'000'000;

    res_list[1] = _area_km;
    res_list[2] = _area_m;

    return res_list;
}

QVariantList CoordHandler::calculatePolygonArea(const QList<QPair<double, double>> &points)
{
    QVariantList res_list = {false, 0.0, 0.0};


    double _area_m = CoordHandler::calculateAreaWGS84(points);
    double _area_km = _area_m  / 1'000'000;

    res_list[0] = true;
    res_list[1] = _area_km;
    res_list[2] = _area_m;

    return res_list;
}

QVariantList CoordHandler::calculateRouteLength(const QString &points)
{
    QVariantList res_list = {false,true, 0.0, 0.0};

    QString coord_str(points);

    coord_str = coord_str.trimmed();

    coord_str.remove(' ');

    while (coord_str.endsWith('/'))
        coord_str.chop(1);

    QStringList coord_list = coord_str.split('/', Qt::SkipEmptyParts);

    if(coord_list.isEmpty())
        return res_list;

    for (QString &s : coord_list) {
        s.remove(QChar(' '));
        s.remove(QChar('\n'));
    }

    /**
        @brief В defining_type_coord определяем тип координаты, парсинг значения
               координаты производим в функциях преобразования любого типа строковой
               координаты в пару WGS84
    */
    QPair<bool, CoordType> test = CoordHandler::defining_type_coord(coord_list.at(0));

    if(!test.first)
        return res_list;

    QPair<bool, QList<QPair<double, double>>> wgs_pair_list = qMakePair(false, QList<QPair<double, double>>());

    CoordType local_type = test.second;

    switch(local_type) {

        case CoordType::WGS84_String:
            wgs_pair_list = CoordHandler::Wgs84S_to_Wgs84P_List(coord_list);
            break;
        case CoordType::MGRS:
            wgs_pair_list = CoordHandler::MGRS_to_Wgs84P_List(coord_list);
            break;
        case CoordType::UTM:
            wgs_pair_list = CoordHandler::UTM_to_Wgs84P_List(coord_list);
            break;

        case CoordType::DMS:
            wgs_pair_list = CoordHandler::DMS_to_Wgs84P_List(coord_list);
            break;

        case CoordType::CK42:
            wgs_pair_list = CoordHandler::CK42_to_Wgs84P_List(coord_list);
            break;

        default:
            return res_list;
    }

    /**
    @brief Координата не валидна
    */
    if(!wgs_pair_list.first)
        return res_list;

    res_list[0] = true;

    double route_len =  CoordHandler::_calculateRouteLength(wgs_pair_list.second);

    if(route_len < 0)
        res_list[1] = false;

    else {
        res_list[2] =  route_len;
        res_list[3] =  route_len / 1000.0;
    }

    return res_list;
}

QVariantList CoordHandler::calculateRouteLength(const QList<QPair<double, double>> &points)
{
    QVariantList res_list = {false,true, 0.0, 0.0};

    res_list[0] = true;
    double route_len = CoordHandler::_calculateRouteLength(points);

    if(route_len < 0)
        res_list[1] = false;
    else {
        res_list[2] =  route_len;
        res_list[3] =  route_len / 1000.0;
    }

    return res_list;
}



QPair<bool, QString> CoordHandler::Wgs84P_to_Wgs84S(const QPair<double, double> &pair)
{
    /**
        @brief: Переводим пару координат WGS84 в строку WGS84
        @note: Для обращения к нестатической ф-ции создаем локальный объект  CoordHandler local
    */

    CoordHandler local;
    if(!local.parse_wgs_pair(pair))
        return qMakePair(false, QString("Невірний формат координати WGS84(пара)"));

    return qMakePair(true, QString("WGS84:%1,%2").arg(local.format_coord(pair.first)).arg(local.format_coord(pair.second)));
}

int CoordHandler::countCoordsInText(const QString &text)
{
    QString cleanStr = text;
    cleanStr.remove(' ');
    cleanStr.remove('\n');

    while (cleanStr.endsWith('/'))
        cleanStr.chop(1);

    while (cleanStr.startsWith('/'))
        cleanStr.remove(0, 1);

    if (cleanStr.isEmpty())
        return 0;

    QStringList coordList = cleanStr.split('/', Qt::SkipEmptyParts);

    return coordList.size();
}

QPair<bool, QString> CoordHandler::Wgs84P_to_MGRS(const QPair<double, double> &pair)
{
    CoordHandler local;
    if(!local.parse_wgs_pair(pair))
        return qMakePair(false, QString("Невірний формат координати WGS84(пара)"));

    double latitude =  pair.first;               // Широта
    double longitude = pair.second;             // Долгота
    long precision = 4;                         // Точность
    char mgrs[14];                              // Буфер для результата MGRS

    latitude = qDegreesToRadians(latitude);
    longitude = qDegreesToRadians(longitude);

    // Вызов функции
    long res = Convert_Geodetic_To_MGRS(latitude, longitude, precision, mgrs);

    if(res == MGRS_NO_ERROR) {
        QString mgrs_str = QString::fromLatin1(mgrs);
        mgrs_str.prepend("MGRS:");
        return qMakePair(true, mgrs_str);
    }

    return qMakePair(false, QString());
}

QPair<bool, QString> CoordHandler::Wgs84P_to_UTM(const QPair<double, double> &pair)
{
    CoordHandler local;
    if(!local.parse_wgs_pair(pair))
        return qMakePair(false, QString("Невірний формат координати WGS84(пара)"));

    double latitude =  pair.first;               // Широта
    double longitude = pair.second;             // Долгота
    latitude = qDegreesToRadians(latitude);
    longitude = qDegreesToRadians(longitude);

    long int Zone;
    char Hemisphere[14] = {'\0'};
    double Easting;
    double Northing;

    long res = Convert_Geodetic_To_UTM(latitude, longitude, &Zone, Hemisphere, &Easting, &Northing);

    if(res == UTM_NO_ERROR) {
        QString HemisphereString = QString::fromLatin1(Hemisphere);
        QString ZoneString = QString::number(Zone);
        QString EastingString = QString::number(Easting, 'f', 0);
        QString NorthingString = QString::number(Northing, 'f', 0);
        ZoneString.append(HemisphereString);
        ZoneString.append(EastingString);
        ZoneString.append(',');
        ZoneString.append(NorthingString);
        ZoneString.prepend("UTM:");
        return qMakePair(true, ZoneString);
    }

    return qMakePair(false, QString());
}

QPair<bool, QString> CoordHandler::Wgs84P_to_DMS(const QPair<double, double> &pair)
{
    CoordHandler local;
    if(!local.parse_wgs_pair(pair))
        return qMakePair(false, QString("Невірний формат координати WGS84(пара)"));

    // Лямбда-функция для преобразования координаты в формат DMS
    auto toDMS = [](double coord) -> QString {
        // Определяем знак координаты (для южного и западного полушарий)
        char sign = coord < 0 ? '-' : ' ';

        // Преобразуем координату в положительное значение
        coord = qFabs(coord);

        // Извлекаем целые градусы из координаты
        int degrees = static_cast<int>(coord);

        // Преобразуем оставшуюся часть координаты в минуты
        coord = (coord - degrees) * 60;
        int minutes = static_cast<int>(coord);

        // Преобразуем оставшуюся часть координаты в секунды
        double seconds = (coord - minutes) * 60;

        // Формируем строку в формате "градусы° минуты' секунды\""
        return QString("%1%2°%3'%4\"").arg(sign).arg(degrees).arg(minutes).arg(seconds, 0, 'f', 3);
    };

    // Преобразование широты в формат DMS с указанием направления (N или S)
    QString latDMS = toDMS(pair.first) + (pair.first < 0 ? "S" : "N");

    // Преобразование долготы в формат DMS с указанием направления (W или E)
    QString lonDMS = toDMS(pair.second) + (pair.second < 0 ? "W" : "E");

    // Возвращаем строку с координатами в формате DMS
    QString coord =  "DMS:" + latDMS + "," + lonDMS;

    return qMakePair(true, coord);
}

QPair<bool, QString> CoordHandler::Wgs84P_to_CK42(const QPair<double, double> &pair)
{
    CoordHandler local;
    if(!local.parse_wgs_pair(pair))
        return qMakePair(false, QString("Невірний формат координати WGS84(пара)"));

    SK42Rectangle __struct = local.wgs_pair_to_sk42_rectangle(pair);
    QString res = local.convert_ck42pair_to_ck42str(qMakePair(__struct.x, __struct.y));
    return qMakePair(true, res);
}

QPair<bool, QPair<double, double> > CoordHandler::Wgs84S_to_Wgs84P(const QString &str)
{
    /**
        @brief: Переводим строку WGS84 в пару координат WGS84
        @note: Для обращения к нестатической ф-ции создаем локальный объект  CoordHandler local
    */

    QString local_str = str.trimmed();
    local_str.remove(' ');

    CoordHandler local;
    QPair<double, double> res = qMakePair(0.0, 0.0);    
    QRegularExpression re("WGS84:([-+]?\\d*\\.?\\d+)\\s*,\\s*([-+]?\\d*\\.?\\d+)");
    QRegularExpressionMatch _match = re.match(local_str);

    if (_match.hasMatch()) {
        double lat = _match.captured(1).toDouble();
        double lon = _match.captured(2).toDouble();
        if (local.parse_wgs_pair( qMakePair(lat, lon))) {
            res.first = lat;
            res.second = lon;
            return qMakePair(true, res);
        }
    }

    return qMakePair(false, res);
}



QPair<bool, QPair<double, double>> CoordHandler::MGRS_to_Wgs84P(const QString &str)
{
    double latitude, longitude;
    QPair<double, double> res = qMakePair(0.0, 0.0);   

    QString mgrs = str.mid(5);
    QByteArray mgrs_bytes = mgrs.toLatin1();
    char* mgrs_buff = mgrs_bytes.data();

    long error_param = Convert_MGRS_To_Geodetic(mgrs_buff, &latitude, &longitude);

    if(error_param == MGRS_NO_ERROR || error_param == MGRS_LAT_WARNING) {
        res.first = qRadiansToDegrees(latitude);
        res.second = qRadiansToDegrees(longitude);
        return qMakePair(true, res);
    }

    return qMakePair(false, res);
}

QPair<bool, QPair<double, double> > CoordHandler::UTM_to_Wgs84P(const QString &str)
{
    QString utmString(str);

    while (utmString.endsWith('/'))
        utmString.chop(1);

    int Zone = 0;
    char Hemisphere = ' ';
    double Easting = 0.0;
    double Northing = 0.0;

    QPair<double, double> res = qMakePair(0.0, 0.0);

    int colonPos = utmString.indexOf(':');

    if(colonPos == -1)  return qMakePair(false, res);

    int firstLetterPos = colonPos + 1;
    while (firstLetterPos < utmString.length() && utmString[firstLetterPos].isDigit())
        ++firstLetterPos;


    if (firstLetterPos >= utmString.length()) return qMakePair(false, res);

    Hemisphere = utmString[firstLetterPos].toLatin1();

    if (Hemisphere != 'N' && Hemisphere != 'S') return qMakePair(false, res);

    QString zoneString = utmString.mid(colonPos + 1, firstLetterPos - colonPos - 1);
    Zone = zoneString.toInt();

    int commaPos = utmString.indexOf(',', firstLetterPos);
    if (commaPos == -1) return qMakePair(false, res);

    QString eastingString = utmString.mid(firstLetterPos + 1, commaPos - firstLetterPos - 1);
    Easting = eastingString.toDouble();

    QString northingString = utmString.mid(commaPos + 1);
    Northing = northingString.toDouble();

    double latitude, longitude;

    long error_param = Convert_UTM_To_Geodetic(Zone, Hemisphere, Easting, Northing, &latitude, &longitude);

    if(error_param == UTM_NO_ERROR) {
        res.first = qRadiansToDegrees(latitude);
        res.second = qRadiansToDegrees(longitude);
        return qMakePair(true, res);
    }

    return qMakePair(false, res);
}

QPair<bool, QPair<double, double> > CoordHandler::DMS_to_Wgs84P(const QString &str)
{
    QPair<double, double> res = qMakePair(0.0, 0.0);
    CoordHandler local;

    // Проверяем префикс DMS:
    if (!str.startsWith("DMS:")) return qMakePair(false, res);    

    QString _cleanStr = str;
    while (_cleanStr.endsWith('/'))
        _cleanStr.chop(1);

    // Удаляем префикс DMS: и разделяем на широту и долготу
    QString coords_str = _cleanStr.mid(4);
    QStringList coords = coords_str.split(',');

    if (coords.size() != 2) return qMakePair(false, res);

    // Парсим широту и долготу
    DMSComponents lat = local.parse_dms_str(coords[0].trimmed());
    DMSComponents lon = local.parse_dms_str(coords[1].trimmed());

    if (!lat.isValid || !lon.isValid) return qMakePair(false, res);

    // Проверяем корректность значений
    if (!local.is_valid_lat_dms(lat) || !local.is_valid_long_dms(lon)) return qMakePair(false, res);

    // Конвертируем в десятичные градусы
    double latitude = local.dms_to_decimal(lat);
    double longitude = local.dms_to_decimal(lon);

    res.first = latitude;
    res.second = longitude;

    return qMakePair(true, res);
}

QPair<bool, QPair<double, double> > CoordHandler::CK42_to_Wgs84P(const QString &str)
{
    CoordHandler local;

    QString _cleanStr = str;
    while (_cleanStr.endsWith('/'))
        _cleanStr.chop(1);

    QPair<double, double> ck_res = local.convert_ck42str_to_ck42pair(_cleanStr);

    if(ck_res.first == -1000.0) return qMakePair(false, ck_res);

    int zone = 6;
    QPair<double, double> res = local.sk42_rectangle_to_wgs_pair(ck_res.first, ck_res.second, zone);

    return qMakePair(true, res);
}

QPair<bool, QList<QPair<double, double>>> CoordHandler::Wgs84S_to_Wgs84P_List(const QStringList &inner_list)
{
    QList<QPair<double, double>> pair_list;
    pair_list.clear();

    QPair<bool, QPair<double, double>> local_pair;

    for(const QString &str : inner_list) {

        local_pair = Wgs84S_to_Wgs84P(str);

        if(!local_pair.first)  return qMakePair(false, QList<QPair<double, double>>());

        pair_list.append(local_pair.second);
    }

    return qMakePair(true, pair_list);
}

QPair<bool, QList<QPair<double, double> > > CoordHandler::MGRS_to_Wgs84P_List(const QStringList &inner_list)
{
    QList<QPair<double, double>> pair_list;
    pair_list.clear();

    QPair<bool, QPair<double, double>> local_pair;

    for(const QString &str : inner_list) {

        local_pair = MGRS_to_Wgs84P(str);

        if(!local_pair.first)  return qMakePair(false, QList<QPair<double, double>>());

        pair_list.append(local_pair.second);
    }

    return qMakePair(true, pair_list);
}

QPair<bool, QList<QPair<double, double> > > CoordHandler::UTM_to_Wgs84P_List(const QStringList &inner_list)
{
    QList<QPair<double, double>> pair_list;
    pair_list.clear();

    QPair<bool, QPair<double, double>> local_pair;

    for(const QString &str : inner_list) {

        local_pair = UTM_to_Wgs84P(str);

        if(!local_pair.first)  return qMakePair(false, QList<QPair<double, double>>());

        pair_list.append(local_pair.second);
    }

    return qMakePair(true, pair_list);
}

QPair<bool, QList<QPair<double, double> > > CoordHandler::DMS_to_Wgs84P_List(const QStringList &inner_list)
{
    QList<QPair<double, double>> pair_list;
    pair_list.clear();

    QPair<bool, QPair<double, double>> local_pair;

    for(const QString &str : inner_list) {

        local_pair = DMS_to_Wgs84P(str);

        if(!local_pair.first)  return qMakePair(false, QList<QPair<double, double>>());

        pair_list.append(local_pair.second);
    }

    return qMakePair(true, pair_list);
}

QPair<bool, QList<QPair<double, double> > > CoordHandler::CK42_to_Wgs84P_List(const QStringList &inner_list)
{
    QList<QPair<double, double>> pair_list;
    pair_list.clear();

    QPair<bool, QPair<double, double>> local_pair;

    for(const QString &str : inner_list) {

        local_pair = CK42_to_Wgs84P(str);

        if(!local_pair.first)  return qMakePair(false, QList<QPair<double, double>>());

        pair_list.append(local_pair.second);
    }

    return qMakePair(true, pair_list);
}

QPair<bool, QString> CoordHandler::Wgs84P_to_Wgs84S_List(const QList<QPair<double, double>> &inner_list)
{
    QPair<bool, QString> res_pair = qMakePair(false, QString()),
                         local_pair;

    QString coord;

    for(const auto &pair : inner_list) {
        local_pair =  Wgs84P_to_Wgs84S(pair);
        if(!local_pair.first) return res_pair;
        coord += local_pair.second + "/";
    }

    coord += "/";

    return qMakePair(true, coord);
}

QPair<bool, QString> CoordHandler::Wgs84P_to_MGRS_List(const QList<QPair<double, double>> &inner_list)
{
    QPair<bool, QString> res_pair = qMakePair(false, QString()),
        local_pair;

    QString coord;

    for(const auto &pair : inner_list) {
        local_pair =  Wgs84P_to_MGRS(pair);
        if(!local_pair.first) return res_pair;
        coord += local_pair.second + "/";
    }

    coord += "/";

    return qMakePair(true, coord);
}

QPair<bool, QString> CoordHandler::Wgs84P_to_UTM_List(const QList<QPair<double, double>> &inner_list)
{
    QPair<bool, QString> res_pair = qMakePair(false, QString()),
        local_pair;

    QString coord;

    for(const auto &pair : inner_list) {
        local_pair =  Wgs84P_to_UTM(pair);
        if(!local_pair.first) return res_pair;
        coord += local_pair.second + "/";
    }

    coord += "/";

    return qMakePair(true, coord);
}

QPair<bool, QString> CoordHandler::Wgs84P_to_DMS_List(const QList<QPair<double, double>> &inner_list)
{
    QPair<bool, QString> res_pair = qMakePair(false, QString()),
        local_pair;

    QString coord;

    for(const auto &pair : inner_list) {
        local_pair =  Wgs84P_to_DMS(pair);
        if(!local_pair.first) return res_pair;
        coord += local_pair.second + "/";
    }

    coord += "/";

    return qMakePair(true, coord);
}

QPair<bool, QString> CoordHandler::Wgs84P_to_CK42S_List(const QList<QPair<double, double>> &inner_list)
{
    QPair<bool, QString> res_pair = qMakePair(false, QString()),
        local_pair;

    QString coord;

    for(const auto &pair : inner_list) {
        local_pair =  Wgs84P_to_CK42(pair);
        if(!local_pair.first) return res_pair;
        coord += local_pair.second + "/";
    }

    coord += "/";

    return qMakePair(true, coord);
}

bool CoordHandler::check_Point_In_Area(const QList<QPair<double, double> > &area_list, const QPair<double, double> &point)
{
    return (is_point_on_boundary(area_list, point) || is_point_in_area(area_list, point));
}

double CoordHandler::calc_Distance(const QPair<double, double> &point1, const QPair<double, double> &point2)
{
    // WGS84 ellipsoid parameters
    const double a = 6378137.0;        // semi-major axis in meters
    const double f = 1.0 / 298.257223563; // flattening
    const double b = a * (1.0 - f);    // semi-minor axis
    const double maxIterations = 100;
    const double epsilon = 1e-12;       // convergence threshold

    // Convert coordinates from degrees to radians
    double phi1 = point1.first * M_PI / 180.0;    // latitude 1
    double lambda1 = point1.second * M_PI / 180.0; // longitude 1
    double phi2 = point2.first * M_PI / 180.0;    // latitude 2
    double lambda2 = point2.second * M_PI / 180.0; // longitude 2

    double L = lambda2 - lambda1; // difference in longitude
    double tanU1 = (1.0 - f) * qTan(phi1);
    double cosU1 = 1.0 / qSqrt(1.0 + tanU1 * tanU1);
    double sinU1 = tanU1 * cosU1;

    double tanU2 = (1.0 - f) * qTan(phi2);
    double cosU2 = 1.0 / qSqrt(1.0 + tanU2 * tanU2);
    double sinU2 = tanU2 * cosU2;

    double lambda = L;
    double lambdaP;
    double sinLambda;
    double cosLambda;
    double sigma;
    double sinSigma;
    double cosSigma;
    double sinAlpha;
    double cos2Alpha;
    double cos2SigmaM;
    double C;

    int iterations = 0;
    do {
        sinLambda = qSin(lambda);
        cosLambda = qCos(lambda);

        sinSigma = qSqrt(qPow(cosU2 * sinLambda, 2) +
                         qPow(cosU1 * sinU2 - sinU1 * cosU2 * cosLambda, 2));

        if (sinSigma == 0) {
            return 0.0; // Points are coincident
        }

        cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
        sigma = qAtan2(sinSigma, cosSigma);

        sinAlpha = cosU1 * cosU2 * sinLambda / sinSigma;
        cos2Alpha = 1.0 - sinAlpha * sinAlpha;

        cos2SigmaM = cosSigma - 2.0 * sinU1 * sinU2 / cos2Alpha;

        if (!qIsFinite(cos2SigmaM)) {
            cos2SigmaM = 0.0; // Equatorial line
        }

        C = f / 16.0 * cos2Alpha * (4.0 + f * (4.0 - 3.0 * cos2Alpha));
        lambdaP = lambda;
        lambda = L + (1.0 - C) * f * sinAlpha *
                         (sigma + C * sinSigma * (cos2SigmaM + C * cosSigma *
                                                                   (-1.0 + 2.0 * qPow(cos2SigmaM, 2))));

    } while (qAbs(lambda - lambdaP) > epsilon && ++iterations < maxIterations);

    if (iterations >= maxIterations) {
        return -1.0; // Formula failed to converge
    }

    double u2 = cos2Alpha * (qPow(a, 2) - qPow(b, 2)) / qPow(b, 2);
    double A = 1.0 + u2 / 16384.0 * (4096.0 + u2 *
                                                  (-768.0 + u2 * (320.0 - 175.0 * u2)));
    double B = u2 / 1024.0 * (256.0 + u2 *
                                          (-128.0 + u2 * (74.0 - 47.0 * u2)));

    double deltaSigma = B * sinSigma * (cos2SigmaM + B / 4.0 *
                                                         (cosSigma * (-1.0 + 2.0 * qPow(cos2SigmaM, 2)) -
                                                          B / 6.0 * cos2SigmaM * (-3.0 + 4.0 * qPow(sinSigma, 2)) *
                                                              (-3.0 + 4.0 * qPow(cos2SigmaM, 2))));

    double distance = b * A * (sigma - deltaSigma);

    return distance; // Returns distance in meters

}

double CoordHandler::calculateAreaWGS84(const QList<QPair<double, double> > &coord_list)
{
    // Константы для эллипсоида WGS-84
    const double WGS84_A = 6378137.0;         // большая полуось в метрах
    const double WGS84_F = 1.0 / 298.257223563; // сжатие

    if (coord_list.size() < 3) {
        return  -1.0;
    }

    // Константы для расчётов
    const double b = WGS84_A * (1.0 - WGS84_F); // малая полуось
    const double e2 = 2 * WGS84_F - WGS84_F * WGS84_F; // квадрат эксцентриситета

    int n = coord_list.size();
    double area = 0.0;

    // Создаём замкнутый многоугольник, если он не замкнут
    QList<QPair<double, double>> coords = coord_list;
    if (!(qFuzzyCompare(coords.first().first, coords.last().first) && qFuzzyCompare(coords.first().second, coords.last().second))) {
        coords.append(coords.first());
        n++;
    }

    // Используем модифицированную формулу Гаусса для площади на сфероиде
    for (int i = 0; i < n - 1; i++) {
        double lat1 = coords[i].first * M_PI / 180.0;    // широта в радианах
        double lon1 = coords[i].second * M_PI / 180.0;   // долгота в радианах
        double lat2 = coords[i+1].first * M_PI / 180.0;  // широта следующей точки
        double lon2 = coords[i+1].second * M_PI / 180.0; // долгота следующей точки

        // Учитываем переход через 180-й меридиан
        double dlon = lon2 - lon1;
        if (dlon > M_PI) dlon -= 2 * M_PI;
        if (dlon < -M_PI) dlon += 2 * M_PI;

        // Вычисляем площадь сегмента
        double q1 = sin(lat1) / (1.0 - e2 * sin(lat1) * sin(lat1));
        double q2 = sin(lat2) / (1.0 - e2 * sin(lat2) * sin(lat2));

        area += dlon * (q1 + q2) / 2.0;
    }

    // Умножаем на квадрат малой полуоси
    area = fabs(area) * b * b;

    area = round(area * 100) / 100.0;
    return area; // площадь в квадратных километрах
}

double CoordHandler::_calculateRouteLength(const QList<QPair<double, double> > &coord_list)
{
    if (coord_list.size() < 2) {
        return -1.0;
    }

    double totalDistance = 0.0;

    // Проходим по всем точкам маршрута и суммируем расстояния между соседними точками
    for (int i = 0; i < coord_list.size() - 1; i++) {
        const QPair<double, double>& point1 = coord_list[i];
        const QPair<double, double>& point2 = coord_list[i + 1];

        // Используем вашу функцию для расчёта расстояния
        double segmentDistance = CoordHandler::calc_Distance(point1, point2);
        totalDistance += segmentDistance;
    }

    // Округляем до 2 знаков после запятой
    totalDistance = round(totalDistance * 100) / 100.0;

    return totalDistance; // Длина маршрута в метрах
}



QString CoordHandler::format_coord(double value)
{
    /**
        @brief: Удаляем завершающие нули и точку, если она осталась последней
    */
    QString str = QString::number(value, 'f', 7);
    str.remove(QRegularExpression("0+$"));
    if (str.endsWith('.')) str.chop(1);
    return str;
}

bool CoordHandler::parse_wgs_pair(const QPair<double, double> &pair)
{
    /**
        @brief: Проверяем допустимые значения пары координат WGS84
    */
    return pair.first >= -90.0 && pair.first <= 90.0 && pair.second >= -180.0 && pair.second <= 180.0;
}

bool CoordHandler::parse_wgs_str(const QString &str)
{
    /**
        @brief: Проверяем валидность WGS84 строки
    */
    QString cleanStr = str.trimmed();
    QRegularExpression wgs84Regex(R"(^WGS84:\s*-?\d+\.\d+\s*,\s*-?\d+\.\d+$)");
    return wgs84Regex.match(cleanStr).hasMatch();
}

CoordHandler::DMSComponents CoordHandler::parse_dms_str(const QString &component)
{
    DMSComponents result;

    /**
        @brief:  Получаем направление (крайний символ справа)
    */
    result.direction = component.right(1);
    if (result.direction != "N" && result.direction != "S" &&
        result.direction != "E" && result.direction != "W") {
        return result;
    }

    // Удаляем направление и разбиваем строку на компоненты
    QString str = component.left(component.length() - 1);

    // Находим позиции символов ° ' "
    int degPos = str.indexOf("°");
    int minPos = str.indexOf("\'");
    int secPos = str.indexOf("\"");

    if (degPos == -1 || minPos == -1 || secPos == -1) {
        return result;
    }

    bool ok = false;

    // Извлекаем градусы
    QString degStr = str.left(degPos);
    result.degrees = degStr.toInt(&ok);
    if (!ok) return result;

    // Извлекаем минуты
    QString minStr = str.mid(degPos + 1, minPos - degPos - 1);
    result.minutes = minStr.toInt(&ok);
    if (!ok) return result;

    // Извлекаем секунды
    QString secStr = str.mid(minPos + 1, secPos - minPos - 1);
    result.seconds = secStr.toDouble(&ok);
    if (!ok) return result;

    result.isValid = true;
    return result;

}

bool CoordHandler::is_valid_lat_dms(const DMSComponents &lat)
{
    if (lat.direction != "N" && lat.direction != "S") return false;
    if (lat.degrees < 0 || lat.degrees > 90) return false;
    if (lat.minutes < 0 || lat.minutes >= 60) return false;
    if (lat.seconds < 0 || lat.seconds >= 60) return false;
    if (lat.degrees == 90 && (lat.minutes > 0 || lat.seconds > 0)) return false;
    return true;
}

bool CoordHandler::is_valid_long_dms(const DMSComponents &lon)
{
    if (lon.direction != "E" && lon.direction != "W") return false;
    if (lon.degrees < 0 || lon.degrees > 180) return false;
    if (lon.minutes < 0 || lon.minutes >= 60) return false;
    if (lon.seconds < 0 || lon.seconds >= 60) return false;
    if (lon.degrees == 180 && (lon.minutes > 0 || lon.seconds > 0)) return false;
    return true;
}

double CoordHandler::dms_to_decimal(const DMSComponents &dms)
{
    double decimal = dms.degrees + dms.minutes/60.0 + dms.seconds/3600.0;
    if (dms.direction == "S" || dms.direction == "W")
        decimal = -decimal;
    return decimal;
}

CoordHandler::DMSComponents CoordHandler::decimal_to_dms(double decimal)
{
    DMSComponents result;

    result.degrees = static_cast<int>(floor(decimal));  // Получаем целые градусы

    double minutesDouble = (decimal - result.degrees) * 60.0;    // Вычисляем минуты
    result.minutes = static_cast<int>(floor(minutesDouble));

    result.seconds = (minutesDouble - result.minutes) * 60.0;    // Вычисляем секунды

    // Округляем секунды до 3 знаков после запятой для предотвращения ошибок округления
    result.seconds = round(result.seconds * 1000) / 1000.0;

    // Корректируем значения, если секунды округлились до 60
    if (result.seconds >= 60.0) {
        result.seconds = 0.0;
        result.minutes++;
        if (result.minutes >= 60) {
            result.minutes = 0;
            result.degrees++;
        }
    }

    result.isValid = true;
    return result;
}

QPair<double, double> CoordHandler::convert_ck42str_to_ck42pair(const QString &str)
{
    QPair<double, double> result(-1000.0, -1000.0);

    // Проверяем, начинается ли строка с "CK42:"
    bool flag = (str.startsWith("CK42:")) || (str.startsWith("СК42:"));
    if (!flag) return result;

    // Убираем префикс "CK42:" и разбираем оставшуюся часть
    QString coord_part = str.mid(5); // Пропускаем первые 5 символов ("CK42:")

    // Находим позиции X и Y в строке
    int x_pos = coord_part.indexOf('X');
    int y_pos = coord_part.indexOf('Y');

    if (x_pos == -1 || y_pos == -1) {
        return result;
    }

    // Извлекаем значения X и Y
    QString xStr = coord_part.mid(x_pos + 1, y_pos - (x_pos + 1));
    QString yStr = coord_part.mid(y_pos + 1);

    // Преобразуем строки в числа
    bool x_ok = false, y_ok = false;
    double x = xStr.toDouble(&x_ok);
    double y = yStr.toDouble(&y_ok);

    if (x_ok && y_ok) {
        result.first = x;
        result.second = y;
    }

    return result;
}

QPair<double, double> CoordHandler::ck42pair_to_wgs_pair(const QPair<double, double> &sk42Coords)
{
    // SK-42 ellipsoid parameters (Krasovsky 1940)
    const double aP = 6378245.0;  // semi-major axis
    const double fP = 1.0 / 298.3;  // flattening
    // const double bP = aP * (1.0 - fP);  // semi-minor axis
    const double e2P = 2.0 * fP - fP * fP;  // first eccentricity squared

    // WGS84 ellipsoid parameters
    const double a = 6378137.0;  // semi-major axis
    const double f = 1.0 / 298.257223563;  // flattening
    // const double b = a * (1.0 - f);  // semi-minor axis
    const double e2 = 2.0 * f - f * f;  // first eccentricity squared

    // Transformation parameters (SK42 -> WGS84) - inverse of WGS84 -> SK42
    const double dx = -23.92;
    const double dy = 141.27;
    const double dz = 80.9;
    const double wx = 0.0;
    const double wy = 0.0;
    const double wz = 0.0;
    const double ms = 0.0;

    double B = sk42Coords.first * M_PI / 180.0;
    double L = sk42Coords.second * M_PI / 180.0;

    // Calculate N - radius of curvature in the prime vertical
    double N = aP / qSqrt(1.0 - e2P * qSin(B) * qSin(B));

    // Calculate Cartesian coordinates for SK42
    double X = N * qCos(B) * qCos(L);
    double Y = N * qCos(B) * qSin(L);
    double Z = N * (1.0 - e2P) * qSin(B);

    // Transform coordinates
    double XW = dx + (1.0 + ms) * X + wz * Y - wy * Z;
    double YW = dy + (-wz) * X + (1.0 + ms) * Y + wx * Z;
    double ZW = dz + wy * X - wx * Y + (1.0 + ms) * Z;

    // Convert back to geodetic coordinates using iterative method
    double BW = qAtan2(ZW, qSqrt(XW * XW + YW * YW));
    double LW = qAtan2(YW, XW);

    for (int i = 0; i < 5; i++) {
        double N = a / qSqrt(1.0 - e2 * qSin(BW) * qSin(BW));
        // double h = qSqrt(XW * XW + YW * YW) / qCos(BW) - N;
        BW = qAtan2(ZW + e2 * N * qSin(BW), qSqrt(XW * XW + YW * YW));
    }

    // Convert radians back to degrees
    BW = BW * 180.0 / M_PI;
    LW = LW * 180.0 / M_PI;

    return QPair<double, double>(BW, LW);
}

QPair<double, double> CoordHandler::sk42_rectangle_to_wgs_pair(double x, double y, int zone)
{
    // Krasovsky 1940 ellipsoid parameters
    const double a = 6378245.0;  // semi-major axis
    const double e2 = 0.00669342162296594323; // first eccentricity squared

    // Remove false easting and zone number
    y -= (zone * 1000000.0 + 500000.0);

    // First approximation
    double B = x / 6367558.4968;
    double L0 = (zone * 6 - 3) * M_PI / 180.0;  // central meridian

    // Iterative solution for better precision
    for (int i = 0; i < 4; i++) {
        double sinB = qSin(B);
        double cosB = qCos(B);
        // double eta2 = e2 * qPow(cosB, 2) / (1.0 - e2);
        double N = a / qSqrt(1.0 - e2 * qPow(sinB, 2));

        // Calculate l (difference from central meridian)
        double l = y / (N * cosB);
        double l2 = qPow(l, 2);
        double l4 = qPow(l, 4);

        // Correction for latitude
        B = x / 6367558.4968 +
            qSin(2.0 * B) * (0.00252588685 - 0.00001491860 * qPow(sinB, 2) +
                             l2 * (0.00000511618 - 0.00000000435 * qPow(sinB, 2)) +
                             l4 * (0.00000000560 + 0.00000000010 * qPow(sinB, 2)));
    }

    // Calculate final longitude
    double sinB = qSin(B);
    double cosB = qCos(B);
    double eta2 = e2 * qPow(cosB, 2) / (1.0 - e2);
    double N = a / qSqrt(1.0 - e2 * qPow(sinB, 2));

    double l = y / (N * cosB);
    double l3 = qPow(l, 3);
    double l5 = qPow(l, 5);

    double L = L0 + l - (l3 / 3.0) * (1.0 + eta2) +
               (l5 / 15.0) * (2.0 + 5.0 * eta2 + 3.0 * qPow(eta2, 2));

    // Convert to degrees
    B = B * 180.0 / M_PI;
    L = L * 180.0 / M_PI;

    // Convert from SK42 to WGS84
    return ck42pair_to_wgs_pair(QPair<double, double>(B, L));
}

CoordHandler::SK42Rectangle CoordHandler::wgs_pair_to_sk42_rectangle(const QPair<double, double> &wgs84Coords)
{
    // First convert to SK42 geographical coordinates
    QPair<double, double> sk42Coords = wgs_pair_to_ck42pair(wgs84Coords);

    double B = sk42Coords.first * M_PI / 180.0;  // latitude in radians
    double L = sk42Coords.second * M_PI / 180.0; // longitude in radians

    // Krasovsky 1940 ellipsoid parameters
    const double a = 6378245.0;  // semi-major axis
    const double e2 = 0.00669342162296594323; // first eccentricity squared
    // const double n = qSqrt(1.0 - e2 * qPow(qSin(B), 2)); // auxiliary value

    // Calculate zone number
    int zone = static_cast<int>((sk42Coords.second + 6.0) / 6.0);

    // Calculate central meridian longitude for the zone
    double L0 = (zone * 6 - 3) * M_PI / 180.0;

    // Calculate l - difference between point longitude and central meridian
    double l = L - L0;

    // Calculate auxiliary values
    double sinB = qSin(B);
    double cosB = qCos(B);
    double eta2 = e2 * qPow(cosB, 2) / (1.0 - e2);

    // Calculate Gauss-Krüger projection coordinates
    double N = a / qSqrt(1.0 - e2 * qPow(sinB, 2));

    double x = 6367558.4968 * B - qSin(2*B) * (16002.89 + 66.9607 * qPow(sinB, 2) +
                                                 0.3515 * qPow(sinB, 4) - qPow(l, 2) * (1594561.25 + 5336.535 * qPow(sinB, 2) +
                                                                                        26.790 * qPow(sinB, 4) + 0.149 * qPow(sinB, 6) + qPow(l, 2) * (672483.4 -
                                                                                                                                                       811219.9 * qPow(sinB, 2) + 5420.0 * qPow(sinB, 4) - 10.6 * qPow(sinB, 6) +
                                                                                                                                                       qPow(l, 2) * (278194.0 - 830174.0 * qPow(sinB, 2) + 572434.0 * qPow(sinB, 4) -
                                                                                                                                                                     16010.0 * qPow(sinB, 6) + qPow(l, 2) * (109500.0 - 574700.0 * qPow(sinB, 2) +
                                                                                                                                                                                                             863700.0 * qPow(sinB, 4) - 398600.0 * qPow(sinB, 6))))));

    double y = (l * cosB * N * (1.0 + qPow(l, 2) / 6.0 * qPow(cosB, 2) *
                                          (1.0 + eta2 + qPow(l, 2) / 20.0 * qPow(cosB, 2) *
                                                            (5.0 - 18.0 * eta2 + qPow(eta2, 2) + 14.0 * qPow(l, 2) -
                                                             58.0 * eta2 * qPow(l, 2)))));

    // Add zone number to Y coordinate (false easting)
    y += zone * 1000000.0 + 500000.0;

    return {x, y, zone};
}

QPair<double, double> CoordHandler::wgs_pair_to_ck42pair(const QPair<double, double> &wgs84Coords)
{
    // WGS84 ellipsoid parameters
    const double a = 6378137.0;  // semi-major axis
    const double f = 1.0 / 298.257223563;  // flattening
    // const double b = a * (1.0 - f);  // semi-minor axis
    const double e2 = 2.0 * f - f * f;  // first eccentricity squared

    // SK-42 ellipsoid parameters (Krasovsky 1940)
    const double aP = 6378245.0;  // semi-major axis
    const double fP = 1.0 / 298.3;  // flattening
    // const double bP = aP * (1.0 - fP);  // semi-minor axis
    const double e2P = 2.0 * fP - fP * fP;  // first eccentricity squared

    // Transformation parameters (WGS84 -> SK42)
    const double dx = 23.92;
    const double dy = -141.27;
    const double dz = -80.9;
    const double wx = 0.0;
    const double wy = 0.0;
    const double wz = 0.0;
    const double ms = 0.0;

    // Convert degrees to radians
    double B = wgs84Coords.first * M_PI / 180.0;
    double L = wgs84Coords.second * M_PI / 180.0;

    // Calculate N - radius of curvature in the prime vertical
    double N = a / qSqrt(1.0 - e2 * qSin(B) * qSin(B));

    // Calculate Cartesian coordinates for WGS84
    double X = N * qCos(B) * qCos(L);
    double Y = N * qCos(B) * qSin(L);
    double Z = N * (1.0 - e2) * qSin(B);

    // Transform coordinates
    double XP = dx + (1.0 + ms) * X + wz * Y - wy * Z;
    double YP = dy + (-wz) * X + (1.0 + ms) * Y + wx * Z;
    double ZP = dz + wy * X - wx * Y + (1.0 + ms) * Z;

    // Convert back to geodetic coordinates using iterative method
    double BP = qAtan2(ZP, qSqrt(XP * XP + YP * YP));
    double LP = qAtan2(YP, XP);

    for (int i = 0; i < 5; i++) {
        double NP = aP / qSqrt(1.0 - e2P * qSin(BP) * qSin(BP));
        // double h = qSqrt(XP * XP + YP * YP) / qCos(BP) - NP;
        BP = qAtan2(ZP + e2P * NP * qSin(BP), qSqrt(XP * XP + YP * YP));
    }

    // Convert radians back to degrees
    BP = BP * 180.0 / M_PI;
    LP = LP * 180.0 / M_PI;

    return QPair<double, double>(BP, LP);
}

QString CoordHandler::convert_ck42pair_to_ck42str(const QPair<double, double> &pair)
{
    // Округляем до целых чисел, так как в формате строки используются целые значения
    qint64 x = qRound(pair.first);
    qint64 y = qRound(pair.second);

    // Формируем строку в формате CK42:X{value}Y{value}
    return QString("CK42:X%1Y%2").arg(x).arg(y);
}

bool CoordHandler::is_point_on_boundary(const QList<QPair<double, double> > &area_list, const QPair<double, double> &point, double epsilon)
{
    for (int i = 0; i < area_list.size(); i++) {
        int next = (i + 1) % area_list.size();

        // Координаты текущего отрезка
        double x1 = area_list[i].first;
        double y1 = area_list[i].second;
        double x2 = area_list[next].first;
        double y2 = area_list[next].second;

        // Координаты проверяемой точки
        double px = point.first;
        double py = point.second;

        // Проверяем, лежит ли точка на отрезке
        double A = (y2 - y1);
        double B = -(x2 - x1);
        double C = y1 * (x2 - x1) - x1 * (y2 - y1);

        // Расстояние от точки до прямой
        double distance = qAbs(A * px + B * py + C) / qSqrt(A * A + B * B);

        if (distance < epsilon) {
            // Проверяем, что точка лежит между концами отрезка
            if (px >= qMin(x1, x2) - epsilon &&
                px <= qMax(x1, x2) + epsilon &&
                py >= qMin(y1, y2) - epsilon &&
                py <= qMax(y1, y2) + epsilon) {
                return true;
            }
        }
    }
    return false;
}

bool CoordHandler::is_point_in_area(const QList<QPair<double, double> > &area_list, const QPair<double, double> &point)
{
    if (area_list.size() < 3) {
        return false; // Многоугольник должен иметь минимум 3 точки
    }

    bool inside = false;
    int i, j;

    // Координаты проверяемой точки
    double x = point.first;
    double y = point.second;

    // Проходим по всем сторонам многоугольника
    for (i = 0, j = area_list.size() - 1; i < area_list.size(); j = i++) {
        double xi = area_list[i].first;
        double yi = area_list[i].second;
        double xj = area_list[j].first;
        double yj = area_list[j].second;

        // Проверяем пересечение луча с ребром многоугольника
        if (((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }

    return inside;
}

QPair<bool, CoordType> CoordHandler::defining_type_coord(const QString &str)
{   
    /**
        @brief Очищаем строку от лишних пробелов в начале, конце и середине строки, создаем регулярные выражения для каждого формата
        @return QPair<bool, CoordType>
    */

    QString clean_str(str);

    clean_str = clean_str.trimmed();
    clean_str.remove(' ');
    while (clean_str.endsWith('/'))
        clean_str.chop(1);

    /**
        @brief Проверяем корректность строкового представления координаты
    */

    static const QRegularExpression wgs84Regex(R"(^WGS84:\s*-?\d+\.\d+\s*,\s*-?\d+\.\d+\s*/{0,2}$)");
    static const QRegularExpression mgrsRegex(R"(^MGRS:\s*\d{1,2}[C-X][A-Z][A-Z]\d{8,10}\s*/{0,2}$)");
    static const QRegularExpression utmRegex(R"(^UTM:\s*\d{1,2}[NS]\d+\s*,\s*\d+\s*/{0,2}$)");
    static const QRegularExpression dmsRegex(R"(^DMS:\s*\d{1,3}°\d{1,2}'\d{1,2}(\.\d+)?\"[NS]\s*,\s*\d{1,3}°\d{1,2}'\d{1,2}(\.\d+)?\"[EW]\s*/{0,2}$)");
    static const QRegularExpression ck42Regex(R"(^CK42:\s*X\d+Y\d+\s*/{0,2}$)");

    if (wgs84Regex.match(clean_str).hasMatch()) return qMakePair(true, CoordType::WGS84_String);
    if (mgrsRegex.match(clean_str).hasMatch())  return qMakePair(true, CoordType::MGRS);
    if (utmRegex.match(clean_str).hasMatch())  return qMakePair(true, CoordType::UTM);
    if (dmsRegex.match(clean_str).hasMatch())  return qMakePair(true, CoordType::DMS);
    if (ck42Regex.match(clean_str).hasMatch()) return qMakePair(true, CoordType::CK42);

    return qMakePair(false, CoordType::NotValid);
}



QString CirclePolygonGenerator::generateCirclePolygon(double centerLat, double centerLon, double radiusMeters, int sides) {
    if (sides < 3) sides = 16;

    QStringList coordinates;

    // Коэффициенты для преобразования метров в градусы
    double latDegreeInMeters = 111000.0; // примерно постоянно
    double lonDegreeInMeters = 111000.0 * qCos(qDegreesToRadians(centerLat)); // зависит от широты

    // Генерируем точки окружности
    for (int i = 0; i <= sides; i++) { // <= для замыкания полигона
        double angle = 2.0 * M_PI * i / sides;

        // Вычисляем смещение в метрах
        double deltaX = radiusMeters * qCos(angle);
        double deltaY = radiusMeters * qSin(angle);

        // Преобразуем в градусы и добавляем к центру
        double lat = centerLat + deltaY / latDegreeInMeters;
        double lon = centerLon + deltaX / lonDegreeInMeters;

        // Форматируем в строку (6 знаков после запятой для точности)
        coordinates.append(QString("WGS84:%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
    }

    return coordinates.join("/");
}

QString CirclePolygonGenerator::generateCirclePolygonAccurate(double centerLat, double centerLon, double radiusMeters, int sides) {
    if (sides < 3) sides = 16;

    QStringList coordinates;

    // Радиус Земли в метрах (средний)
    const double earthRadius = 6371000.0;

    // Преобразуем центр в радианы
    double centerLatRad = qDegreesToRadians(centerLat);
    double centerLonRad = qDegreesToRadians(centerLon);

    // Угловое расстояние (в радианах)
    double angularDistance = radiusMeters / earthRadius;

    // Генерируем точки окружности
    for (int i = 0; i <= sides; i++) { // <= для замыкания полигона
        double bearing = 2.0 * M_PI * i / sides;

        // Формулы сферической геометрии
        double lat = qAsin(qSin(centerLatRad) * qCos(angularDistance) +
                           qCos(centerLatRad) * qSin(angularDistance) * qCos(bearing));

        double lon = centerLonRad + qAtan2(qSin(bearing) * qSin(angularDistance) * qCos(centerLatRad),
                                           qCos(angularDistance) - qSin(centerLatRad) * qSin(lat));

        // Преобразуем обратно в градусы
        lat = qRadiansToDegrees(lat);
        lon = qRadiansToDegrees(lon);

        // Нормализуем долготу
        if (lon > 180.0) lon -= 360.0;
        if (lon < -180.0) lon += 360.0;

        coordinates.append(QString("WGS84:%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
    }

    return coordinates.join("/");
}

QString CirclePolygonGenerator::generateEllipsePolygon(double centerLat, double centerLon, double radiusMeters, int sides) {
    if (sides < 3) sides = 16;

    QStringList coordinates;

    // Коэффициенты для преобразования метров в градусы
    double latDegreeInMeters = 111000.0;
    double lonDegreeInMeters = 111000.0 * qCos(qDegreesToRadians(centerLat));

    // Коррекция радиуса для эллипса (больше растяжение по долготе на высоких широтах)
    double latRadius = radiusMeters;
    double lonRadius = radiusMeters;

    // На высоких широтах увеличиваем радиус по долготе
    if (qAbs(centerLat) > 45.0) {
        lonRadius *= (1.0 + qAbs(centerLat) / 180.0);
    }

    for (int i = 0; i <= sides; i++) {
        double angle = 2.0 * M_PI * i / sides;

        double deltaX = lonRadius * qCos(angle);
        double deltaY = latRadius * qSin(angle);

        double lat = centerLat + deltaY / latDegreeInMeters;
        double lon = centerLon + deltaX / lonDegreeInMeters;

        coordinates.append(QString("WGS84:%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
    }

    return coordinates.join("/");
}
