/**
    @file  sd_test.cpp
    @brief Проверка SdImporter без окна и без SD-карты.

    В базе во временной папке — «эфир» полёта (пакеты и спектры), рядом —
    файлы «SD» в папке 261123. Импорт сверяет, добавляет недостающее в базу
    с источником SD и выдаёт отчёт.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>
#include <cstring>

#include "classes/sd_importer.h"
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
QByteArray hms(int h, int m, int s) { return QByteArray(1, char(h)) + QByteArray(1, char(m)) + QByteArray(1, char(s)); }

/// ID19 / ID20: координата и время
QByteArray coord(int h, int m, int s) { return f32(50.45f) + f32(30.523f) + hms(h, m, s); }

/// ID22: ПАЕД и время (остальное — из примера протокола)
QByteArray meas(float paed, int h, int m, int s)
{
    return f32(paed) + QByteArray::fromHex("5A0111E803FF0F") + f32(50.45f) + f32(30.523f)
         + QByteArray(5, 0) + f32(12.4f) + hms(h, m, s);
}

/// 22 байта ID27 / заголовка итогового спектра: 23.11.2026 hh:mm, номер
QByteArray specHead(int number, int h, int m)
{
    return QByteArray("261123") + QString("%1%2").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).toLatin1()
         + u16(quint16(number)) + f32(50.45f) + f32(30.523f) + u16(1500);
}

/// Итоговый спектр на SD: заголовок + 1024 канала (во всех value, в 220 — peak)
QByteArray finalSpectrum(int number, int h, int m, quint16 value, quint16 peak)
{
    QByteArray b = specHead(number, h, m);
    for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch)
        b.append(u16(ch == 220 ? peak : value));
    return b;
}

/// Спектр эфира (сумма порций) с теми же каналами
Spectrum airSpectrum(int number, int h, int m, quint16 value, quint16 peak)
{
    Spectrum s;
    s.info.number = quint16(number);
    s.info.startTime = QDateTime(QDate(2026, 11, 23), QTime(h, m), QTimeZone::utc());
    s.info.finished = true;
    s.channels = QVector<quint16>(CoreConst::SpectrumChannels, value);
    s.channels[220] = peak;
    return s;
}

/// Строка SD: «ID,XX,XX,…»
QByteArray sdLine(quint8 id, const QByteArray &payload)
{
    QByteArray line = QByteArray::number(id, 16).rightJustified(2, '0').toUpper();
    for (char c : payload)
        line += ',' + QByteArray::number(quint8(c), 16).rightJustified(2, '0').toUpper();
    return line + "\r\n";
}

Packet packet(quint8 id, const QByteArray &payload)
{
    Packet p;
    p.id = id;
    p.payload = payload;
    p.rxTime = QDateTime(QDate(2026, 11, 23), QTime(12, 0), QTimeZone::utc());
    return p;
}

void writeFile(const QString &path, const QByteArray &content)
{
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(content);
}

void runTests(const QString &root)
{
    out << "Проверка SdImporter (обработка SD)" << Qt::endl;

    SessionStore store;
    store.open(root + "/sd_test.db");
    store.startSession("Полёт с SD");
    const qint64 sessionId = store.currentSession().id;

    // ---- «эфир» полёта ----
    store.savePacket(packet(0x14, coord(12, 34, 56)));          // ID20
    store.savePacket(packet(0x16, meas(0.12f, 12, 35, 10)));    // ID22 m1
    store.savePacket(packet(0x16, meas(0.30f, 12, 35, 30)));    // ID22 m3 (m2 потерян в эфире)
    store.savePacket(packet(0x1B, specHead(1, 12, 36)));        // ID27 спектра № 1
    store.savePacket(packet(0x1B, specHead(2, 12, 38)));        // ID27 спектра № 2
    store.savePacket(packet(0x13, coord(12, 40, 0)));           // ID19 в возвращении — на SD не пишется
    store.saveSpectrum(airSpectrum(1, 12, 36, 10, 500));        // сумма порций № 1 — как на SD
    store.saveSpectrum(airSpectrum(2, 12, 38, 10, 300));        // № 2 — порция потерялась: на SD 400
    store.finishSession();                                      // полёт закончен до импорта SD
    const QDateTime endBefore = store.sessions().first().endTime;

    // ---- файлы SD ----
    QDir(root).mkpath("261123");
    const QString file1 = root + "/261123/123400.CSV";
    const QString file2 = root + "/261123/235950.CSV";
    writeFile(file1,
        sdLine(0x14, coord(12, 34, 56)) +                       // совпадёт
        sdLine(0x16, meas(0.12f, 12, 35, 10)) +                 // совпадёт
        sdLine(0x16, meas(0.20f, 12, 35, 20)) +                 // только на SD
        sdLine(0x16, meas(0.30f, 12, 35, 30)) +                 // совпадёт
        sdLine(0x17, finalSpectrum(1, 12, 36, 10, 500)) +       // совпадёт с суммой эфира
        sdLine(0x17, finalSpectrum(2, 12, 38, 10, 400)) +       // расхождение
        sdLine(0x17, finalSpectrum(3, 12, 39, 5, 50)) +         // только на SD (радиомолчание)
        "16,ZZ,00\r\n" +                                        // не HEX
        "40,01\r\n" +                                           // ID на SD не пишется
        "16,00,01\r\n" +                                        // неверная длина
        sdLine(0x1C, QByteArray(1, 0x01)));                     // ID28 — только на SD
    writeFile(file2,
        sdLine(0x16, meas(0.15f, 23, 59, 58)) +                 // до полуночи
        sdLine(0x16, meas(0.16f, 0, 0, 3)));                    // после — дата +1

    SdImporter importer(&store);
    QVector<int> progress;
    QObject::connect(&importer, &SdImporter::progress, [&](int p) { progress.append(p); });

    const SdReport r = importer.importFiles({file2, file1});    // порядок не важен

    check("Отчёт: 2 файла, по порядку папки и имени", r.files.size() == 2 && r.files[0] == file1);
    check("Совпало 4: ID20, два ID22, спектр № 1", r.matched == 4, QString::number(r.matched));
    check("Только на SD 5: ID22, ID28, спектр № 3, два ID22 у полуночи", r.onlySd == 5, QString::number(r.onlySd));
    check("Только в эфире 1: ID19 в возвращении", r.onlyAir == 1, QString::number(r.onlyAir));
    check("Расхождение в спектре 1: № 2", r.spectraMismatch == 1, QString::number(r.spectraMismatch));
    check("Ошибочных строк 3: не HEX, чужой ID, неверная длина", r.badLines == 3, r.details.join("; "));
    check("Ход: от 0 до 100 %", !progress.isEmpty() && progress.first() == 0 && progress.last() == 100);

    // ---- что добавлено в базу ----
    const SessionData d = store.sessionDataFor(sessionId);
    int sdMeas = 0;
    QDateTime beforeMidnight, afterMidnight;
    for (const Measurement &m : d.measurements) {
        if (m.source == DataSource::Sd)
            ++sdMeas;
        if (qAbs(m.paed - 0.15f) < 1e-5) beforeMidnight = m.time;
        if (qAbs(m.paed - 0.16f) < 1e-5) afterMidnight = m.time;
    }
    int sdSpectra = 0;
    for (const Spectrum &s : d.spectra)
        if (s.info.source == DataSource::Sd)
            ++sdSpectra;

    check("В базу добавлено 3 измерения с источником SD", sdMeas == 3 && d.measurements.size() == 3);
    check("Дата из папки 261123 и полночь: 23.11 23:59:58 и 24.11 00:00:03",
          beforeMidnight == QDateTime(QDate(2026, 11, 23), QTime(23, 59, 58), QTimeZone::utc())
          && afterMidnight == QDateTime(QDate(2026, 11, 24), QTime(0, 0, 3), QTimeZone::utc()),
          beforeMidnight.toString(Qt::ISODate) + " / " + afterMidnight.toString(Qt::ISODate));
    check("Спектры с SD добавлены: № 2 (основной — с SD) и № 3, всего в полёте 4",
          sdSpectra == 2 && d.spectra.size() == 4);
    check("Спектр № 3 с SD: канал 220 = 50, 23.11.2026 12:39",
          [&] { for (const Spectrum &s : d.spectra)
                    if (s.info.number == 3)
                        return s.channels[220] == 50 && s.info.startTime == QDateTime(QDate(2026, 11, 23), QTime(12, 39), QTimeZone::utc());
                return false; }());
    check("ID28 с SD — событие «спектр не запустился» с источником SD",
          d.events.size() == 1 && d.events[0].code == EventCode::SpectrumStartFailed && d.events[0].source == DataSource::Sd);
    check("Полёт был закончен — данные добавлены в него, время окончания не изменилось",
          store.sessions().first().endTime == endBefore && !store.hasSession());

    store.close();

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

    QTemporaryDir dir;              // временная папка для базы и «SD» — удаляется после проверки
    runTests(dir.path());
    return g_failed == 0 ? 0 : 1;
}
