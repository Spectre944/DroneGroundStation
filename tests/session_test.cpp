/**
    @file  session_test.cpp
    @brief Проверка SessionStore (база полётов) без окна.

    База создаётся во временной папке и удаляется после проверки.
    Записывается полёт, читается обратно и сравнивается поле в поле;
    поиск по архиву; база закрывается и открывается заново — данные на месте.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>
#include <cmath>

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

bool approx(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

const QDateTime T0 = QDateTime(QDate(2026, 11, 23), QTime(10, 0, 0), QTimeZone::utc());

/// Квадрат ~1 км под Киевом
const QVector<GeoPoint> Square = { {50.445, 30.515}, {50.445, 30.529}, {50.454, 30.529}, {50.454, 30.515} };

/// Измерение № i: внутри квадрата, ПАЕД растёт 0,1 · i
Measurement measurementAt(int i, bool inside = true)
{
    Measurement m;
    m.time = m.rxTime = T0.addSecs(i * 10);
    m.paed = float(0.1 * i);
    m.tempCode = 0x015A;
    m.bdbgStat = 0x11;
    m.lidarCm = quint16(1000 + i);
    m.lidarStrength = 0x0FFF;
    m.lat = inside ? 50.446 + 0.001 * i : 50.47;   // снаружи — 2 км севернее
    m.lon = 30.516 + 0.001 * i;
    m.battV = 15.5f;
    m.flags = (i == 3) ? MfLidarRange : MfNone;
    return m;
}

/// Приёмник сигналов SessionStore
struct Receiver {
    SessionStore store;
    QVector<SessionInfo> started, finished, listed;
    SessionData data;
    ArchiveResult archive;
    QVector<Measurement> series;
    QStringList errors;

    Receiver()
    {
        QObject::connect(&store, &SessionStore::sessionStarted,  [this](const SessionInfo &s) { started.append(s); });
        QObject::connect(&store, &SessionStore::sessionFinished, [this](const SessionInfo &s) { finished.append(s); });
        QObject::connect(&store, &SessionStore::sessionsListed,  [this](const QVector<SessionInfo> &l) { listed = l; });
        QObject::connect(&store, &SessionStore::sessionData,     [this](const SessionData &d) { data = d; });
        QObject::connect(&store, &SessionStore::archiveResult,   [this](const ArchiveResult &r) { archive = r; });
        QObject::connect(&store, &SessionStore::timeSeriesReady, [this](qint64, const QVector<Measurement> &s) { series = s; });
        QObject::connect(&store, &SessionStore::dbError,         [this](const QString &t) { errors << t; });
    }
};

void runTests(const QString &dbPath)
{
    out << "Проверка SessionStore (база полётов)" << Qt::endl;

    qint64 flight1 = 0, flight2 = 0;
    {
        Receiver r;

        // 1. База открыта, таблицы созданы, журнал WAL
        const bool opened = r.store.open(dbPath);
        QSqlQuery q(QSqlDatabase::database(QSqlDatabase::connectionNames().last()));
        QString journal;
        if (q.exec("PRAGMA journal_mode") && q.next())
            journal = q.value(0).toString();
        int tables = 0;
        if (q.exec("SELECT COUNT(*) FROM sqlite_master WHERE type='table'") && q.next())
            tables = q.value(0).toInt();
        check("База открыта, 12 таблиц, журнал WAL", opened && tables == 12 && journal == "wal",
              QString("таблиц %1, журнал %2").arg(tables).arg(journal));

        // 2. Новый полёт и запись всего
        r.store.startSession("Тест: поле под Киевом");
        flight1 = r.store.currentSession().id;

        QVector<qint64> mIds;
        for (int i = 1; i <= 5; ++i)
            mIds.append(r.store.saveMeasurement(measurementAt(i)));
        r.store.saveMeasurement(measurementAt(6, false));      // снаружи области

        RoutePoint rp;
        rp.time = rp.rxTime = T0;
        rp.lat = 50.446; rp.lon = 30.516; rp.inArea = true; rp.msgId = 0x14; rp.mode = LoggerMode::Work;
        r.store.saveRoutePoint(rp);

        Packet p;
        p.id = 0x16; p.payload = QByteArray::fromHex("0000003E"); p.rxTime = T0;
        r.store.savePacket(p);
        r.store.saveRaw(false, QByteArray::fromHex("AA55AA55"), T0);

        CoreEvent e;
        e.code = EventCode::BatteryOk; e.time = T0; e.params = {{"voltage", 15.6}};
        const qint64 eventId = r.store.saveEvent(e);

        CommandOutcome c;
        c.request.id = 4; c.request.vertices = Square; c.request.reconMode = ReconMode::Distance;
        c.request.intervalS = 30; c.request.distanceM = 100; c.request.dupFactor = 3; c.request.seq = 7;
        c.result = CommandResult::Done; c.sentTime = T0; c.finishTime = T0.addSecs(1); c.attempts = 1;
        r.store.saveCommand(c);
        r.store.saveArea(c.request, c.finishTime);

        Alarm a;
        a.type = AlarmType::DoseRate; a.level = AlarmLevel::Alarm; a.time = T0; a.hasPos = true;
        a.lat = 50.45; a.lon = 30.52; a.value = 1.5f;
        const qint64 alarmId = r.store.saveAlarm(a);
        r.store.setAlarmAcknowledged(alarmId);

        Spectrum s;
        s.info.number = 1; s.info.startTime = T0; s.info.lat = 50.45; s.info.lon = 30.52;
        s.info.portions = 2; s.info.finished = true; s.info.stopReason = SpectrumStopReason::TimeElapsed;
        s.channels[220] = 600; s.channels[1023] = 65535;
        SpectrumPortion portion;
        portion.channels = QVector<quint16>(CoreConst::SpectrumChannels, 3);
        portion.accTime = 6; portion.paed = 0.5f;
        s.portionList = {portion, portion};
        r.store.saveSpectrum(s);

        r.store.setNoSd(true);
        r.store.finishSession();

        check("Новый полёт записан: 6 измерений с номерами по порядку, событие, тревога",
              flight1 > 0 && mIds.size() == 5 && mIds[0] > 0 && mIds[4] == mIds[0] + 4
              && eventId > 0 && alarmId > 0 && r.errors.isEmpty(), r.errors.join("; "));
        check("Полёт закончен: время окончания, 6 измерений",
              r.finished.size() == 1 && r.finished[0].endTime.isValid() && r.finished[0].measurementCount == 6);

        // 3. Автоматический полёт: данные без «Нового полёта»
        r.store.saveMeasurement(measurementAt(1));
        flight2 = r.store.currentSession().id;
        check("Данные без «Нового полёта» — полёт начат сам, имя по дате и времени",
              flight2 > flight1 && r.started.size() == 2 && r.started[1].name.endsWith("UTC"),
              r.started.size() > 1 ? r.started[1].name : QString());

        // 4. Список полётов
        r.store.listSessions();
        bool listOk = r.listed.size() == 2;
        const SessionInfo *f1 = nullptr;
        for (const SessionInfo &si : r.listed)
            if (si.id == flight1)
                f1 = &si;
        listOk = listOk && f1 && f1->name == "Тест: поле под Киевом" && f1->measurementCount == 6 && f1->noSd;
        check("Список полётов: 2 полёта, имя, 6 измерений, «без SD»", listOk);

        // 5. Открыть полёт: прочитанное = записанному
        r.store.loadSession(flight1);
        const SessionData &d = r.data;
        bool mOk = d.measurements.size() == 6;
        if (mOk) {
            const Measurement src = measurementAt(3), &m = d.measurements[2];
            mOk = m.id == mIds[2] && m.time == src.time && approx(m.paed, src.paed, 1e-5)
               && m.tempCode == src.tempCode && m.bdbgStat == src.bdbgStat && m.lidarCm == src.lidarCm
               && approx(m.lat, src.lat) && approx(m.lon, src.lon) && approx(m.battV, 15.5, 1e-5)
               && m.flags == MfLidarRange;
        }
        check("Открыт полёт: измерения поле в поле, номера совпадают", mOk);
        check("Маршрут: точка ID20 в режиме «разведка»",
              d.route.size() == 1 && d.route[0].msgId == 0x14 && d.route[0].mode == LoggerMode::Work && d.route[0].inArea);
        check("Событие: «аккумулятор заряжен», 15,6 В",
              d.events.size() == 1 && d.events[0].code == EventCode::BatteryOk
              && approx(d.events[0].params.value("voltage").toDouble(), 15.6));
        check("Команда ID04: 4 вершины, «по расстоянию», № 7, «выполнено»",
              d.commands.size() == 1 && d.commands[0].request.vertices.size() == 4
              && d.commands[0].request.reconMode == ReconMode::Distance && d.commands[0].request.seq == 7
              && d.commands[0].result == CommandResult::Done);
        check("Область из принятой ID04 — 4 вершины",
              d.areas.size() == 1 && d.areas[0].size() == 4 && approx(d.areas[0][2].lat, 50.454));
        check("Тревога: ПАЕД 1,5, «видел»",
              d.alarms.size() == 1 && approx(d.alarms[0].value, 1.5) && d.alarms[0].acknowledged);
        check("Спектр: каналы 220 = 600, 1023 = 65535, 2 порции по 1024 канала",
              d.spectra.size() == 1 && d.spectra[0].channels[220] == 600 && d.spectra[0].channels[1023] == 65535
              && d.spectra[0].portionList.size() == 2 && d.spectra[0].portionList[1].channels.size() == 1024
              && d.spectra[0].info.stopReason == SpectrumStopReason::TimeElapsed);
        check("Открытый полёт — «только просмотр»", d.info.readOnly && d.info.id == flight1);

        // 6. Поиск по архиву
        r.store.setThresholds(0.3f, 0.45f);
        SessionFilter f;
        f.sessionId = flight1;
        r.store.query(f);
        const int all = r.archive.points.size();            // 6 измерений + 1 спектр

        f.area = Square;
        f.types = ApMeasurement;
        r.store.query(f);
        const int inside = r.archive.points.size();

        f.types = ApAlarm;
        r.store.query(f);
        const int alarms = r.archive.points.size();         // ПАЕД ≥ 0,45: № 5 (0,5)

        SessionFilter t;
        t.sessionId = flight1;
        t.from = T0.addSecs(20);
        t.to = T0.addSecs(40);
        t.types = ApMeasurement;
        r.store.query(t);
        const int byTime = r.archive.points.size();         // № 2, 3, 4

        check("Поиск по полёту: 6 измерений + 1 спектр", all == 7, QString::number(all));
        check("Поиск по области: 5 внутри, 1 снаружи не найдена", inside == 5, QString::number(inside));
        check("Поиск «выше порога тревоги 0,45»: 1 точка", alarms == 1, QString::number(alarms));
        check("Поиск по времени 10:00:20–10:00:40: 3 точки", byTime == 3, QString::number(byTime));

        // Графики по времени
        r.store.timeSeries(flight1);
        check("Данные для графиков по времени: 6 измерений", r.series.size() == 6);

        r.store.close();
    }

    // 7. Открыть заново: данные на месте
    {
        Receiver r;
        r.store.open(dbPath);
        r.store.listSessions();
        r.store.loadSession(flight1);
        check("База открыта заново: 2 полёта, 6 измерений первого полёта",
              r.listed.size() == 2 && r.data.measurements.size() == 6);
        check("Незаконченный автоматический полёт закончен при закрытии базы",
              [&] { for (const SessionInfo &s : r.listed) if (s.id == flight2) return s.endTime.isValid(); return false; }());
        r.store.close();
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

    QTemporaryDir dir;              // временная папка — удаляется вместе с базой после проверки
    runTests(dir.filePath("test_flights.db"));
    return g_failed == 0 ? 0 : 1;
}
