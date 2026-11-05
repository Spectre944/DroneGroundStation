#ifndef COORDHANDLER_H
#define COORDHANDLER_H

#include <QObject>
#include <QMetaType>
#include <QString>
#include <QList>
#include <QPair>
#include <QtMath>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStringList>
#include <db/sqlite3.h>
#include <map/mapcore.h>

#include "mgrs.h"
#include "utm.h"

enum class CoordType { WGS84_Pair,  WGS84_String,  MGRS,  UTM,  DMS,  CK42,  NotValid};

Q_DECLARE_METATYPE(CoordType)

class CoordHandler : public QObject
{
    Q_OBJECT

public:

    /**
        @brief : Структура для хранения разобранных компонентов DMS
    */
    struct DMSComponents {

        DMSComponents() {
            degrees = 0;
            minutes = 0;
            seconds = 0.0;
            isValid = false;
            direction.clear();
        }

        int degrees;
        int minutes;
        double seconds;
        bool isValid;
        QString direction;
    };

    struct SK42Rectangle {
        double x;  // meters
        double y;  // meters
        int zone;  // zone number
    };

    explicit CoordHandler(QObject *parent = nullptr);

    /**
        @brief Преобразование одной координаты из типа в тип
    */
    static QPair<bool, QString> convertSingleCoord(const QString &, const CoordType &);

    /**
        @brief Преобразование пакета координат из типа в тип
    */
    static QPair<bool, QString> convertPackageCoords(const QString &, const CoordType &);

    /**
        @brief Преобразование пакета координат для карты
    */
    static QPair<bool, QVector<smappoint> > convertPackageCoordsToMapPoints(const QString &);


    /**
        @brief Проверка вхождения точки в область
    */
    static QList<bool> pointInArea(const QString &, const QString &);
    static void pointInAreaForSQL(sqlite3_context *context, int argc, sqlite3_value **argv);

    /**
        @brief Расчет расстояния между двумя точками
    */
    static QVariantList distanceBetweenPoints(const QString &, const QString &);

    static double distanceInMetersBetweenPoints(const QPair<double, double>& point1, const QPair<double, double>& point2);

    /**
        @brief Расчет площади области
    */
    static QVariantList calculatePolygonArea(const QString &);
    static QVariantList calculatePolygonArea(const QList<QPair<double, double> > &points);

    /**
        @brief Расчет длины маршрута
    */
    static QVariantList calculateRouteLength(const QString &);
    static QVariantList calculateRouteLength(const QList<QPair<double, double> > &points);


    static QPair<bool, QString> Wgs84P_to_Wgs84S(const QPair<double, double>&);
    static int countCoordsInText(const QString &text);







private slots:

    /**
        @brief  Статический метод для анализа типа строковой координаты
        @return Возвращаем пару с флагом преобразования и типом координаты
    */
    static QPair<bool, CoordType> defining_type_coord(const QString&);

    /**
        @brief  Статические методы перевода пары(double, double) WGS84 в строковую координату
        @return Возвращаем пару с флагом преобразования и координатой-строкой выбранного формата
    */
    // static QPair<bool, QString> Wgs84P_to_Wgs84S(const QPair<double, double>&);
    static QPair<bool, QString> Wgs84P_to_MGRS(const QPair<double, double>&);
    static QPair<bool, QString> Wgs84P_to_UTM(const QPair<double, double>&);
    static QPair<bool, QString> Wgs84P_to_DMS(const QPair<double, double>&);
    static QPair<bool, QString> Wgs84P_to_CK42(const QPair<double, double>&);

    /**
        @brief  Статические методы перевода строковой координаты в пару(double, double) WGS84
        @return Возвращаем пару с флагом преобразования и парой координат
    */
    static QPair<bool, QPair<double, double>> Wgs84S_to_Wgs84P(const QString&);
    static QPair<bool, QPair<double, double>> MGRS_to_Wgs84P(const QString&);
    static QPair<bool, QPair<double, double>> UTM_to_Wgs84P(const QString&);
    static QPair<bool, QPair<double, double>> DMS_to_Wgs84P(const QString&);
    static QPair<bool, QPair<double, double>> CK42_to_Wgs84P(const QString&);

