/**
    @file  csv_test.cpp
    @brief Проверка CsvExporter (экспорт CSV) без окна.

    Полёт собирается в памяти, выгружается во временную папку, файлы читаются
    обратно: BOM, разделитель «;», десятичная запятая или точка, заголовки
    на украинском или английском, число строк, спектр — 1024 канала.
    В конце — через CoreEngine: полёт из базы → файлы → exportFinished.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>

#include "classes/csv_exporter.h"
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

/// Тестовый полёт: 3 измерения, 2 точки маршрута, 1 спектр, событие, команда, тревога
SessionData flight()
{
    SessionData d;
    d.info.id = 7;
    d.info.name = "Поле: Київ/Північ";          // «:» и «/» в имени файла недопустимы
    for (int i = 0; i < 3; ++i) {
        Measurement m;
        m.time = T0.addSecs(i * 10);
        m.paed = 0.125f + i;
        m.lat = 50.45;
        m.lon = 30.523;
        m.lidarCm = 1550;
        m.battV = 12.4f;
        m.flags = (i == 2) ? (MfOutOfArea | MfLidarRange) : MfNone;
        m.source = (i == 1) ? DataSource::Sd : DataSource::Air;
        if (i == 0) {
            m.paed1m = 0.14f;
            m.hasPaed1m = true;
        }
        d.measurements.append(m);
    }
    for (int i = 0; i < 2; ++i) {
        RoutePoint r;
        r.time = T0.addSecs(i);
        r.lat = 50.45;
        r.lon = 30.52;
        r.inArea = (i == 1);
        r.msgId = (i == 1) ? 0x14 : 0x13;
        d.route.append(r);
    }
    Spectrum s;
    s.info.number = 3;
    s.info.startTime = T0;
    s.info.portions = 8;
    s.info.stopReason = SpectrumStopReason::TimeElapsed;
    s.channels[220] = 600;
    d.spectra.append(s);

    CoreEvent e;
    e.code = EventCode::BatteryOk;
    e.time = T0.addSecs(5);
    e.params = {{"voltage", 15.6}};
    d.events.append(e);

    CommandOutcome c;
    c.request.id = 4;
    c.result = CommandResult::Done;
    c.sentTime = T0.addSecs(2);
    d.commands.append(c);

    Alarm a;
    a.type = AlarmType::DoseRate;
    a.level = AlarmLevel::Alarm;
    a.time = T0.addSecs(20);
    a.value = 2.125f;
    a.acknowledged = true;
    d.alarms.append(a);
    return d;
}

/// Прочитать файл: первые байты и строки
QByteArray readAll(const QString &path)
{
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    return f.readAll();
}
QStringList lines(const QString &path)
{
    QByteArray b = readAll(path);
    if (b.startsWith("\xEF\xBB\xBF"))
        b.remove(0, 3);
    QStringList l = QString::fromUtf8(b).split("\r\n");
    if (!l.isEmpty() && l.last().isEmpty())
        l.removeLast();
    return l;
}

// ---- для проверки через CoreEngine ----
class FakeLink : public DroneLink
{
public:
    SendResult send(quint8, const QByteArray &) override { return SendResult::Ok; }
    bool canSend() const override { return true; }
    void answerReceived() override {}
};

void runTests(const QString &root)
{
    out << "Проверка CsvExporter (экспорт CSV)" << Qt::endl;

    // 1. Все четыре вида, по-украински, с запятой
    {
        CsvExporter x;
        CoreSettings s;                                 // по умолчанию: «,» и uk
        const QString dir = root + "/uk";
        const bool ok = x.exportSession(flight(), dir, ExAll, s);
        const QString base = dir + "/flight_7_Поле__Київ_Північ";

        check("Записано 4 файла, имя без недопустимых символов",
              ok && x.files().size() == 4 && QFile::exists(base + "_measurements.csv"),
              x.files().join(" | ") + " " + x.error());

        const QStringList m = lines(base + "_measurements.csv");
        check("Измерения: BOM в начале файла", readAll(base + "_measurements.csv").startsWith("\xEF\xBB\xBF"));
        check("Измерения: заголовок по-украински и 3 строки",
              m.size() == 4 && m[0].startsWith("Дата і час UTC;Широта;Довгота"), m.value(0));
        check("Измерения: десятичная запятая, ПАЕД 0,1250, на 1 м 0,1400, высота 15,50 м",
              m.size() > 1 && m[1].contains(";0,1250;0,1400;") && m[1].contains(";15,50;") && m[1].contains("50,450000"),
              m.value(1));
        check("Измерения: источник SD, недостоверное — «поза областю, LIDAR»",
              m.size() > 3 && m[2].endsWith(";SD") && m[3].contains("поза областю, LIDAR"), m.value(3));

        const QStringList t = lines(base + "_track.csv");
        check("Маршрут: 2 точки, «ні» / «так», ID19 / ID20",
              t.size() == 3 && t[1].contains(";ні;ID19;") && t[2].contains(";так;ID20;"));

        const QStringList sp = lines(base + "_spectrum_3.csv");
        check("Спектр № 3: сведения + 1024 канала, канал 220 = 600",
              sp.size() == 8 + 2 + 1024 && sp.contains("220;600") && sp[0] == "Номер спектра;3", QString::number(sp.size()));

        const QStringList e = lines(base + "_events.csv");
        check("События: команда, событие, тревога — по времени",
              e.size() == 4 && e[1].contains("команда;ID4") && e[2].contains("подія;0x05")
              && e[3].contains("тривога") && e[3].contains("ПАЕД 2,125"), e.join(" | "));
    }

    // 2. По-английски, с точкой, только измерения
    {
        CsvExporter x;
        CoreSettings s;
        s.csvDecimalSep = QLatin1Char('.');
        s.language = "en";
        const QString dir = root + "/en";
        const bool ok = x.exportSession(flight(), dir, ExMeasurements, s);
        const QStringList m = lines(x.files().value(0));
        check("Только измерения — один файл", ok && x.files().size() == 1);
        check("По-английски и с точкой: «Date and time UTC», 0.1250",
              m.size() == 4 && m[0].startsWith("Date and time UTC;Latitude") && m[1].contains(";0.1250;"), m.value(1));
    }

    // 3. Папка недоступна
    {
        CsvExporter x;
        QFile blocker(root + "/blocker");
        blocker.open(QIODevice::WriteOnly);
        blocker.close();
        const bool ok = x.exportSession(flight(), root + "/blocker/sub", ExAll, CoreSettings());
        check("Папку создать нельзя — ошибка с причиной", !ok && !x.error().isEmpty());
    }

    // 4. Через CoreEngine: полёт из базы → файлы
    {
        FakeLink link;
        CoreEngine engine(&link);
        CoreSettings s;
        s.dbPath = root + "/csv_test.db";
        engine.applySettings(s);
        engine.startSession("Експорт");
        const qint64 id = engine.store()->currentSession().id;
        for (const Measurement &m : flight().measurements)
            engine.store()->saveMeasurement(m);

        QVector<QPair<bool, QString>> done;
        QObject::connect(&engine, &CoreEngine::exportFinished, [&](bool ok, const QString &p) { done.append({ok, p}); });
        engine.exportCsv(id, root + "/engine", ExMeasurements | ExTrack);

        const QStringList files = QDir(root + "/engine").entryList(QDir::Files);
        check("CoreEngine: exportFinished(true, папка), записаны измерения и маршрут",
              done.size() == 1 && done[0].first && done[0].second == root + "/engine" && files.size() == 2,
              files.join(", "));
        check("CoreEngine: в файле измерений 3 строки из базы",
              lines(root + "/engine/" + CsvExporter::fileBase(engine.store()->currentSession()) + "_measurements.csv").size() == 4);
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

    QTemporaryDir dir;              // временная папка — удаляется после проверки
    runTests(dir.path());
    return g_failed == 0 ? 0 : 1;
}
