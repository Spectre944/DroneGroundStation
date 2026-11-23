#include "recon_area.h"

#include <algorithm>
#include <cmath>

/**
    @brief Расчёт на плоскости: вершины переводятся в метры от первой вершины.
    @note  Для области до десятков километров плоское приближение точное
           до долей метра. Знаки (слева / справа) — те же, что в градусах.
*/
namespace {
    constexpr double Pi = 3.14159265358979323846;
    constexpr double EarthRadiusM    = 6371000.0;   ///< средний радиус Земли — как в логгере (logger_state_machine.c)
    constexpr double MetersPerDegLat = EarthRadiusM * Pi / 180.0;   ///< метров в градусе широты (~111 195)

    struct XY {
        double x = 0.0;     ///< на восток, м
        double y = 0.0;     ///< на север, м
    };

    /// Округлить вершины до float — как их получит логгер в ID04
    QVector<GeoPoint> toFloat(const QVector<GeoPoint> &v)
    {
        QVector<GeoPoint> f;
        f.reserve(v.size());
        for (const GeoPoint &p : v)
            f.append({ double(float(p.lat)), double(float(p.lon)) });
        return f;
    }

    /// Вершины в метрах от первой вершины
    QVector<XY> toXY(const QVector<GeoPoint> &v)
    {
        const double lat0 = v[0].lat, lon0 = v[0].lon;
        const double mLon = MetersPerDegLat * std::cos(lat0 * Pi / 180.0);
        QVector<XY> xy;
        xy.reserve(v.size());
        for (const GeoPoint &p : v)
            xy.append({ (p.lon - lon0) * mLon, (p.lat - lat0) * MetersPerDegLat });
        return xy;
    }

    /// Векторное произведение (b − a) × (c − a): > 0 — c слева от направления a→b, < 0 — справа
    double cross(const XY &a, const XY &b, const XY &c)
    {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    /// Пересекаются ли отрезки ab и cd (в том числе касанием)
    bool segmentsIntersect(const XY &a, const XY &b, const XY &c, const XY &d)
    {
        const double d1 = cross(c, d, a), d2 = cross(c, d, b);
        const double d3 = cross(a, b, c), d4 = cross(a, b, d);
        if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
            ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
            return true;                            // пересекаются «крест-накрест»

        // Касание: конец одного отрезка лежит на другом
        auto onSegment = [](const XY &p, const XY &q, const XY &r) {    // r на отрезке pq (уже на одной прямой)
            return std::min(p.x, q.x) <= r.x && r.x <= std::max(p.x, q.x)
                && std::min(p.y, q.y) <= r.y && r.y <= std::max(p.y, q.y);
        };
        return (d1 == 0 && onSegment(c, d, a)) || (d2 == 0 && onSegment(c, d, b))
            || (d3 == 0 && onSegment(a, b, c)) || (d4 == 0 && onSegment(a, b, d));
    }

    /// Итог «ошибка»
    AreaCheck fail(const QVector<GeoPoint> &v, AreaError error, int vertex = -1)
    {
        AreaCheck c;
        c.ok = false;
        c.error = error;
        c.vertex = vertex;
        c.vertices = v;
        return c;
    }
}

// ============================================================================
//  Проверка
// ============================================================================

AreaCheck ReconArea::check(const QVector<GeoPoint> &vertices, bool isRoute, double maxAreaKm)
{
    const int n = vertices.size();

    // 1. Линия вместо многоугольника
    if (isRoute)
        return fail(vertices, AreaError::RouteNotSupported);

    // 2. Число вершин — иначе логгер ответит ID02
    if (n < CoreConst::AreaMinVertices)
        return fail(vertices, AreaError::TooFewVertices);
    if (n > CoreConst::AreaMaxVertices)
        return fail(vertices, AreaError::TooManyVertices);

    // 3. Координаты
    for (int i = 0; i < n; ++i) {
        const GeoPoint &p = vertices[i];
        if (!std::isfinite(p.lat) || !std::isfinite(p.lon)
            || p.lat < -90.0 || p.lat > 90.0 || p.lon < -180.0 || p.lon > 180.0)
            return fail(vertices, AreaError::BadCoordinate, i);
    }

    // 4. 180-й меридиан: долгота «прыгает» с +180 на −180, логгер считает в градусах напрямую.
    //    Проверяется до расчётов на плоскости: через 180° они тоже неверны.
    const Bounds b = bounds(vertices);
    if (b.lonMax - b.lonMin > 180.0)
        return fail(vertices, AreaError::CrossesAntimeridian);

    // Дальше — с вершинами, как их получит логгер (float), в метрах на плоскости
    const QVector<GeoPoint> f = toFloat(vertices);
    const QVector<XY> xy = toXY(f);

    // 5. Повтор вершины: нулевая сторона ломает «слева / справа»
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (distanceM(f[i], f[j]) < MinVertexDistM)
                return fail(vertices, AreaError::DuplicateVertex, j);

    // 6. Три вершины подряд на одной прямой: средняя ближе 1 м к линии соседей
    for (int i = 0; i < n; ++i) {
        const XY &prev = xy[(i + n - 1) % n], &cur = xy[i], &next = xy[(i + 1) % n];
        const double base = std::hypot(next.x - prev.x, next.y - prev.y);
        if (base > 0.0 && std::fabs(cross(prev, next, cur)) / base < MinVertexDistM)
            return fail(vertices, AreaError::Collinear, i);
    }

    // 7. Пересечение сторон (несоседних): «бантик», «звезда»
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
            if (adjacent)
                continue;
            if (segmentsIntersect(xy[i], xy[(i + 1) % n], xy[j], xy[(j + 1) % n]))
                return fail(vertices, AreaError::SelfIntersection, i);
        }
    }

    // 8. Выпуклость: во всех вершинах поворот в одну сторону
    int sign = 0;
    for (int i = 0; i < n; ++i) {
        const double c = cross(xy[(i + n - 1) % n], xy[i], xy[(i + 1) % n]);
        const int s = (c > 0) ? 1 : -1;     // нулей нет: три на прямой уже отсеяны
        if (sign == 0)
            sign = s;
        else if (s != sign)
            return fail(vertices, AreaError::NotConvex, i);
    }

    // 9. Размер: самое большое расстояние между вершинами
    double maxDist = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            maxDist = std::max(maxDist, distanceM(f[i], f[j]));
    if (maxDist > maxAreaKm * 1000.0)
        return fail(vertices, AreaError::TooLarge);

    AreaCheck ok;
    ok.ok = true;
    ok.vertices = vertices;
    return ok;
}

