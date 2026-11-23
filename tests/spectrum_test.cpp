/**
    @file  spectrum_test.cpp
    @brief Проверка SpectrumCollector без окна и без логгера.

    Сумма порций, насыщение 65535, остановка по 55705, причины конца,
    таймер заданного времени, порция без ID27. В конце — через всю цепочку
    CoreEngine: байты ID27 и ID23 из «порта» → спектр собран, записан в базу,
    прочитан обратно и совпал.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QTimer>
#include <QVector>
#include <cstring>

#include "classes/spectrum_collector.h"
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

void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

const QDateTime T0 = QDateTime(QDate(2026, 11, 23), QTime(12, 0, 0), QTimeZone::utc());

/// Порция: во всех каналах value, в канале 220 — peak
SpectrumPortion portion(quint16 value, quint16 peak = 0)
{
    SpectrumPortion p;
    p.channels = QVector<quint16>(CoreConst::SpectrumChannels, value);
    if (peak)
        p.channels[220] = peak;
    p.accTime = 6;
    p.paed = 0.5f;
    p.stat = 0x11;
    p.rxTime = T0;
    return p;
}

SpectrumInfo start(quint16 number)
{
    SpectrumInfo i;
    i.number = number;
    i.startTime = T0;
    i.lat = 50.45;
    i.lon = 30.52;
    i.lidarCm = 1500;
    return i;
}

/// Приёмник: SpectrumCollector и всё, что он выдал
struct Receiver {
    SpectrumCollector c;
    QVector<Spectrum> updates, finished;
    QVector<SpectrumStopReason> selfStops;

    Receiver()
    {
        QObject::connect(&c, &SpectrumCollector::spectrumUpdated,  [this](const Spectrum &s) { updates.append(s); });
        QObject::connect(&c, &SpectrumCollector::spectrumFinished, [this](const Spectrum &s) { finished.append(s); });
        QObject::connect(&c, &SpectrumCollector::stoppedByItself,  [this](SpectrumStopReason r) { selfStops.append(r); });
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

QByteArray u16(quint16 v) { return QByteArray(1, char(v & 0xFF)) + QByteArray(1, char(v >> 8)); }
QByteArray f32(float v)
{
    quint32 bits;
    std::memcpy(&bits, &v, 4);
    QByteArray b;
    for (int i = 0; i < 4; ++i)
        b.append(char((bits >> (8 * i)) & 0xFF));
    return b;
}

/// Байты ID23: во всех каналах value, в канале 220 — peak
QByteArray id23(quint16 value, quint16 peak)
{
    QByteArray b;
    for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch)
        b.append(u16(ch == 220 ? peak : value));
    return b + u16(6) + f32(0.5f) + QByteArray(1, char(0x11));
}

void runTests(const QString &dbPath)
{
    out << "Проверка SpectrumCollector" << Qt::endl;

    // 1. Сумма трёх порций
    {
        Receiver r;
        r.c.onStart(start(1));
        r.c.onPortion(portion(10, 100));
        r.c.onPortion(portion(20, 200));
        r.c.onPortion(portion(30, 300));
        const Spectrum &s = r.c.current();
        check("Сумма 3 порций: каналы 60, канал 220 = 600, порций 3",
              s.channels[0] == 60 && s.channels[1023] == 60 && s.channels[220] == 600 && s.info.portions == 3);
        check("В spectrumUpdated — только последняя порция (начало + 3 порции = 4 сигнала)",
              r.updates.size() == 4 && r.updates[0].portionList.isEmpty()
              && r.updates[3].portionList.size() == 1 && r.updates[3].portionList[0].channels[0] == 30);

        r.c.finish(SpectrumStopReason::Measurement);
        check("Конец по ID22: в spectrumFinished все 3 порции, причина «измерение», набор окончен",
              r.finished.size() == 1 && r.finished[0].portionList.size() == 3
              && r.finished[0].info.stopReason == SpectrumStopReason::Measurement
              && r.finished[0].info.finished && !r.c.isCollecting());
    }

    // 2. Насыщение 65535 и остановка по 55705
    {
        Receiver r;
        r.c.onStart(start(2));
        r.c.onPortion(portion(1, 40000));
        const bool going = r.c.isCollecting();
        r.c.onPortion(portion(1, 40000));                   // 220: 80000 → 65535
        check("Канал 220: 40000 — набор идёт", going);
        check("Канал 220: 40000 + 40000 → 65535 (насыщение), набор остановлен «канал заполнен»",
              r.finished.size() == 1 && r.finished[0].channels[220] == 65535
              && r.finished[0].info.stopReason == SpectrumStopReason::ChannelFull
              && r.selfStops == QVector<SpectrumStopReason>{SpectrumStopReason::ChannelFull});
    }

    // 3. Новый ID27 при незакрытом наборе; NewSpectrum от LoggerState не закрывает новый
    {
        Receiver r;
        r.c.onStart(start(3));
        r.c.onPortion(portion(5));
        r.c.onStart(start(4));
        r.c.finish(SpectrumStopReason::NewSpectrum);        // приходит от LoggerState позже
        check("Новый ID27: спектр № 3 закрыт «новый спектр», № 4 набирается",
              r.finished.size() == 1 && r.finished[0].info.number == 3
              && r.finished[0].info.stopReason == SpectrumStopReason::NewSpectrum
              && r.c.isCollecting() && r.c.current().info.number == 4);
    }

    // 4. Порция без ID27
    {
        Receiver r;
        r.c.onPortion(portion(7));
        check("Порция без ID27 — спектр без номера, порция учтена",
              r.c.isCollecting() && r.c.current().info.number == 0 && r.c.current().channels[0] == 7);
    }

    // 5. finish без набора — ничего
    {
        Receiver r;
        r.c.finish(SpectrumStopReason::CommandStop);
        check("Конец без набора — ничего не выдаётся", r.finished.isEmpty());
    }

    // 6. Заданное время: 1 с + запас 0,2 с
    {
        Receiver r;
        r.c.setTimeMarginMs(200);
        r.c.setDuration(1);
        r.c.onStart(start(5));
        r.c.onPortion(portion(3));
        wait(700);
        const bool stillGoing = r.c.isCollecting();
        wait(900);
        check("Время набора 1 с: через 0,7 с набор идёт", stillGoing);
        check("Через 1,6 с — набор окончен «истекло время», сообщено LoggerState",
              r.finished.size() == 1 && r.finished[0].info.stopReason == SpectrumStopReason::TimeElapsed
              && r.finished[0].info.setDurationS == 1
              && r.selfStops == QVector<SpectrumStopReason>{SpectrumStopReason::TimeElapsed});
    }

    // 7. Время 0 — до ID12, таймер не закрывает
    {
        Receiver r;
        r.c.setTimeMarginMs(0);
        r.c.setDuration(0);
        r.c.onStart(start(6));
        wait(300);
        check("Время набора 0 (до ID12) — таймер набор не закрывает", r.c.isCollecting());
    }

    // 8. Вся цепочка: «порт» → CoreEngine → база
    {
        FakeLink link;
        CoreEngine engine(&link);
        CoreSettings s;
        s.dbPath = dbPath;
        engine.applySettings(s);
        engine.startSession("Спектр");

        QVector<Spectrum> finished;
        QVector<LoggerStatus> status;
        QObject::connect(&engine, &CoreEngine::spectrumFinished, [&](const Spectrum &sp) { finished.append(sp); });
        QObject::connect(&engine, &CoreEngine::loggerStatusChanged, [&](const LoggerStatus &st) { status.append(st); });

        QDateTime t = T0;
        auto receive = [&](quint8 id, const QByteArray &payload) {
            t = t.addSecs(6);
            link.processBytes(DroneLink::buildPacket(id, payload), t);
        };

        receive(0x14, QByteArray::fromHex("CDCC49421B2FF4410C2238"));          // ID20 — разведка
        receive(0x1B, QByteArray("2611231200") + u16(9) + f32(50.45f) + f32(30.523f) + u16(1500));  // ID27, № 9
        const bool collecting = !status.isEmpty() && status.last().spectrumStage == SpectrumStage::Collecting;
        receive(0x17, id23(10, 100));
        receive(0x17, id23(20, 200));
        receive(0x16, QByteArray::fromHex("0000003E5A0111E803FF0FCDCC49421B2FF441000000000066664641" "0C230A"));  // ID22

        const qint64 sessionId = engine.store()->currentSession().id;
        const SessionData d = engine.store()->sessionDataFor(sessionId);

        check("CoreEngine: ID27 — состояние «идёт набор»", collecting);
        check("CoreEngine: ID22 — спектр № 9 окончен «измерение», 2 порции, канал 220 = 300",
              finished.size() == 1 && finished[0].info.number == 9 && finished[0].info.portions == 2
              && finished[0].channels[220] == 300 && finished[0].channels[0] == 30
              && finished[0].info.stopReason == SpectrumStopReason::Measurement);
        check("CoreEngine: спектр записан в базу (номер записи), «идёт набор» снят",
              finished.size() == 1 && finished[0].info.id > 0
              && status.last().spectrumStage == SpectrumStage::None);
        check("База: спектр прочитан обратно — № 9, канал 220 = 300, 2 порции",
              d.spectra.size() == 1 && d.spectra[0].info.number == 9 && d.spectra[0].channels[220] == 300
              && d.spectra[0].portionList.size() == 2 && d.spectra[0].portionList[1].channels[220] == 200);
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
    runTests(dir.filePath("spectrum_test.db"));
    return g_failed == 0 ? 0 : 1;
}
