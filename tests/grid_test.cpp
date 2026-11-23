/**
    @file  grid_test.cpp
    @brief Проверка DoseGrid (тепловая карта) без окна и без логгера.

    Маска области, IDW по одной и двум точкам, «нет данных» вне радиуса,
    частичный пересчёт = полному, горячие точки, большая область, сетка без
    области. В конце — через всю цепочку CoreEngine: ID04, ID20, ID22 из «порта»
    → сетка; открытие прошлого полёта → его сетка.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>
#include <cmath>
#include <cstring>

#include "classes/dose_grid.h"
#include "classes/core_engine.h"
#include "classes/drone_link.h"
#include "classes/session_store.h"

#ifdef _WIN32
#include <windows.h>                // SetConsoleOutputCP, CP_UTF8
#endif

namespace {

QTextStream out(stdout);
int g_failed = 0;   ///< число проваленных проверок

/// Напечатать итог одной проверки
void check(const QString &name, bool ok, const QString &details = QString())
{
    out << (ok ? "  ok      " : "  ОШИБКА  ") << name;
    if (!ok && !details.isEmpty())
        out << "  (" << details << ")";
    out << Qt::endl;
    if (!ok)
        ++g_failed;
}

// Точки в метрах от базовой точки: x — на восток, y — на север
const double BaseLat = 50.45, BaseLon = 30.52;
const double Pi = 3.14159265358979323846;
const double MLat = 6371000.0 * Pi / 180.0;
const double MLon = MLat * std::cos(BaseLat * Pi / 180.0);

GeoPoint pt(double x, double y) { return { BaseLat + y / MLat, BaseLon + x / MLon }; }

Measurement meas(double x, double y, float paed, quint32 flags = MfNone)
{
    Measurement m;
    const GeoPoint p = pt(x, y);
    m.lat = p.lat;
    m.lon = p.lon;
    m.paed = paed;
    m.flags = flags;
    return m;
}

/// Квадрат 400 × 400 м
QVector<GeoPoint> square() { return { pt(0, 0), pt(400, 0), pt(400, 400), pt(0, 400) }; }

/// Значение клетки, в которой лежит точка (x, y) метров от базы
float valueAt(const GridData &g, double x, double y)
{
    const GeoPoint p = pt(x, y);
    const int c = int((p.lon - g.lonMin) * MLon / g.cellM);
    const int r = int((p.lat - g.latMin) * MLat / g.cellM);
    if (r < 0 || c < 0 || r >= g.rows || c >= g.cols)
        return std::numeric_limits<float>::quiet_NaN();
    return g.values[r * g.cols + c];
}

int countValid(const GridData &g)
{
    int n = 0;
    for (float v : g.values)
        if (!std::isnan(v))
            ++n;
    return n;
}

// ---- для проверки через CoreEngine ----
class FakeLink : public DroneLink
{
public:
    SendResult send(quint8, const QByteArray &) override { return SendResult::Ok; }
    bool canSend() const override { return true; }
    void answerReceived() override {}
};

QByteArray f32(float v)
{
    quint32 bits;
    std::memcpy(&bits, &v, 4);
    QByteArray b;
    for (int i = 0; i < 4; ++i)
        b.append(char((bits >> (8 * i)) & 0xFF));
    return b;
}

void runTests(const QString &dbPath)
{
    out << "Проверка DoseGrid (тепловая карта)" << Qt::endl;

    // 1. Сетка по области: 40 × 40 клеток по 10 м, маска треугольника
    {
        DoseGrid g;
        g.setArea({ pt(0, 0), pt(400, 0), pt(0, 400) });
        const GridData d = g.data();
        int inside = 0;
        for (quint8 m : d.inArea)
            inside += m;
        check("Область 400 × 400 м, шаг 10 м — 40 × 40 клеток, все «нет данных»",
              d.cols == 40 && d.rows == 40 && countValid(d) == 0, QString("%1 × %2").arg(d.cols).arg(d.rows));
        check("Треугольник — в области примерно половина клеток (700–900 из 1600)",
              inside > 700 && inside < 900, QString::number(inside));
    }

    // 2. Одна точка: в радиусе 50 м — её значение, дальше — «нет данных»
    {
        DoseGrid g;
        g.setArea(square());
        g.addMeasurement(meas(200, 200, 0.5f));
        const GridData d = g.data();
        check("Одна точка 0,5: клетка в 30 м — 0,5", std::fabs(valueAt(d, 230, 200) - 0.5f) < 1e-5);
        check("Клетка в 100 м — «нет данных» (NaN), а не ноль", std::isnan(valueAt(d, 300, 200)));
    }

    // 3. Две точки 0,2 и 1,0 в 40 м — посередине 0,6 (равные веса)
    {
        DoseGrid g;
        g.setArea(square());
        g.addMeasurement(meas(185, 205, 0.2f));
        g.addMeasurement(meas(225, 205, 1.0f));
        const float mid = valueAt(g.data(), 205, 205);
        check("Две точки 0,2 и 1,0 — посередине 0,6", std::fabs(mid - 0.6f) < 0.01f, QString::number(mid));
    }

    // 4. Недостоверное измерение не участвует
    {
        DoseGrid g;
        g.setArea(square());
        g.addMeasurement(meas(200, 200, 5.0f, MfLidarRange));
        check("Недостоверное измерение (флаг Ф9) — сетка пустая", countValid(g.data()) == 0);
    }

    // 5. Частичный пересчёт = полному
    {
        DoseGrid partial, full;
        partial.setArea(square());
        full.setArea(square());
        QVector<Measurement> list;
        auto *rng = QRandomGenerator::global();
        for (int i = 0; i < 60; ++i) {
            const Measurement m = meas(rng->bounded(400.0), rng->bounded(400.0), float(0.1 + rng->bounded(2.0)));
            list.append(m);
            partial.addMeasurement(m);
        }
        full.setMeasurements(list);
        const GridData a = partial.data(), b = full.data();
        bool same = a.values.size() == b.values.size();
        for (int i = 0; same && i < a.values.size(); ++i)
            same = (std::isnan(a.values[i]) && std::isnan(b.values[i])) || std::fabs(a.values[i] - b.values[i]) < 1e-4;
        check("60 случайных точек: пересчёт по одной = полный пересчёт", same);
        check("Горячие точки одинаковы в обоих случаях", a.hotspots == b.hotspots);
    }

    // 6. Горячие точки: два пика выше 0,3 — две; фон ниже порога — ни одной
    {
        DoseGrid g;
        g.setArea(square());
        g.setMeasurements({ meas(100, 100, 1.2f), meas(300, 300, 0.8f), meas(100, 300, 0.1f) });
        check("Два пика 1,2 и 0,8 — две горячие точки, фон 0,1 — нет",
              g.data().hotspots.size() == 2, QString::number(g.data().hotspots.size()));
    }

    // 7. Большая область 50 × 50 км — шаг увеличен, клеток не больше 250 000
    {
        DoseGrid g;
        g.setArea({ pt(0, 0), pt(50000, 0), pt(50000, 50000), pt(0, 50000) });
        const GridData d = g.data();
        check("Область 50 × 50 км — шаг больше 10 м, клеток ≤ 250 000",
              d.cellM > 10.0 && d.cols * d.rows <= DoseGrid::MaxCells,
              QString("шаг %1 м, клеток %2").arg(d.cellM).arg(d.cols * d.rows));
    }

    // 8. Без области — сетка по точкам и растёт вместе с ними
    {
        DoseGrid g;
        g.addMeasurement(meas(0, 0, 0.4f));
        const int cols1 = g.data().cols;
        const bool first = std::fabs(valueAt(g.data(), 0, 0) - 0.4f) < 1e-5;
        g.addMeasurement(meas(500, 0, 0.6f));
        check("Без области: сетка вокруг первой точки, значение 0,4", first && cols1 > 0);
        check("Точка в 500 м — сетка выросла, у новой точки 0,6",
              g.data().cols > cols1 && std::fabs(valueAt(g.data(), 500, 0) - 0.6f) < 1e-5);
    }

    // 9. Настройки: шаг 20 м
    {
        DoseGrid g;
        g.setArea(square());
        g.setSettings(20.0, 50.0, 2.0, 0.3f);
        check("Шаг 20 м из настроек — 20 × 20 клеток", g.data().cols == 20 && g.data().rows == 20);
    }

    // 10. Вся цепочка: «порт» → CoreEngine → сетка; прошлый полёт → его сетка
    {
        FakeLink link;
        CoreEngine engine(&link);
        CoreSettings s;
        s.dbPath = dbPath;
        engine.applySettings(s);
        engine.startSession("Тепловая карта");

        QVector<GridData> grids;
        QObject::connect(&engine, &CoreEngine::gridUpdated, [&](const GridData &gd) { grids.append(gd); });

        QDateTime t = QDateTime(QDate(2026, 11, 23), QTime(12, 0), QTimeZone::utc());
        auto receive = [&](quint8 id, const QByteArray &payload) {
            t = t.addSecs(3);
            link.processBytes(DroneLink::buildPacket(id, payload), t);
        };

        CommandRequest id04;
        id04.id = 4;
        id04.vertices = { {50.445, 30.515}, {50.445, 30.529}, {50.454, 30.529}, {50.454, 30.515} };
        id04.reconMode = ReconMode::Time;
        id04.intervalS = 30; id04.distanceM = 100; id04.dupFactor = 3;
        engine.sendCommand(id04);
        receive(0x01, {});                                          // ID04 принята
        const bool areaGrid = !grids.isEmpty() && grids.last().area.size() == 4 && countValid(grids.last()) == 0;

        receive(0x14, QByteArray::fromHex("CDCC49421B2FF4410C2238"));                   // ID20
        receive(0x16, f32(1.5f) + QByteArray::fromHex("5A0111E803FF0F") + f32(50.45f) + f32(30.522f)
                      + QByteArray(5, 0) + f32(12.4f) + QByteArray::fromHex("0C230A")); // ID22, ПАЕД 1,5
        const bool live = !grids.isEmpty() && countValid(grids.last()) > 0 && grids.last().hotspots.size() == 1;

        const qint64 sessionId = engine.store()->currentSession().id;
        grids.clear();
        engine.openSession(sessionId);
        const bool past = grids.size() == 1 && grids[0].area.size() == 4 && countValid(grids[0]) > 0;

        check("CoreEngine: ID01 на ID04 — сетка по принятой области, пустая", areaGrid);
        check("CoreEngine: ID22 с ПАЕД 1,5 — клетки посчитаны, одна горячая точка", live);
        check("CoreEngine: открыт прошлый полёт — его сетка по области и измерениям", past);
    }

    out << Qt::endl << (g_failed == 0 ? "ИТОГ: все проверки пройдены"
                                      : QString("ИТОГ: провалено проверок — %1").arg(g_failed))
        << Qt::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    registerCoreTypes();

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);    // консоль Windows: показывать вывод в UTF-8, иначе вместо русского — кракозябры
#endif

    QTemporaryDir dir;              // временная папка для базы — удаляется после проверки
    runTests(dir.filePath("grid_test.db"));
    return g_failed == 0 ? 0 : 1;
}