    /**
        @brief  Статические методы перевода списка строковых координат в список пар(double, double) WGS84
        @return Возвращаем пару с флагом преобразования и парой координат
    */

    static QPair<bool, QList<QPair<double, double>>> Wgs84S_to_Wgs84P_List(const QStringList&);
    static QPair<bool, QList<QPair<double, double>>> MGRS_to_Wgs84P_List(const QStringList&);
    static QPair<bool, QList<QPair<double, double>>> UTM_to_Wgs84P_List(const QStringList&);
    static QPair<bool, QList<QPair<double, double>>> DMS_to_Wgs84P_List(const QStringList&);
    static QPair<bool, QList<QPair<double, double>>> CK42_to_Wgs84P_List(const QStringList&);


    static QPair<bool, QString> Wgs84P_to_Wgs84S_List(const QList<QPair<double,double>>&);
    static QPair<bool, QString> Wgs84P_to_MGRS_List(const QList<QPair<double,double>>&);
    static QPair<bool, QString> Wgs84P_to_UTM_List(const QList<QPair<double,double>>&);
    static QPair<bool, QString> Wgs84P_to_DMS_List(const QList<QPair<double,double>>&);
    static QPair<bool, QString> Wgs84P_to_CK42S_List(const QList<QPair<double,double>>&);


    static bool check_Point_In_Area(const QList<QPair<double, double>>& area_list, const QPair<double, double>& point);
    static double calc_Distance(const QPair<double, double>& point1, const QPair<double, double>& point2);
    static double calculateAreaWGS84(const QList<QPair<double, double>>& coord_list);
    static double _calculateRouteLength(const QList<QPair<double, double>>& coord_list);

private:

    QString format_coord(double);

    bool parse_wgs_pair(const QPair<double,double>&);
    bool parse_wgs_str(const QString&);

    DMSComponents parse_dms_str(const QString&);
    bool is_valid_lat_dms(const DMSComponents&);
    bool is_valid_long_dms(const DMSComponents&);
    double dms_to_decimal(const DMSComponents&);
    DMSComponents decimal_to_dms(double);

    QPair<double, double> convert_ck42str_to_ck42pair(const QString&);
    QPair<double, double> ck42pair_to_wgs_pair(const QPair<double, double>&);
    QPair<double, double> sk42_rectangle_to_wgs_pair(double, double, int);
    SK42Rectangle wgs_pair_to_sk42_rectangle(const QPair<double, double>&);
    QPair<double, double> wgs_pair_to_ck42pair(const QPair<double, double>&);
    QString convert_ck42pair_to_ck42str(const QPair<double, double>&);


    static bool is_point_on_boundary(const QList<QPair<double, double>>& area_list, const QPair<double, double>& point, double epsilon = 1e-9);
    static bool is_point_in_area(const QList<QPair<double, double>>& area_list, const QPair<double, double>& point);

};

class CirclePolygonGenerator {
public:
    /**
     * @brief Генерирует полигон в виде многоугольника, аппроксимирующего круг
     * @param centerLat - широта центра
     * @param centerLon - долгота центра
     * @param radiusMeters - радиус в метрах
     * @param sides - количество сторон полигона (по умолчанию 16)
     * @return QString - строка координат в формате для pointInArea
     */
    static QString generateCirclePolygon(double centerLat, double centerLon,
                                         double radiusMeters, int sides = 16);

    /**
     * @brief Более точная версия с учетом эллипсоидности Земли
     * Использует формулы сферической геометрии
     */
    static QString generateCirclePolygonAccurate(double centerLat, double centerLon,
                                                 double radiusMeters, int sides = 16);

    /**
     * @brief Генерирует эллипс (для учета искажений на больших широтах)
     */
    static QString generateEllipsePolygon(double centerLat, double centerLon,
                                          double radiusMeters, int sides = 16);
};

#endif // COORDHANDLER_H
