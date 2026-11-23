#include "sd_importer.h"
#include "session_store.h"
#include "message_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimeZone>
#include <algorithm>

/**
    @brief Что бывает на SD (протокол, п. 10.4).
*/
namespace {
    constexpr quint8 IdCoordOut   = 0x13;   ///< ID19
    constexpr quint8 IdCoordIn    = 0x14;   ///< ID20
    constexpr quint8 IdMeasure    = 0x16;   ///< ID22
    constexpr quint8 IdSpectrum   = 0x17;   ///< ID23 — на SD итоговый спектр
    constexpr quint8 IdDevice     = 0x19;   ///< ID25
    constexpr quint8 IdSpecFail   = 0x1C;   ///< ID28
    constexpr quint8 IdSpecStart  = 0x1B;   ///< ID27 — в эфире; его 22 байта = начало итогового спектра на SD

    constexpr int LenFinalSpectrum = 2070;  ///< 22 байта заголовка + 1024 канала × 2
    constexpr int LenSpecHeader    = 22;

    /// Пакеты, которые сверяются байт в байт
    bool isComparable(quint8 id)
    {
        return id == IdCoordOut || id == IdCoordIn || id == IdMeasure || id == IdDevice || id == IdSpecFail;
    }

    /// Положенная длина данных на SD; −1 — такой ID на SD не пишется
    int sdLength(quint8 id)
    {
        switch (id) {
        case IdCoordOut: case IdCoordIn: return 11;
        case IdMeasure:   return 31;
        case IdDevice:    return 1;
        case IdSpecFail:  return 1;
        case IdSpectrum:  return LenFinalSpectrum;
        default:          return -1;
        }
    }

    /// Итоговый спектр с SD → Spectrum (номер, дата, точка, высота, каналы)
    bool spectrumFromSd(const Packet &p, Spectrum &s)
    {
        // Заголовок — те же 22 байта, что ID27: разбираем его готовым MessageParser
        MessageParser parser;
        bool ok = false;
        QObject::connect(&parser, &MessageParser::spectrumStarted, [&](const SpectrumInfo &info) {
            s.info = info;
            ok = true;
        });
        Packet head;
        head.id = IdSpecStart;
        head.payload = p.payload.left(LenSpecHeader);
        head.rxTime = p.rxTime;
        parser.parse(head);
        if (!ok)
            return false;

        for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch) {
            const int at = LenSpecHeader + ch * 2;
            s.channels[ch] = quint16(quint8(p.payload[at]) | (quint8(p.payload[at + 1]) << 8));
        }
        s.info.finished = true;
        s.info.source = DataSource::Sd;
        return true;
    }
}

SdImporter::SdImporter(SessionStore *store, QObject *parent)
    : QObject(parent), m_store(store)
{
}

// ============================================================================
//  Импорт
// ============================================================================

