/**
    @file  area_test.cpp
    @brief Проверка ReconArea без окна.

    Правильные области должны проходить, каждая неправильная — давать свою
    ошибку и номер вершины. «Точка внутри» — центр, сторона, вершина, снаружи.
    Плюс: DroneCommands не отправляет ID04 с неправильной областью.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QTextStream>
#include <QVector>
#include <algorithm>
#include <cmath>

#include "classes/recon_area.h"
#include "classes/drone_commands.h"
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

// Точки задаются в метрах от базовой точки под Киевом: x — на восток, y — на север
const double BaseLat = 50.45;
const double BaseLon = 30.52;
const double Pi = 3.14159265358979323846;

GeoPoint pt(double xm, double ym)
{
    const double mLat = 6371000.0 * Pi / 180.0;     // метров в градусе широты — радиус Земли как в логгере
    const double mLon = mLat * std::cos(BaseLat * Pi / 180.0);
    return { BaseLat + ym / mLat, BaseLon + xm / mLon };
}

/// Правильный многоугольник: n вершин на окружности радиусом r м, против часовой
QVector<GeoPoint> regular(int n, double r)
{
    QVector<GeoPoint> v;
    for (int i = 0; i < n; ++i)
        v.append(pt(r * std::cos(2 * Pi * i / n), r * std::sin(2 * Pi * i / n)));
    return v;
}

/// Квадрат 1 × 1 км против часовой: ЮЗ, ЮВ, СВ, СЗ
QVector<GeoPoint> square()
{
    return { pt(0, 0), pt(1000, 0), pt(1000, 1000), pt(0, 1000) };
}

/// Название ошибки — для подробностей при провале
QString errorName(AreaError e)
{
    switch (e) {
    case AreaError::None:                return "нет";
    case AreaError::TooFewVertices:      return "мало вершин";
    case AreaError::TooManyVertices:     return "много вершин";
    case AreaError::BadCoordinate:       return "координата";
    case AreaError::DuplicateVertex:     return "повтор";
    case AreaError::Collinear:           return "на прямой";
    case AreaError::SelfIntersection:    return "пересечение";
    case AreaError::NotConvex:           return "невыпуклая";
    case AreaError::TooLarge:            return "большая";
    case AreaError::CrossesAntimeridian: return "180°";
    case AreaError::RouteNotSupported:   return "линия";
    }
    return "?";
}

/// Проверить, что область даёт ожидаемую ошибку и номер вершины (-2 — номер не важен)
void expectError(const QString &name, const QVector<GeoPoint> &v, AreaError error, int vertex = -2,
                 bool isRoute = false)
{
    const AreaCheck c = ReconArea::check(v, isRoute, 50.0);
    const bool ok = !c.ok && c.error == error && (vertex == -2 || c.vertex == vertex);
    check(name, ok, QString("ошибка «%1», вершина %2").arg(errorName(c.error)).arg(c.vertex));
}

void runTests()
{
    out << "Проверка ReconArea" << Qt::endl;

    // ---- правильные области ----
    {
        QVector<GeoPoint> cw = square();
        std::reverse(cw.begin(), cw.end());
        check("Квадрат 1 км против часовой — годится", ReconArea::check(square()).ok);
        check("Квадрат 1 км по часовой — годится", ReconArea::check(cw).ok);
        check("Треугольник — годится", ReconArea::check({pt(0, 0), pt(800, 0), pt(300, 600)}).ok);
        check("Правильный 20-угольник радиусом 2 км — годится", ReconArea::check(regular(20, 2000)).ok);
    }

    // ---- неправильные области: каждая со своей ошибкой ----
    expectError("2 вершины — мало", {pt(0, 0), pt(1000, 0)}, AreaError::TooFewVertices);
    expectError("21 вершина — много", regular(21, 2000), AreaError::TooManyVertices);
    {
        QVector<GeoPoint> v = square();
        v[2].lat = 91.0;
        expectError("Широта 91° у вершины 2 — неверная координата", v, AreaError::BadCoordinate, 2);
    }
    expectError("Линия вместо многоугольника — не поддерживается", square(), AreaError::RouteNotSupported, -1, true);
    {
        QVector<GeoPoint> v = square();
        v.insert(2, pt(1000.3, 0.2));                       // в 0,4 м от вершины 1
        expectError("Вершина в 0,4 м от соседней — повтор", v, AreaError::DuplicateVertex, 2);
    }
    {
        QVector<GeoPoint> v = square();
        v.insert(1, pt(500, 0.3));                          // середина нижней стороны, 0,3 м в сторону
        expectError("Вершина посреди стороны — три на одной прямой", v, AreaError::Collinear, 1);
    }
    expectError("«Бантик» — стороны пересекаются",
                {pt(0, 0), pt(1000, 1000), pt(1000, 0), pt(0, 1000)}, AreaError::SelfIntersection);
    {
        const QVector<GeoPoint> p = regular(5, 1000);
        expectError("Пятиконечная звезда — стороны пересекаются",
                    {p[0], p[2], p[4], p[1], p[3]}, AreaError::SelfIntersection);
    }
    expectError("«Стрелка» (вмятина в вершине 3) — невыпуклая",
                {pt(0, 0), pt(1000, 0), pt(1000, 1000), pt(500, 400), pt(0, 1000)}, AreaError::NotConvex, 3);
    expectError("Прямоугольник 60 × 1 км — больше 50 км",
                {pt(0, 0), pt(60000, 0), pt(60000, 1000), pt(0, 1000)}, AreaError::TooLarge);
    expectError("Область через 180-й меридиан",
                {{10.0, 179.9}, {10.0, -179.9}, {10.1, -179.9}, {10.1, 179.9}}, AreaError::CrossesAntimeridian);

    // ---- точка внутри (алгоритм логгера) ----
    {
        const QVector<GeoPoint> sq = square();
        const GeoPoint mid{ sq[0].lat, (sq[0].lon + sq[1].lon) / 2 };   // середина нижней стороны
        const GeoPoint south = pt(500, -10);
        const GeoPoint east  = pt(1010, 500);
        check("Центр квадрата — внутри", ReconArea::contains(sq, pt(500, 500).lat, pt(500, 500).lon));
        check("Точка на стороне — внутри (граница считается внутри)", ReconArea::contains(sq, mid.lat, mid.lon));
        check("Вершина — внутри", ReconArea::contains(sq, sq[2].lat, sq[2].lon));
        check("10 м южнее нижней стороны — снаружи", !ReconArea::contains(sq, south.lat, south.lon));
        check("10 м восточнее правой стороны — снаружи", !ReconArea::contains(sq, east.lat, east.lon));
        check("Далеко (Львов) — снаружи", !ReconArea::contains(sq, 49.84, 24.03));
        check("Область из 2 вершин — «снаружи» для любой точки",
              !ReconArea::contains({pt(0, 0), pt(1000, 0)}, BaseLat, BaseLon));
    }

    // ---- прямоугольник и расстояние ----
    {
        const QVector<GeoPoint> sq = square();
        const ReconArea::Bounds b = ReconArea::bounds(sq);
        check("Охватывающий прямоугольник квадрата",
              b.latMin == sq[0].lat && b.latMax == sq[2].lat && b.lonMin == sq[0].lon && b.lonMax == sq[1].lon);
        const double d = ReconArea::distanceM(pt(0, 0), pt(1000, 0));
        check("Расстояние 1 км — 1000 ± 1 м", std::fabs(d - 1000.0) < 1.0, QString::number(d));
    }

    // ---- DroneCommands: ID04 с неправильной областью не уходит ----
    {
        DroneLink link;                     // порт закрыт
        DroneCommands commands(&link);
        QVector<CommandOutcome> results;
        QObject::connect(&commands, &DroneCommands::commandFinished,
                         [&results](const CommandOutcome &o) { results.append(o); });

        CommandRequest r;
        r.id = 4;
        r.reconMode = ReconMode::Time;
        r.intervalS = 30;
        r.distanceM = 100;
        r.dupFactor = 3;

        r.vertices = {pt(0, 0), pt(1000, 0), pt(1000, 1000), pt(500, 400), pt(0, 1000)};   // «стрелка»
        commands.send(r);
        r.vertices = square();                                                              // правильная
        commands.send(r);

        // «Стрелка» — отказ по области; квадрат проходит проверки и упирается в закрытый порт
        const bool ok = results.size() == 2
                     && results[0].reason == RejectReason::AreaNotValid
                     && results[1].reason == RejectReason::PortClosed;
        check("ID04 со «стрелкой» — «область не годится», с квадратом — дошла до порта", ok);
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
