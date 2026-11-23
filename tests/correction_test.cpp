/**
    @file  correction_test.cpp
    @brief Проверка DoseCorrection (пересчёт ПАЕД на 1 м, флаги достоверности) без окна.

    Временная модель: ПАЕД(1 м) = ПАЕД(h) · exp(μ · (h − 1)).
    Флаги: LIDAR вне 1–600 м или слабый сигнал; точка вне принятой области.
    В конце — через всю цепочку CoreEngine: измерение из «порта» → ПАЕД на 1 м,
    недостоверное измерение не даёт тревогу и не идёт в тепловую карту.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>
#include <cmath>
#include <cstring>

#include "classes/dose_correction.h"
#include "classes/core_engine.h"
#include "classes/drone_link.h"

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

bool approx(double a, double b, double eps = 1e-4) { return std::fabs(a - b) < eps; }

const QVector<GeoPoint> Square = { {50.445, 30.515}, {50.445, 30.529}, {50.454, 30.529}, {50.454, 30.515} };

Measurement meas(float paed, quint16 lidarCm, double lat = 50.45, double lon = 30.52, quint16 strength = 0x0FFF)
{
    Measurement m;
    m.paed = paed;
    m.lidarCm = lidarCm;
    m.lidarStrength = strength;
    m.lat = lat;
    m.lon = lon;
    return m;
}

CoreSettings settings(bool on, double mu, double minM = 0, double maxM = 0)
{
    CoreSettings s;
    s.correctionOn = on;
    s.correctionCoeffs = {mu};
    s.correctionMinM = minM;
    s.correctionMaxM = maxM;
    return s;
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

/// ID22: ПАЕД, высота LIDAR, координаты
QByteArray id22(float paed, quint16 lidarCm, float lat, float lon)
{
    QByteArray b = f32(paed) + QByteArray::fromHex("5A0111");
    b.append(char(lidarCm & 0xFF));
    b.append(char(lidarCm >> 8));
    return b + QByteArray::fromHex("FF0F") + f32(lat) + f32(lon) + QByteArray(5, 0) + f32(12.4f)
             + QByteArray::fromHex("0C230A");
}

void runTests()
{
    out << "Проверка DoseCorrection (пересчёт на 1 м, достоверность)" << Qt::endl;

    // 1. Пересчёт выключен
    {
        DoseCorrection c;
        c.setSettings(settings(false, 0.01));
        const Measurement m = c.apply(meas(0.5f, 1500));
        check("Пересчёт выключен — ПАЕД на 1 м не считается, показывается исходная 0,5",
              !m.hasPaed1m && approx(m.shownPaed(), 0.5) && m.flags == MfNone);
    }

    // 2. Пересчёт включён: h = 15 м, μ = 0,01 → 0,5 · exp(0,14)
    {
        DoseCorrection c;
        c.setSettings(settings(true, 0.01));
        const Measurement m = c.apply(meas(0.5f, 1500));
        const double expected = 0.5 * std::exp(0.01 * 14.0);
        check("Пересчёт включён, h = 15 м, μ = 0,01 — 0,5 · e^0,14 = 0,575",
              m.hasPaed1m && approx(m.paed1m, expected) && approx(m.shownPaed(), expected) && approx(m.paed, 0.5),
              QString::number(m.paed1m));
        check("На высоте 1 м пересчёт ничего не меняет",
              approx(c.apply(meas(0.5f, 100)).paed1m, 0.5));
    }

    // 3. Допустимые высоты 5–50 м
    {
        DoseCorrection c;
        c.setSettings(settings(true, 0.01, 5, 50));
        check("Высоты 5–50 м: на 15 м — пересчёт есть", c.apply(meas(0.5f, 1500)).hasPaed1m);
        check("Высоты 5–50 м: на 60 м — пересчёта нет, показывается исходная",
              !c.apply(meas(0.5f, 6000)).hasPaed1m);
    }

    // 4. LIDAR вне 1–600 м или слабый сигнал — недостоверно, пересчёта нет
    {
        DoseCorrection c;
        c.setSettings(settings(true, 0.01));
        const Measurement low = c.apply(meas(0.5f, 50));
        const Measurement high = c.apply(meas(0.5f, 60001));
        const Measurement weak = c.apply(meas(0.5f, 1500, 50.45, 30.52, 50));
        check("LIDAR 0,5 м — флаг «LIDAR», ПАЕД на 1 м нет", (low.flags & MfLidarRange) && !low.hasPaed1m);
        check("LIDAR 600,01 м — флаг «LIDAR»", high.flags & MfLidarRange);
        check("Слабый сигнал LIDAR (50 < 100) — флаг «LIDAR»", weak.flags & MfLidarRange);
    }

    // 5. Вне принятой области
    {
        DoseCorrection c;
        const bool noAreaOk = c.apply(meas(0.5f, 1500, 50.47, 30.52)).flags == MfNone;
        c.setArea(Square);
        const Measurement in = c.apply(meas(0.5f, 1500, 50.45, 30.52));
        const Measurement out = c.apply(meas(0.5f, 1500, 50.47, 30.52));
        check("Области нет — флаг «вне области» не ставится", noAreaOk);
        check("Точка в области — флагов нет, вне области — флаг «вне области»",
              in.flags == MfNone && (out.flags & MfOutOfArea));
    }

    // 6. Вся цепочка
    {
        FakeLink link;
        CoreEngine engine(&link);
        engine.applySettings(settings(true, 0.01));

        QVector<Measurement> measurements;
        QVector<Alarm> alarms;
        QVector<GridData> grids;
        QObject::connect(&engine, &CoreEngine::measurement, [&](const Measurement &m) { measurements.append(m); });
        QObject::connect(&engine, &CoreEngine::alarm,       [&](const Alarm &a) { alarms.append(a); });
        QObject::connect(&engine, &CoreEngine::gridUpdated, [&](const GridData &g) { grids.append(g); });

        QDateTime t = QDateTime(QDate(2026, 11, 23), QTime(12, 0), QTimeZone::utc());
        auto receive = [&](quint8 id, const QByteArray &payload) {
            t = t.addSecs(3);
            link.processBytes(DroneLink::buildPacket(id, payload), t);
        };

        CommandRequest a;
        a.id = 4; a.vertices = Square; a.reconMode = ReconMode::Time; a.intervalS = 30; a.distanceM = 100; a.dupFactor = 3;
        engine.sendCommand(a);
        receive(0x01, {});                                          // область принята
        receive(0x14, QByteArray::fromHex("CDCC49421B2FF4410C2238"));   // ID20

        receive(0x16, id22(0.2f, 1500, 50.45f, 30.52f));           // в области, 15 м
        const double expected = 0.2f * std::exp(0.14);
        const bool corrected = measurements.size() == 1 && measurements[0].hasPaed1m
                            && approx(measurements[0].paed1m, expected);

        grids.clear();
        receive(0x16, id22(5.0f, 1500, 50.47f, 30.52f));           // вне области, ПАЕД 5
        const bool outFlag = measurements.size() == 2 && (measurements[1].flags & MfOutOfArea);
        int validAfter = 0;
        for (const GridData &g : grids)
            for (float v : g.values)
                if (!std::isnan(v) && v > 1.0f)
                    ++validAfter;

        check("CoreEngine: ID22 на 15 м — ПАЕД на 1 м посчитана (0,2 · e^0,14)", corrected);
        check("CoreEngine: ID22 вне области — флаг «вне области»", outFlag);
        check("CoreEngine: недостоверное измерение с ПАЕД 5 — тревоги нет, в тепловую карту не попало",
              alarms.isEmpty() && validAfter == 0);
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

    runTests();
    return g_failed == 0 ? 0 : 1;
}