// ============================================================================
//  Точка внутри — алгоритм логгера
// ============================================================================

/**
    @note Перенос LoggerStateMachine_IsPointInsideArea (logger_state_machine.c):
          для каждой стороны AB — векторное произведение AB × AP;
          все ненулевые одного знака — точка внутри; ноль (точка на прямой
          стороны) — сторона не ограничивает. Координаты — float, как в логгере.
*/
bool ReconArea::contains(const QVector<GeoPoint> &vertices, double lat, double lon)
{
    const int n = vertices.size();
    if (n < CoreConst::AreaMinVertices || n > CoreConst::AreaMaxVertices)
        return false;

    const double pLat = double(float(lat));
    const double pLon = double(float(lon));
    int direction = 0;                      // 0 — не определено, 1 — положительное, −1 — отрицательное

    for (int i = 0; i < n; ++i) {
        const int next = (i + 1 == n) ? 0 : i + 1;

        // X — долгота, Y — широта (как в логгере)
        const double aLat = double(float(vertices[i].lat)),    aLon = double(float(vertices[i].lon));
        const double bLat = double(float(vertices[next].lat)), bLon = double(float(vertices[next].lon));

        const double edgeX  = bLon - aLon;
        const double edgeY  = bLat - aLat;
        const double pointX = pLon - aLon;
        const double pointY = pLat - aLat;
        const double c = edgeX * pointY - edgeY * pointX;

        if (c == 0.0)                       // точка на прямой стороны — граница считается внутри
            continue;

        if (direction == 0)
            direction = (c > 0.0) ? 1 : -1;
        else if ((c > 0.0 && direction < 0) || (c < 0.0 && direction > 0))
            return false;
    }
    return true;
}

// ============================================================================
//  Прямоугольник и расстояние
// ============================================================================

ReconArea::Bounds ReconArea::bounds(const QVector<GeoPoint> &vertices)
{
    Bounds b;
    if (vertices.isEmpty())
        return b;
    b.latMin = b.latMax = vertices[0].lat;
    b.lonMin = b.lonMax = vertices[0].lon;
    for (const GeoPoint &p : vertices) {
        b.latMin = std::min(b.latMin, p.lat);
        b.latMax = std::max(b.latMax, p.lat);
        b.lonMin = std::min(b.lonMin, p.lon);
        b.lonMax = std::max(b.lonMax, p.lon);
    }
    return b;
}

double ReconArea::distanceM(const GeoPoint &a, const GeoPoint &b)
{
    const double toRad = Pi / 180.0;
    const double dLat = (b.lat - a.lat) * toRad;
    const double dLon = (b.lon - a.lon) * toRad;
    const double h = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(a.lat * toRad) * std::cos(b.lat * toRad)
                   * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * EarthRadiusM * std::asin(std::min(1.0, std::sqrt(h)));
}