SdReport SdImporter::importFiles(const QStringList &files)
{
    SdReport report;
    emit progress(0);

    // ---- куда писать: текущий полёт или последний ----
    const bool hadSession = m_store->hasSession();
    const qint64 sessionId = hadSession ? m_store->currentSession().id : m_store->lastSessionId();
    if (sessionId == 0) {
        report.details << QStringLiteral("Нет полёта в базе — данные с SD не добавлены");
    } else if (!hadSession) {
        m_store->resumeSession(sessionId);
    }

    // ---- эфир этого полёта ----
    QVector<Packet> air;                    // сверяемые пакеты эфира, по порядку
    QVector<QByteArray> airSpecStarts;      // 22 байта каждого ID27 эфира
    for (const Packet &p : m_store->packetsOf(sessionId)) {
        if (isComparable(p.id))
            air.append(p);
        else if (p.id == IdSpecStart)
            airSpecStarts.append(p.payload);
    }
    const QVector<Spectrum> airSpectra = m_store->sessionDataFor(sessionId).spectra;
    QVector<bool> airMatched(air.size(), false);
    int airPos = 0;                         // сверка «в том же порядке»: дальше этой позиции

    // ---- файлы — по порядку папок и имён (ГГММДД/ЧЧММСС) ----
    QStringList sorted = files;
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
        const QFileInfo fa(a), fb(b);
        return fa.dir().dirName() + fa.fileName() < fb.dir().dirName() + fb.fileName();
    });

    qint64 totalBytes = 0, doneBytes = 0;
    for (const QString &f : sorted)
        totalBytes += QFileInfo(f).size();
    int lastPercent = 0;

    for (const QString &fileName : sorted) {
        report.files << fileName;
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            report.details << QStringLiteral("%1: не открывается").arg(fileName);
            continue;
        }

        // Дата — из имени папки; время начала — из имени файла (для ID25, ID28 без времени)
        QDate date = folderDate(fileName);
        if (!date.isValid()) {
            date = QFileInfo(fileName).lastModified().toUTC().date();
            report.details << QStringLiteral("%1: папка не ГГММДД — дата по файлу %2")
                              .arg(fileName, date.toString(Qt::ISODate));
        }
        QTime last = QTime::fromString(QFileInfo(fileName).completeBaseName().left(6), QStringLiteral("hhmmss"));
        if (!last.isValid())
            last = QTime(0, 0);

        int lineNo = 0;
        while (!file.atEnd()) {
            const QByteArray line = file.readLine().trimmed();
            ++lineNo;
            doneBytes += line.size() + 2;
            if (line.isEmpty())
                continue;

            Packet p;
            QString error;
            if (!parseLine(line, p, error)) {
                ++report.badLines;
                report.details << QStringLiteral("%1, строка %2: %3").arg(QFileInfo(fileName).fileName()).arg(lineNo).arg(error);
                continue;
            }

            // Время пакета: из самого пакета, иначе — предыдущее. Время назад — полночь.
            const QTime t = packetTime(p);
            if (t.isValid()) {
                if (t < last)
                    date = date.addDays(1);
                last = t;
            }
            p.rxTime = QDateTime(date, last, QTimeZone::utc());

            // ---- итоговый спектр ----
            if (p.id == IdSpectrum) {
                Spectrum sd;
                if (!spectrumFromSd(p, sd)) {
                    ++report.badLines;
                    report.details << QStringLiteral("%1, строка %2: заголовок спектра").arg(QFileInfo(fileName).fileName()).arg(lineNo);
                    continue;
                }
                const QByteArray head = p.payload.left(LenSpecHeader);
                const Spectrum *airSpec = nullptr;
                if (airSpecStarts.contains(head))
                    for (const Spectrum &a : airSpectra)
                        if (a.info.number == sd.info.number && a.info.startTime == sd.info.startTime)
                            airSpec = &a;

                if (airSpec && airSpec->channels == sd.channels) {
                    ++report.matched;               // сумма порций эфира = итог на SD
                } else {
                    if (airSpec) {
                        ++report.spectraMismatch;   // часть порций потерялась — основным считается спектр с SD
                        report.details << QStringLiteral("Спектр № %1: сумма порций эфира не совпала с SD").arg(sd.info.number);
                    } else {
                        ++report.onlySd;            // набор шёл в радиомолчании или ID27 потерялся
                        report.details << QStringLiteral("Спектр № %1: только на SD").arg(sd.info.number);
                    }
                    if (sessionId != 0)
                        m_store->saveSpectrum(sd);
                }
                continue;
            }

            // ---- остальные: найти в эфире в том же порядке, байт в байт ----
            int found = -1;
            for (int i = airPos; i < air.size(); ++i) {
                if (!airMatched[i] && air[i].id == p.id && air[i].payload == p.payload) {
                    found = i;
                    break;
                }
            }
            if (found >= 0) {
                airMatched[found] = true;
                airPos = found + 1;
                ++report.matched;
            } else {
                ++report.onlySd;
                if (sessionId != 0)
                    addMissing(p);
            }

            // Ход — по прочитанным байтам
            const int percent = totalBytes > 0 ? int(doneBytes * 100 / totalBytes) : 100;
            if (percent > lastPercent && percent < 100) {
                lastPercent = percent;
                emit progress(percent);
            }
        }
    }

    report.onlyAir = int(std::count(airMatched.begin(), airMatched.end(), false));

    if (sessionId != 0 && !hadSession)
        m_store->releaseSession();

    emit progress(100);
    emit imported(report);
    return report;
}

// ============================================================================
//  Разбор строки
// ============================================================================

bool SdImporter::parseLine(const QByteArray &line, Packet &packet, QString &error)
{
    const QList<QByteArray> fields = line.split(',');

    // Каждое поле — ровно два HEX-символа
    QByteArray bytes;
    bytes.reserve(fields.size());
    for (const QByteArray &f : fields) {
        bool ok = false;
        const uint v = f.trimmed().toUInt(&ok, 16);
        if (!ok || f.trimmed().size() != 2 || v > 0xFF) {
            error = QStringLiteral("не HEX: «%1»").arg(QString::fromLatin1(f.left(8)));
            return false;
        }
        bytes.append(char(v));
    }

    packet.id = quint8(bytes[0]);
    packet.payload = bytes.mid(1);

    const int expected = sdLength(packet.id);
    if (expected < 0) {
        error = QStringLiteral("ID %1 на SD не пишется").arg(packet.id);
        return false;
    }
    if (packet.payload.size() != expected) {
        error = QStringLiteral("ID%1: длина %2 вместо %3").arg(packet.id).arg(packet.payload.size()).arg(expected);
        return false;
    }
    return true;
}

QDate SdImporter::folderDate(const QString &file)
{
    const QString dir = QFileInfo(file).dir().dirName();            // ГГММДД
    const QDate d = QDate::fromString(dir, QStringLiteral("yyMMdd"));
    return d.isValid() ? d.addYears(d.year() < 2000 ? 100 : 0) : QDate();   // «26» — это 2026, а не 1926
}

QTime SdImporter::packetTime(const Packet &p)
{
    int at = -1;
    if (p.id == IdCoordOut || p.id == IdCoordIn)
        at = 8;
    else if (p.id == IdMeasure)
        at = 28;
    if (at < 0)
        return QTime();
    return QTime(quint8(p.payload[at]), quint8(p.payload[at + 1]), quint8(p.payload[at + 2]));
}

// ============================================================================
//  Дополнение базы
// ============================================================================

/**
    @brief Пакет «только на SD» — в базу с источником SD.
    @note  Разбирается тем же MessageParser, что и эфир: одни правила разбора.
*/
void SdImporter::addMissing(const Packet &p)
{
    MessageParser parser;
    QObject::connect(&parser, &MessageParser::routePoint, [this](RoutePoint r) {
        r.source = DataSource::Sd;
        m_store->saveRoutePoint(r);
    });
    QObject::connect(&parser, &MessageParser::measurement, [this](Measurement m) {
        m.source = DataSource::Sd;
        m_store->saveMeasurement(m);
    });
    QObject::connect(&parser, &MessageParser::event, [this](CoreEvent e) {
        e.source = DataSource::Sd;
        m_store->saveEvent(e);
    });
    parser.parse(p);
}
