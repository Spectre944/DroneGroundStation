#include "dose_grid.h"
#include "recon_area.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    constexpr double Pi = 3.14159265358979323846;
    constexpr double EarthRadiusM = 6371000.0;                      ///< как в логгере
    constexpr double MetersPerDegLat = EarthRadiusM * Pi / 180.0;
    constexpr float  NoData = std::numeric_limits<float>::quiet_NaN();
}

DoseGrid::DoseGrid(QObject *parent)
    : QObject(parent)
{
}

// ============================================================================
//  Вход
// ============================================================================

void DoseGrid::setArea(const QVector<GeoPoint> &area)
{
    m_area = area;
    m_points.clear();                       // новая территория — прежние точки к ней не относятся
    m_buckets.clear();
    rebuildGeometry();
    recalcAll();
    publish();
}

void DoseGrid::addMeasurement(const Measurement &m)
{
    if (m.flags != MfNone)                  // недостоверное — в интерполяцию не идёт (Ф9)
        return;

    Point p;
    p.lat = m.lat;
    p.lon = m.lon;
    p.v = m.shownPaed();
    m_points.append(p);

    // Без области сетка растёт вместе с точками: точка ближе R к краю — границы заново
    if (m_area.isEmpty() && (m_grid.cols == 0 || nearEdge(p.lat, p.lon))) {
        rebuildGeometry();
        recalcAll();
        publish();
        return;
    }
    if (m_grid.cols == 0)
        return;

    Point &added = m_points.last();
    added.x = (added.lon - m_grid.lonMin) * m_mLon;
    added.y = (added.lat - m_grid.latMin) * m_mLat;
    addToBucket(m_points.size() - 1);

    // Только клетки в радиусе R от новой точки
    const double c = m_grid.cellM;
    const int r0 = int(std::floor((added.y - m_radiusM) / c)), r1 = int(std::ceil((added.y + m_radiusM) / c));
    const int c0 = int(std::floor((added.x - m_radiusM) / c)), c1 = int(std::ceil((added.x + m_radiusM) / c));
    recalcCells(r0, r1, c0, c1);
    findHotspots();
    publish();
}

void DoseGrid::setMeasurements(const QVector<Measurement> &list)
{
    m_points.clear();
    m_buckets.clear();
    for (const Measurement &m : list) {
        if (m.flags != MfNone)
            continue;
        Point p;
        p.lat = m.lat;
        p.lon = m.lon;
        p.v = m.shownPaed();
        m_points.append(p);
    }
    rebuildGeometry();
    recalcAll();
    publish();
}

void DoseGrid::setSettings(double cellM, double radiusM, double power, float warnLevel)
{
    m_cellSetM = cellM > 0 ? cellM : 10.0;
    m_radiusM  = radiusM > 0 ? radiusM : 50.0;
    m_power    = power > 0 ? power : 2.0;
    m_warn     = warnLevel;
    rebuildGeometry();
    recalcAll();
    publish();
}

void DoseGrid::clear()
{
    m_area.clear();
    m_points.clear();
    m_buckets.clear();
    m_grid = GridData();
    publish();
}

// ============================================================================
//  Геометрия
// ============================================================================

