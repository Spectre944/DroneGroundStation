/**
    @file  engine_test.cpp
    @brief Проверка CoreEngine — всей цепочки ядра — без окна и без логгера.

    В CoreEngine подставляется «поддельный» DroneLink: команды не уходят в порт,
    а запоминаются. Тест сам подаёт байты пакетов, как будто они пришли из порта,
    и смотрит, что выходит из CoreEngine наружу. Сценарий — полёт целиком.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>
#include <cmath>
#include <cstring>

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

/**
    @brief «Поддельный» DroneLink: команды не уходят в порт, а запоминаются.
           Принятые байты подаются вызовом processBytes — как из порта.
*/
class FakeLink : public DroneLink
{
public:
    QVector<quint8> sentIds;        ///< ID отправленных команд

    SendResult send(quint8 id, const QByteArray &) override
    {
        sentIds.append(id);
        return SendResult::Ok;
    }
    bool canSend() const override { return true; }
    void answerReceived() override {}
};

/// Всё, что CoreEngine выдал наружу
struct Output {
    QVector<LoggerStatus>   status;
    QList<quint8>           allowed;
    QVector<RoutePoint>     route;
    QVector<Measurement>    measurements;
    QVector<CoreEvent>      events;
    QVector<CommandOutcome> commands;
    QVector<AreaCheck>      areas;

    void connectTo(CoreEngine &e)
    {
        QObject::connect(&e, &CoreEngine::loggerStatusChanged,    [this](const LoggerStatus &s) { status.append(s); });
        QObject::connect(&e, &CoreEngine::allowedCommandsChanged, [this](const QList<quint8> &a) { allowed = a; });
        QObject::connect(&e, &CoreEngine::routePoint,             [this](const RoutePoint &r) { route.append(r); });
        QObject::connect(&e, &CoreEngine::measurement,            [this](const Measurement &m) { measurements.append(m); });
        QObject::connect(&e, &CoreEngine::event,                  [this](const CoreEvent &ev) { events.append(ev); });
        QObject::connect(&e, &CoreEngine::commandFinished,        [this](const CommandOutcome &o) { commands.append(o); });
        QObject::connect(&e, &CoreEngine::areaChecked,            [this](const AreaCheck &c) { areas.append(c); });
    }
    LoggerMode mode() const { return status.isEmpty() ? LoggerMode::Unknown : status.last().mode; }
};

/// «Пришёл пакет из порта». Каждый следующий — на 2 с позже: иначе одинаковые пакеты
/// (например, два ответа ID01) отсеялись бы как копии.
QDateTime g_time = QDateTime(QDate(2026, 11, 23), QTime(12, 34, 0), QTimeZone::utc());
void receive(FakeLink &link, quint8 id, const QByteArray &payload = {})
{
    g_time = g_time.addSecs(2);
    link.processBytes(DroneLink::buildPacket(id, payload), g_time);
}

QByteArray f32(float v)
{
    quint32 bits;
    std::memcpy(&bits, &v, 4);
    QByteArray b;
    for (int i = 0; i < 4; ++i)
        b.append(char((bits >> (8 * i)) & 0xFF));
    return b;
}

// ID20 и ID22 — из примера в протоколе (п. 10.3)
const QByteArray Coord = QByteArray::fromHex("CDCC4942" "1B2FF441" "0C2238");
const QByteArray Id22  = QByteArray::fromHex("0000003E" "5A01" "11" "E803" "FF0F" "CDCC4942" "1B2FF441"
                                             "00000000" "00" "66664641" "0C230A");

CommandRequest cmd(quint8 id, int value = 0)
{
    CommandRequest r;
    r.id = id;
    r.value = value;
    return r;
}

/// ID04: квадрат ~1 км под Киевом или «стрелка»
CommandRequest id04(bool good)
{
    CommandRequest r = cmd(4);
    if (good)
        r.vertices = { {50.445, 30.515}, {50.445, 30.529}, {50.454, 30.529}, {50.454, 30.515} };
    else
        r.vertices = { {50.445, 30.515}, {50.445, 30.529}, {50.454, 30.529}, {50.449, 30.522}, {50.454, 30.515} };
    r.reconMode = ReconMode::Time;
    r.intervalS = 30;
    r.distanceM = 100;
    r.dupFactor = 3;
    return r;
}

