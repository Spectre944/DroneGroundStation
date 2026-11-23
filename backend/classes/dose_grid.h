#ifndef DOSE_GRID_H
#define DOSE_GRID_H

/**
    @file  dose_grid.h
    @brief Тепловая карта ПАЕД (Ф7) и горячие точки (Ф8): сетка по области,
           значения клеток методом обратных расстояний (IDW).

    Клетка — взвешенное среднее измерений в радиусе R от её центра,
    вес точки 1 / d^p (по умолчанию p = 2); ближе 1 м — как 1 м.
    Нет измерений в радиусе — «нет данных» (NaN), а не ноль:
    ноль на карте выглядел бы как «чисто».

    @note  Сетка — по принятой области (ID04). Области нет (например, программу
           перезапустили в полёте) — по измерениям, с запасом R; растёт вместе с ними.
    @note  Новое измерение пересчитывает только клетки в радиусе R от него.
    @note  Клеток не больше MaxCells: для большой области шаг клетки увеличивается.
    @note  Живёт в потоке ядра, в CoreEngine. Рисует слой Никиты (MapWidget::onGridUpdated).
*/

#include <QObject>
#include <QHash>
#include <QVector>

#include "core_types.h"

class DoseGrid : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxCells = 250000;     ///< не больше клеток (500 × 500): ~1 МБ на сигнал

    explicit DoseGrid(QObject *parent = nullptr);

    /**
        @brief Принятая область (ID01 на ID04): новая территория — сетка заново, точки сбрасываются.
        @param area Вершины; пусто — сетка по измерениям.
    */
    void setArea(const QVector<GeoPoint> &area);

    /**
        @brief Новое измерение: пересчитать клетки в радиусе R, найти горячие точки, выдать сетку.
        @note  Недостоверное измерение (флаги Ф9) в расчёт не идёт.
    */
    void addMeasurement(const Measurement &m);

    /**
        @brief Все измерения сразу (прошлый полёт): полный расчёт, одна выдача.
    */
    void setMeasurements(const QVector<Measurement> &list);

    /**
        @brief Параметры из настроек: шаг клетки, радиус, степень, порог предупреждения.
        @note  Сетка пересчитывается заново.
    */
    void setSettings(double cellM, double radiusM, double power, float warnLevel);

    void clear();                               ///< без области и точек; выдаёт пустую сетку — слой очищается
    GridData data() const { return m_grid; }

signals:
    void gridUpdated(const GridData &grid);

private:
    struct Point {
        double lat = 0.0, lon = 0.0;            ///< градусы
        double x = 0.0, y = 0.0;                ///< метры от юго-западного угла сетки
        float  v = 0.0f;                        ///< ПАЕД
    };

    void rebuildGeometry();                     ///< границы, шаг, маска «в области»; точки — в метры
    void recalcAll();                           ///< все клетки
    void recalcCells(int r0, int r1, int c0, int c1);   ///< клетки прямоугольника строк и столбцов
    void findHotspots();
    float cellValue(int row, int col) const;    ///< IDW по точкам в радиусе
    qint64 bucketKey(int bx, int by) const { return (qint64(bx) << 32) ^ quint32(by); }
    void addToBucket(int index);
    bool nearEdge(double lat, double lon) const;    ///< точка ближе R к краю сетки (или за ним)
    void publish() { emit gridUpdated(m_grid); }

    // настройки
    double m_cellSetM = 10.0;                   ///< шаг из настроек
    double m_radiusM  = 50.0;
    double m_power    = 2.0;
    float  m_warn     = 0.3f;

    QVector<GeoPoint> m_area;                   ///< принятая область; пусто — по измерениям
    QVector<Point>    m_points;
    QHash<qint64, QVector<int>> m_buckets;      ///< точки по квадратам R × R — быстрый поиск соседей

    GridData m_grid;
    double   m_mLat = 0.0;                      ///< метров в градусе широты
    double   m_mLon = 0.0;                      ///< метров в градусе долготы на широте сетки
};

#endif // DOSE_GRID_H