/**
    @brief Границы, шаг и маска. Точки переводятся в метры от юго-западного угла.
*/
void DoseGrid::rebuildGeometry()
{
    m_grid = GridData();
    m_buckets.clear();

    // Границы: область или точки с запасом R
    double latMin, latMax, lonMin, lonMax;
    if (!m_area.isEmpty()) {
        const ReconArea::Bounds b = ReconArea::bounds(m_area);
        latMin = b.latMin; latMax = b.latMax; lonMin = b.lonMin; lonMax = b.lonMax;
    } else if (!m_points.isEmpty()) {
        latMin = latMax = m_points[0].lat;
        lonMin = lonMax = m_points[0].lon;
        for (const Point &p : std::as_const(m_points)) {
            latMin = std::min(latMin, p.lat); latMax = std::max(latMax, p.lat);
            lonMin = std::min(lonMin, p.lon); lonMax = std::max(lonMax, p.lon);
        }
        const double padLat = m_radiusM / MetersPerDegLat;
        const double padLon = m_radiusM / (MetersPerDegLat * std::cos((latMin + latMax) / 2 * Pi / 180.0));
        latMin -= padLat; latMax += padLat; lonMin -= padLon; lonMax += padLon;
    } else {
        return;                             // ни области, ни точек — сетки нет
    }

    m_mLat = MetersPerDegLat;
    m_mLon = MetersPerDegLat * std::cos((latMin + latMax) / 2 * Pi / 180.0);
    const double widthM  = std::max(1.0, (lonMax - lonMin) * m_mLon);
    const double heightM = std::max(1.0, (latMax - latMin) * m_mLat);

    // Шаг: из настроек, но клеток не больше MaxCells
    double cell = m_cellSetM;
    if ((widthM / cell) * (heightM / cell) > MaxCells)
        cell = std::ceil(std::sqrt(widthM * heightM / MaxCells));

    m_grid.cellM  = cell;
    // −1e-6: 400 м / 10 м — ровно 40 клеток, а не 41 из-за погрешности перевода градусов в метры
    m_grid.cols   = std::max(1, int(std::ceil(widthM / cell - 1e-6)));
    m_grid.rows   = std::max(1, int(std::ceil(heightM / cell - 1e-6)));
    m_grid.latMin = latMin;
    m_grid.lonMin = lonMin;
    m_grid.latMax = latMin + m_grid.rows * cell / m_mLat;
    m_grid.lonMax = lonMin + m_grid.cols * cell / m_mLon;
    m_grid.area   = m_area;

    const int n = m_grid.rows * m_grid.cols;
    m_grid.values = QVector<float>(n, NoData);
    m_grid.inArea = QVector<quint8>(n, 1);
    if (!m_area.isEmpty()) {
        for (int r = 0; r < m_grid.rows; ++r)
            for (int c = 0; c < m_grid.cols; ++c) {
                const double lat = latMin + (r + 0.5) * cell / m_mLat;
                const double lon = lonMin + (c + 0.5) * cell / m_mLon;
                m_grid.inArea[r * m_grid.cols + c] = ReconArea::contains(m_area, lat, lon) ? 1 : 0;
            }
    }

    // Точки — в метры и по квадратам
    for (int i = 0; i < m_points.size(); ++i) {
        m_points[i].x = (m_points[i].lon - lonMin) * m_mLon;
        m_points[i].y = (m_points[i].lat - latMin) * m_mLat;
        addToBucket(i);
    }
}

bool DoseGrid::nearEdge(double lat, double lon) const
{
    const double x = (lon - m_grid.lonMin) * m_mLon;
    const double y = (lat - m_grid.latMin) * m_mLat;
    return x < m_radiusM || y < m_radiusM
        || x > m_grid.cols * m_grid.cellM - m_radiusM || y > m_grid.rows * m_grid.cellM - m_radiusM;
}

void DoseGrid::addToBucket(int index)
{
    const Point &p = m_points[index];
    const int bx = int(std::floor(p.x / m_radiusM));
    const int by = int(std::floor(p.y / m_radiusM));
    m_buckets[bucketKey(bx, by)].append(index);
}

// ============================================================================
//  Расчёт
// ============================================================================

void DoseGrid::recalcAll()
{
    if (m_grid.cols == 0)
        return;
    recalcCells(0, m_grid.rows - 1, 0, m_grid.cols - 1);
    findHotspots();
}