void runTests()
{
    out << "Проверка CoreEngine (вся цепочка ядра)" << Qt::endl;

    FakeLink link;
    CoreEngine engine(&link);
    Output o;
    o.connectTo(engine);

    // 1. ID01 → ответ ID01 → «проверка»
    engine.sendCommand(cmd(0x01));
    receive(link, 0x01);
    check("ID01 ушла, ответ ID01 — «выполнено», режим «проверка»",
          link.sentIds == QVector<quint8>{1} && o.commands.size() == 1
          && o.commands[0].result == CommandResult::Done && o.mode() == LoggerMode::Test);

    // 2. Приборы → ID18 → «ожидание»
    receive(link, 0x13, Coord);
    receive(link, 0x0B);
    receive(link, 0x09);
    receive(link, 0x05, f32(15.6f));
    receive(link, 0x12);
    check("Проверка пройдена — «ожидание», все приборы исправны",
          o.mode() == LoggerMode::Expect
          && o.status.last().devices[int(LoggerDevice::Sd)].check == DeviceCheck::Ok
          && o.status.last().devices[int(LoggerDevice::Gps)].check == DeviceCheck::Ok);
    check("Разрешённые команды сменились на «ожидание»: ID04–ID09, ID11, ID14",
          o.allowed == QList<quint8>({4, 5, 6, 7, 8, 9, 11, 14}));
    check("Стартовая координата (ID19) — точка маршрута с режимом «проверка»",
          !o.route.isEmpty() && o.route[0].msgId == 0x13 && o.route[0].mode == LoggerMode::Test);

    // 3. ID04: «стрелка» не уходит, квадрат уходит и принимается
    engine.sendCommand(id04(false));
    const bool arrowRejected = o.commands.last().result == CommandResult::Rejected
                            && o.commands.last().reason == RejectReason::AreaNotValid
                            && link.sentIds.size() == 1;
    engine.sendCommand(id04(true));
    receive(link, 0x01);
    check("ID04 со «стрелкой» — не отправлена, «область не годится»", arrowRejected);
    check("ID04 с квадратом — ушла, ответ ID01, область принята (4 вершины)",
          link.sentIds.last() == 4 && o.commands.last().result == CommandResult::Done
          && engine.acceptedArea().size() == 4);

    // 4. Вход в область, измерение
    receive(link, 0x14, Coord);
    const bool work = o.mode() == LoggerMode::Work;
    receive(link, 0x16, Id22);
    check("ID20 — «разведка»", work);
    check("ID22 — измерение ПАЕД 0,125, точка маршрута с режимом «разведка»",
          o.measurements.size() == 1 && std::fabs(o.measurements[0].paed - 0.125) < 1e-4
          && o.route.last().msgId == 0x16 && o.route.last().mode == LoggerMode::Work);
    check("Напряжение из ID22 — 12,4 В в состоянии логгера",
          std::fabs(o.status.last().battV - 12.4f) < 0.01f);

    // 5. Неизвестный ID, битый пакет — работа продолжается
    receive(link, 0x40, QByteArray::fromHex("0102"));
    const bool unknown = o.events.last().code == EventCode::UnknownId;
    QByteArray bad = DroneLink::buildPacket(0x16, Id22);
    bad[10] = char(bad[10] ^ 0xFF);
    g_time = g_time.addSecs(2);
    link.processBytes(bad, g_time);
    receive(link, 0x16, Id22);
    check("Неизвестный ID — событие в журнал", unknown);
    check("Битый пакет выброшен (ошибка CRC), следующее измерение принято",
          link.rxStats().crcErrors == 1 && o.measurements.size() == 2);

    // 6. Радиомолчание
    engine.sendCommand(cmd(0x0D));
    receive(link, 0x01);
    const bool silenceOn = o.status.last().radioSilence;
    engine.sendCommand(cmd(0x0A));
    const bool id10Rejected = o.commands.last().result == CommandResult::Rejected
                           && o.commands.last().reason == RejectReason::NotAllowed;
    engine.sendCommand(cmd(0x0E));
    const bool id14Sent = link.sentIds.last() == 0x0E;
    receive(link, 0x01);
    check("ID13 — радиомолчание включено", silenceOn);
    check("В радиомолчании ID10 не отправлена, ID14 ушла", id10Rejected && id14Sent);
    check("Ответ на ID14 — радиомолчание выключено", !o.status.last().radioSilence);

    // 7. Авария
    receive(link, 0x19, QByteArray::fromHex("01"));
    check("ID25 (LIDAR) — «авария», разрешена только ID01",
          o.mode() == LoggerMode::Error && o.allowed == QList<quint8>{1});

    // 8. Проверка области
    engine.checkArea(id04(false).vertices, false);
    engine.checkArea(id04(true).vertices, true);
    check("Проверка области: «стрелка» — невыпуклая, линия — не поддерживается",
          o.areas.size() == 2 && o.areas[0].error == AreaError::NotConvex
          && o.areas[1].error == AreaError::RouteNotSupported);

    // 9. Настройки: предел размера области 0,5 км
    CoreSettings s;
    s.maxAreaKm = 0.5;
    engine.applySettings(s);
    engine.checkArea(id04(true).vertices, false);
    check("Настройка «размер области 0,5 км» — квадрат 1 км слишком большой",
          o.areas.last().error == AreaError::TooLarge);

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
