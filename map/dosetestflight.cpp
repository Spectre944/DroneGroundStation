#include "dosetestflight.h"

#include <QHash>
#include <QRandomGenerator>
#include <QtMath>
#include <cmath>
#include <limits>

namespace {

double gauss(QRandomGenerator &rng)
{
    // Бокс-Мюллер
    const double u1 = qMax(1e-12, rng.generateDouble());
    const double u2 = rng.generateDouble();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

double bump(double u, double v, double cu, double cv, double amp, double sigma)
{
    const double du = u - cu, dv = v - cv;
    return amp * std::exp(-(du * du + dv * dv) / (2.0 * sigma * sigma));
}

} // namespace

DoseTestFlight::DoseTestFlight(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &DoseTestFlight::tick);
}

void DoseTestFlight::setGridParams(double cellM, double radiusM, double power, float warnLevel)
{
    m_cellM = qMax(1.0, cellM);
    m_radiusM = qMax(m_cellM, radiusM);
    m_power = power;
    m_warn = warnLevel;
}

void DoseTestFlight::toLatLon(double u, double v, double &lat, double &lon) const
{
    const double e = u * std::cos(m_angle) - v * std::sin(m_angle);
    const double n = u * std::sin(m_angle) + v * std::cos(m_angle);
    lat = m_lat0 + n / m_mPerDegLat;
    lon = m_lon0 + e / m_mPerDegLon;
}

void DoseTestFlight::toLocal(double lat, double lon, double &u, double &v) const
{
    const double e = (lon - m_lon0) * m_mPerDegLon;
    const double n = (lat - m_lat0) * m_mPerDegLat;
    u =  e * std::cos(m_angle) + n * std::sin(m_angle);
    v = -e * std::sin(m_angle) + n * std::cos(m_angle);
}

// "Истинное" поле ПАЕД на 1 м, мкЗв/ч
double DoseTestFlight::field(double u, double v) const
{
    double f = 0.10 + 0.035 * (u + m_halfU) / (2.0 * m_halfU);   // фон с лёгким наклоном
    f += bump(u, v,  180,   60, 0.55, 50);                       // широкое пятно ~0.65
    f += bump(u, v, -140,  -90, 3.60, 28);                       // сильный источник ~3.7
    f += bump(u, v,   60, -150, 1.10, 16);                       // маленький, около порога тревоги
    if (u > -140)                                                // "шлейф" от сильного источника
        f += 0.25 * std::exp(-std::pow(v + 90, 2) / (2 * 20.0 * 20.0)) * std::exp(-(u + 140) / 250.0);
    return f;
}

void DoseTestFlight::generate(double centerLat, double centerLon)
{
    stop();
    m_lat0 = centerLat;
    m_lon0 = centerLon;
    m_mPerDegLat = 111320.0;
    m_mPerDegLon = 111320.0 * std::cos(qDegreesToRadians(centerLat));

    QRandomGenerator rng(944);
    m_points.clear();
    m_sent = 0;

    // Маршрут "змейкой": галсы через 25 м, шаг 6 м (≈ 1 точка в секунду при 6 м/с),
    // на разворотах дрон вылетает на 30 м за область
    struct P { double u, v; };
    QVector<P> path;
    const double spacing = 25, step = 6, over = 30;
    const int lines = int((2 * m_halfV) / spacing);
    for (int k = 0; k < lines; ++k) {
        const double v = -m_halfV + spacing / 2 + k * spacing;
        const bool fwd = (k % 2) == 0;
        const double uStart = fwd ? -m_halfU - over : m_halfU + over;
        const double uEnd   = -uStart;
        const int n = int((2 * (m_halfU + over)) / step);
        for (int i = 0; i <= n; ++i)
            path.append({ uStart + (fwd ? 1 : -1) * i * step, v });
        if (k + 1 < lines)
            for (double dv = step; dv < spacing; dv += step)
                path.append({ uEnd, v + dv });
    }

    const QDateTime start = QDateTime::currentDateTime();
    m_points.reserve(path.size());
    for (int i = 0; i < path.size(); ++i) {
        const double u = path[i].u + gauss(rng) * 1.2;    // погрешность GPS
        const double v = path[i].v + gauss(rng) * 1.2;

        Measurement m;
        m.id = i + 1;
        m.time = start.addSecs(i);
        m.rxTime = m.time.addMSecs(300);
        m.paed1m = float(field(u, v) * std::exp(gauss(rng) * 0.12));   // статистический шум
        m.paed = m.paed1m * 1.10f;
        m.hasPaed1m = true;
        m.lidarCm = quint16(qBound(300.0, 1500 + gauss(rng) * 60, 6000.0));
        m.lidarStrength = 2000;
        m.battV = float(16.4 - i * 0.0008);
        toLatLon(u, v, m.lat, m.lon);

        if (std::fabs(u) > m_halfU || std::fabs(v) > m_halfV)
            m.flags |= MfOutOfArea;
        const double r = rng.generateDouble();
        if (r < 0.012) {
            m.flags |= MfLidarRange;
            m.lidarCm = 3900;
        } else if (r < 0.017) {
            m.flags |= MfBdbgStat;
        }
        m_points.append(m);
    }
}