void DoseGrid::recalcCells(int r0, int r1, int c0, int c1)
{
    r0 = std::max(r0, 0); c0 = std::max(c0, 0);
    r1 = std::min(r1, m_grid.rows - 1); c1 = std::min(c1, m_grid.cols - 1);
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c) {
            const int i = r * m_grid.cols + c;
            m_grid.values[i] = m_grid.inArea[i] ? cellValue(r, c) : NoData;   // вне области не считаем
        }
}

/**
    @brief IDW: Σ w·v / Σ w по точкам в радиусе R, w = 1 / d^p; d < 1 м — как 1 м.
*/
float DoseGrid::cellValue(int row, int col) const
{
    const double cx = (col + 0.5) * m_grid.cellM;
    const double cy = (row + 0.5) * m_grid.cellM;
    const int bx = int(std::floor(cx / m_radiusM));
    const int by = int(std::floor(cy / m_radiusM));
    const double r2 = m_radiusM * m_radiusM;

    double sumW = 0.0, sumV = 0.0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const auto it = m_buckets.constFind(bucketKey(bx + dx, by + dy));
            if (it == m_buckets.constEnd())
                continue;
            for (int i : it.value()) {
                const Point &p = m_points[i];
                const double d2 = (p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy);
                if (d2 > r2)
                    continue;
                const double d = std::max(std::sqrt(d2), 1.0);
                const double w = 1.0 / std::pow(d, m_power);
                sumW += w;
                sumV += w * p.v;
            }
        }
    return sumW > 0.0 ? float(sumV / sumW) : NoData;
}

/**
    @brief Горячие точки: участки выше порога предупреждения, у которых нет
           соседа с большим значением (локальные максимумы).
    @note  Вокруг одиночного измерения все клетки в радиусе R одинаковые — ровное
           «плато». Плато ищется целиком (соседние клетки с тем же значением), и на
           каждое плато-максимум — одна горячая точка: клетка, ближайшая к его середине.
*/
void DoseGrid::findHotspots()
{
    m_grid.hotspots.clear();
    const int rows = m_grid.rows, cols = m_grid.cols;
    const QVector<float> &val = m_grid.values;
    QVector<quint8> visited(val.size(), 0);

    auto same = [](float a, float b) { return std::fabs(a - b) <= 1e-6f * std::max(1.0f, std::fabs(a)); };

    for (int start = 0; start < val.size(); ++start) {
        const float v = val[start];
        if (visited[start] || std::isnan(v) || v < m_warn)
            continue;

        // Плато: все соседние клетки с тем же значением (обход в ширину)
        QVector<int> plateau{start};
        visited[start] = 1;
        bool isMax = true;
        for (int k = 0; k < plateau.size(); ++k) {
            const int r = plateau[k] / cols, c = plateau[k] % cols;
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc) {
                    const int rr = r + dr, cc = c + dc;
                    if ((dr == 0 && dc == 0) || rr < 0 || cc < 0 || rr >= rows || cc >= cols)
                        continue;
                    const int j = rr * cols + cc;
                    const float n = val[j];
                    if (std::isnan(n))
                        continue;
                    if (same(n, v)) {
                        if (!visited[j]) {
                            visited[j] = 1;
                            plateau.append(j);
                        }
                    } else if (n > v) {
                        isMax = false;          // рядом есть больше — не максимум
                    }
                }
        }
        if (!isMax)
            continue;

        // Одна точка на плато — клетка, ближайшая к его середине
        double sr = 0, sc = 0;
        for (int j : plateau) {
            sr += j / cols;
            sc += j % cols;
        }
        sr /= plateau.size();
        sc /= plateau.size();
        int best = plateau[0];
        double bestD = 1e300;
        for (int j : plateau) {
            const double d = (j / cols - sr) * (j / cols - sr) + (j % cols - sc) * (j % cols - sc);
            if (d < bestD) {
                bestD = d;
                best = j;
            }
        }
        m_grid.hotspots.append(best);
    }
    std::sort(m_grid.hotspots.begin(), m_grid.hotspots.end());
}
