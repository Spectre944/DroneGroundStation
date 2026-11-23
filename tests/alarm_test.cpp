/**
    @file  alarm_test.cpp
    @brief Проверка AlarmManager без окна и без логгера.

    Пороги ПАЕД (одна тревога на вход в зону), авария, ошибка SD, потеря связи,
    «режим не совпадает». В конце — через всю цепочку CoreEngine: измерение
    из «порта» → тревога с номером из базы → «Бачу» → отметка в базе.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>
#include <cstring>

#include "classes/alarm_manager.h"
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

const QDateTime T0 = QDateTime(QDate(2026, 11, 23), QTime(12, 0, 0), QTimeZone::utc());

Measurement meas(float paed, quint32 flags = MfNone)
{
    Measurement m;
    m.time = T0;
    m.paed = paed;
    m.lat = 50.45;
    m.lon = 30.52;
    m.flags = flags;
    return m;
}

LinkStatus link(LinkState s)
{
    LinkStatus l;
    l.state = s;
    return l;
}

/// Приёмник: AlarmManager и всё, что он выдал
struct Receiver {
    AlarmManager m;
    QVector<Alarm> alarms;
    QVector<CoreEvent> events;

    Receiver()
    {
        QObject::connect(&m, &AlarmManager::alarm, [this](const Alarm &a) { alarms.append(a); });
        QObject::connect(&m, &AlarmManager::event, [this](const CoreEvent &e) { events.append(e); });
    }
    /// Уровни выданных тревог: W — предупреждение, A — тревога
    QString levels() const
    {
        QString s;
        for (const Alarm &a : alarms)
            s += (a.level == AlarmLevel::Alarm ? 'A' : 'W');
        return s;
    }
};

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

/// ID22 с заданной ПАЕД (остальное — из примера протокола)
QByteArray id22(float paed)
{
    return f32(paed) + QByteArray::fromHex("5A0111E803FF0FCDCC49421B2FF441000000000066664641" "0C230A");
}

void runTests(const QString &dbPath)
{
    out << "Проверка AlarmManager" << Qt::endl;

    // 1. Пороги: 0,1 → 0,4 (W) → 0,5 (без повтора) → 1,5 (A) → 1,6 (без повтора) → 0,6 → 1,2 (A) → 0,1 → 0,4 (W)
    {
        Receiver r;
        for (float v : {0.1f, 0.4f, 0.5f, 1.5f, 1.6f, 0.6f, 1.2f, 0.1f, 0.4f})
            r.m.onMeasurement(meas(v));
        check("Пороги 0,3 / 1,0: тревоги W, A, A, W — по одной на вход в зону",
              r.levels() == "WAAW", r.levels());
        check("Тревога несёт ПАЕД и координаты измерения",
              r.alarms.size() >= 2 && qAbs(r.alarms[1].value - 1.5f) < 1e-5
              && r.alarms[1].hasPos && r.alarms[1].lat == 50.45 && r.alarms[1].type == AlarmType::DoseRate);
    }

    // 2. ПАЕД на 1 м вместо исходной
    {
        Receiver r;
        Measurement m = meas(0.1f);
        m.paed1m = 1.4f;
        m.hasPaed1m = true;
        r.m.onMeasurement(m);
        check("Сравнивается ПАЕД на 1 м (1,4), а не исходная (0,1)", r.levels() == "A");
    }

    // 3. Недостоверное измерение
    {
        Receiver r;
        r.m.onMeasurement(meas(5.0f, MfLidarRange));
        check("Недостоверное измерение (флаг Ф9) — тревоги нет", r.alarms.isEmpty());
    }

    // 4. Пороги из настроек
    {
        Receiver r;
        r.m.setThresholds(2.0f, 5.0f);
        r.m.onMeasurement(meas(1.5f));
        r.m.onMeasurement(meas(2.5f));
        check("Пороги из настроек 2 / 5: 1,5 — нет, 2,5 — предупреждение", r.levels() == "W");
    }

    // 5. Авария — одна тревога
    {
        Receiver r;
        r.m.onRoutePoint({QDateTime(), QDateTime(), 50.46, 30.53, true, 0x16, LoggerMode::Work, DataSource::Air});
        r.m.onLoggerError(0x19);
        r.m.onLoggerError(0x18);
        check("Авария ID25 — тревога «авария» с ID25 и последним положением, вторая не повторяется",
              r.alarms.size() == 1 && r.alarms[0].type == AlarmType::LoggerError && r.alarms[0].msgId == 0x19
              && r.alarms[0].level == AlarmLevel::Alarm && r.alarms[0].lat == 50.46);
    }

    // 6. Ошибка SD — одна
    {
        Receiver r;
        CoreEvent e;
        e.code = EventCode::SdFault;
        e.time = T0;
        r.m.onEvent(e);
        r.m.onEvent(e);
        check("Ошибка SD (ID16) — одно предупреждение",
              r.alarms.size() == 1 && r.alarms[0].type == AlarmType::SdError && r.levels() == "W");
    }

    // 7. Связь
    {
        Receiver r;
        r.m.onLinkStatus(link(LinkState::Ok));
        r.m.onLinkStatus(link(LinkState::Silent));      // радиомолчание — не потеря
        const bool noSilent = r.alarms.isEmpty();
        r.m.onLinkStatus(link(LinkState::Lost));
        r.m.onLinkStatus(link(LinkState::Lost));        // раз в секунду — без повтора
        const bool oneLost = r.alarms.size() == 1 && r.alarms[0].type == AlarmType::LinkLost;
        r.m.onLinkStatus(link(LinkState::Ok));
        r.m.onLinkStatus(link(LinkState::Lost));
        check("Радиомолчание — тревоги нет", noSilent);
        check("Связь потеряна — одна тревога, хотя состояние приходит каждую секунду", oneLost);
        check("Восстановление и новая потеря — снова тревога; в журнал: потеряна, восстановлена, потеряна",
              r.alarms.size() == 2 && r.events.size() == 3
              && r.events[0].code == EventCode::LinkLost && r.events[1].code == EventCode::LinkRestored
              && r.events[2].code == EventCode::LinkLost);
    }

    // 8. Режим не совпадает
    {
        Receiver r;
        CommandRequest c;
        c.id = 0x0A;
        r.m.onModeMismatch(c);
        check("Режим не совпадает — предупреждение с ID команды",
              r.alarms.size() == 1 && r.alarms[0].type == AlarmType::ModeMismatch && r.alarms[0].msgId == 0x0A);
    }

    // 9. Сброс
    {
        Receiver r;
        r.m.onMeasurement(meas(1.5f));
        r.m.onLoggerError(0x18);
        r.m.reset();
        r.m.onMeasurement(meas(1.5f));
        r.m.onLoggerError(0x18);
        check("Сброс (новый сеанс) — тревоги снова срабатывают", r.alarms.size() == 4);
    }

    // 10. Вся цепочка: «порт» → CoreEngine → база → «Бачу»
    {
        FakeLink fake;
        CoreEngine engine(&fake);
        CoreSettings s;
        s.dbPath = dbPath;
        engine.applySettings(s);
        engine.startSession("Тревоги");

        QVector<Alarm> alarms;
        QVector<qint64> acks;
        QObject::connect(&engine, &CoreEngine::alarm,             [&](const Alarm &a) { alarms.append(a); });
        QObject::connect(&engine, &CoreEngine::alarmAcknowledged, [&](qint64 id) { acks.append(id); });

        QDateTime t = T0;
        auto receive = [&](quint8 id, const QByteArray &payload) {
            t = t.addSecs(3);
            fake.processBytes(DroneLink::buildPacket(id, payload), t);
        };
        receive(0x14, QByteArray::fromHex("CDCC49421B2FF4410C2238"));   // ID20
        receive(0x16, id22(0.2f));
        receive(0x16, id22(1.5f));                                      // выше порога тревоги
        const bool doseAlarm = alarms.size() == 1 && alarms[0].type == AlarmType::DoseRate
                            && alarms[0].level == AlarmLevel::Alarm && alarms[0].id > 0;

        engine.acknowledgeAlarm(alarms.isEmpty() ? 0 : alarms[0].id);
        const qint64 sessionId = engine.store()->currentSession().id;
        const SessionData d = engine.store()->sessionDataFor(sessionId);

        receive(0x19, QByteArray::fromHex("01"));                       // ID25 LIDAR — авария

        check("CoreEngine: ID22 с ПАЕД 1,5 — тревога с номером из базы", doseAlarm);
        check("CoreEngine: «Бачу» — alarmAcknowledged, в базе отмечено",
              acks.size() == 1 && acks[0] == alarms[0].id && d.alarms.size() == 1 && d.alarms[0].acknowledged);
        check("CoreEngine: ID25 — тревога «авария» (ID25)",
              alarms.size() == 2 && alarms[1].type == AlarmType::LoggerError && alarms[1].msgId == 0x19);
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
    runTests(dir.filePath("alarm_test.db"));
    return g_failed == 0 ? 0 : 1;
}