GridData DoseTestFlight::buildGrid(int count) const
{
    GridData g;
    if (count < 0 || count > m_points.size())
        count = m_points.size();

    // Границы - охватывающий прямоугольник повёрнутой области
    double latMin = 1e9, latMax = -1e9, lonMin = 1e9, lonMax = -1e9;
    const double cu[4] = { -m_halfU,  m_halfU, m_halfU, -m_halfU };
    const double cv[4] = { -m_halfV, -m_halfV, m_halfV,  m_halfV };
    for (int i = 0; i < 4; ++i) {
        GeoPoint p;
        toLatLon(cu[i], cv[i], p.lat, p.lon);
        g.area.append(p);
        latMin = qMin(latMin, p.lat); latMax = qMax(latMax, p.lat);
        lonMin = qMin(lonMin, p.lon); lonMax = qMax(lonMax, p.lon);
    }
    g.cellM = m_cellM;
    g.cols = int(std::ceil((lonMax - lonMin) * m_mPerDegLon / m_cellM));
    g.rows = int(std::ceil((latMax - latMin) * m_mPerDegLat / m_cellM));
    g.latMin = latMin;
    g.lonMin = lonMin;
    g.latMax = latMin + g.rows * m_cellM / m_mPerDegLat;    // квадратные клетки
    g.lonMax = lonMin + g.cols * m_cellM / m_mPerDegLon;

    const int cells = g.cols * g.rows;
    g.values.fill(std::numeric_limits<float>::quiet_NaN(), cells);
    g.inArea.fill(0, cells);

    // Индекс достоверных точек по корзинам размером R
    struct L { double e, n; float val; };
    QVector<L> pts;
    QHash<qint64, QVector<int>> bins;
    const double R = m_radiusM;
    auto key = [](int bx, int by) { return (qint64(bx) << 32) ^ quint32(by); };
    for (int i = 0; i < count; ++i) {
        const Measurement &m = m_points[i];
        if (m.flags != MfNone)
            continue;   // недостоверные в интерполяцию не идут (Ф9)
        L l { (m.lon - m_lon0) * m_mPerDegLon, (m.lat - m_lat0) * m_mPerDegLat, m.shownPaed() };
        bins[key(int(std::floor(l.e / R)), int(std::floor(l.n / R)))].append(pts.size());
        pts.append(l);
    }

    const double dLat = (g.latMax - g.latMin) / g.rows;
    const double dLon = (g.lonMax - g.lonMin) / g.cols;
    for (int r = 0; r < g.rows; ++r) {
        for (int c = 0; c < g.cols; ++c) {
            const double lat = g.latMin + (r + 0.5) * dLat;
            const double lon = g.lonMin + (c + 0.5) * dLon;
            double u, v;
            toLocal(lat, lon, u, v);
            if (std::fabs(u) > m_halfU || std::fabs(v) > m_halfV)
                continue;
            const int idx = r * g.cols + c;
            g.inArea[idx] = 1;

            const double e = (lon - m_lon0) * m_mPerDegLon;
            const double n = (lat - m_lat0) * m_mPerDegLat;
            const int bx = int(std::floor(e / R)), by = int(std::floor(n / R));
            double sw = 0, swv = 0;
            bool exact = false;
            for (int dy = -1; dy <= 1 && !exact; ++dy) {
                for (int dx = -1; dx <= 1 && !exact; ++dx) {
                    const auto it = bins.constFind(key(bx + dx, by + dy));
                    if (it == bins.constEnd())
                        continue;
                    for (int pi : it.value()) {
                        const double d = std::hypot(pts[pi].e - e, pts[pi].n - n);
                        if (d > R)
                            continue;
                        if (d < 0.5) {
                            swv = pts[pi].val;
                            sw = 1;
                            exact = true;
                            break;
                        }
                        const double w = 1.0 / std::pow(d, m_power);
                        sw += w;
                        swv += w * pts[pi].val;
                    }
                }
            }
            if (sw > 0)
                g.values[idx] = float(swv / sw);
        }
    }

    // Горячие точки (Ф8): выше порога и максимум в окрестности 7x7 клеток
    // (в тесте окрестность шире, чем 3x3 из описания, - иначе шум даёт россыпь пиков)
    for (int r = 0; r < g.rows; ++r) {
        for (int c = 0; c < g.cols; ++c) {
            const int idx = r * g.cols + c;
            const float val = g.values[idx];
            if (!g.inArea[idx] || std::isnan(val) || val < m_warn)
                continue;
            bool peak = true;
            for (int dy = -3; dy <= 3 && peak; ++dy)
                for (int dx = -3; dx <= 3 && peak; ++dx) {
                    if (!dx && !dy)
                        continue;
                    const int rr = r + dy, cc = c + dx;
                    if (rr < 0 || cc < 0 || rr >= g.rows || cc >= g.cols)
                        continue;
                    const float nv = g.values[rr * g.cols + cc];
                    if (!std::isnan(nv) && nv >= val)
                        peak = false;
                }
            if (peak)
                g.hotspots.append(idx);
        }
    }
    return g;
}

void DoseTestFlight::startLive(int pointsPerTick, int intervalMs)
{
    m_sent = 0;
    m_perTick = qMax(1, pointsPerTick);
    m_timer.start(qMax(10, intervalMs));
}

void DoseTestFlight::stop()
{
    m_timer.stop();
}

void DoseTestFlight::tick()
{
    const int end = qMin(m_points.size(), m_sent + m_perTick);
    for (; m_sent < end; ++m_sent)
        emit measurement(m_points[m_sent]);
    emit gridUpdated(buildGrid(m_sent));

    if (m_sent >= m_points.size()) {
        m_timer.stop();
        emit finished();
    }
}
