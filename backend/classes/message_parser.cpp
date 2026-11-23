#include "message_parser.h"

#include <QTimeZone>
#include <cstring>

/**
    @brief Длины данных сообщений (протокол, п. 6.1).
*/
namespace {
    constexpr int NoData        = 1;        ///< пакет «без данных»: Payload = 0x01
    constexpr int LenBattery    = 4;        ///< ID04, ID05: напряжение, float
    constexpr int LenCoord      = 11;       ///< ID19, ID20
    constexpr int LenDevice     = 1;        ///< ID25: тип прибора
    constexpr int LenMeasure    = 31;       ///< ID22
    constexpr int LenPortion    = 2055;     ///< ID23
    constexpr int LenSpecStart  = 22;       ///< ID27

    constexpr quint8 DebugFirst = 70;       ///< отладочные ID70–ID84
    constexpr quint8 DebugLast  = 84;

    /// Имена состояний стейт-машины логгера для отладочных ID70–ID84 (протокол, п. 7.2)
    const char *debugStateName(quint8 id)
    {
        static const char *names[] = {
            "COMMAND_INFO", "REQ_BATTERY", "BATTERY_PROC", "BATTERY_ERROR_PROC",
            "REQ_BDBG", "BDBG_PROC", "BDBG_ERROR_PROC",
            "REQ_LIDAR", "LIDAR_PROC", "LIDAR_ERROR_PROC",
            "REQ_GPS", "GPS_PROC", "GPS_ERROR_PROC",
            "WORK_SD", "FLIGHT_CHANGE"
        };
        return names[id - DebugFirst];
    }
}

MessageParser::MessageParser(QObject *parent)
    : QObject(parent)
{
}

// ============================================================================
//  Длина данных
// ============================================================================

int MessageParser::expectedLength(quint8 id)
{
    switch (id) {
    case 0x04: case 0x05:
        return LenBattery;
    case 0x13: case 0x14:
        return LenCoord;
    case 0x16:
        return LenMeasure;
    case 0x17:
        return LenPortion;
    case 0x19:
        return LenDevice;
    case 0x1B:
        return LenSpecStart;
    case 0x01: case 0x02: case 0x03:                        // ответы на команды
    case 0x06: case 0x07: case 0x08: case 0x09:             // проверка: батарея, БДБГ
    case 0x0B: case 0x0C: case 0x0F: case 0x10: case 0x12:  // проверка: LIDAR, GPS, SD, итог
    case 0x15: case 0x18: case 0x1A: case 0x1C: case 0x1D:  // точка измерена, спуфинг, часы, спектр, режим
        return NoData;
    default:
        break;
    }
    if (id >= DebugFirst && id <= DebugLast)
        return NoData;
    return -1;                                              // неизвестный ID
}

// ============================================================================
//  Разбор
// ============================================================================

void MessageParser::parse(const Packet &p)
{
    // 1. Известен ли ID
    const int expected = expectedLength(p.id);
    if (expected < 0) {
        sendEvent(EventCode::UnknownId, p, {{QStringLiteral("msgId"), p.id},
                                            {QStringLiteral("length"), p.payload.size()}});
        return;
    }

    // 2. Длина: иначе байты разошлись бы по полям как настоящие значения
    if (p.payload.size() != expected) {
        sendEvent(EventCode::BadLength, p, {{QStringLiteral("msgId"), p.id},
                                            {QStringLiteral("length"), p.payload.size()}});
        return;
    }

    // 3. Отладочные ID70–ID84 — только в журнал, с пометкой «отладка»
    if (p.id >= DebugFirst && p.id <= DebugLast) {
        sendEvent(EventCode::DebugState, p, {{QStringLiteral("msgId"), p.id},
                                             {QStringLiteral("state"), QString::fromLatin1(debugStateName(p.id))}},
                  true);
        return;
    }

    // 4. По ID
    switch (p.id) {
    // ---- ответы на команды ----
    case 0x01: case 0x02: case 0x03:
        emit answerReceived(p.id);
        return;

    // ---- данные ----
    case 0x13: case 0x14:                   // ID19, ID20 — координата
        if (parseCoord(p))
            emit loggerMessage(p.id, {});
        return;
    case 0x16:                              // ID22 — измерение
        if (parseMeasurement(p))
            emit loggerMessage(p.id, {});
        return;
    case 0x17:                              // ID23 — порция спектра
        parsePortion(p);
        return;
    case 0x1B:                              // ID27 — начало спектра
        if (parseSpectrumStart(p))
            emit loggerMessage(p.id, {});
        return;

    // ---- аккумулятор: напряжение ----
    case 0x04: case 0x05: {
        const QVariantMap params{{QStringLiteral("voltage"), double(readFloat(p.payload, 0))}};
        sendEvent(EventCode(p.id), p, params);
        emit loggerMessage(p.id, params);
        return;
    }

    // ---- ID25: прибор неисправен ----
    case 0x19: {
        const quint8 device = readU8(p.payload, 0);
        if (device > 3) {                   // 0 БДБГ, 1 LIDAR, 2 GPS, 3 аккумулятор
            sendBadValue(p, QStringLiteral("device"));
            return;
        }
        const QVariantMap params{{QStringLiteral("device"), device}};
        sendEvent(EventCode::DeviceFailed, p, params);
        emit loggerMessage(p.id, params);
        return;
    }

    // ---- остальные «без данных»: событие + состояние логгера ----
    default:
        sendEvent(EventCode(p.id), p);      // код события = ID сообщения (core_types.h)
        if (p.id != 0x15 && p.id != 0x1D)   // ID21, ID29 на состояние логгера не влияют
            emit loggerMessage(p.id, {});
        return;
    }
}

/**
    @brief ID19, ID20: широта, долгота (float), часы, минуты, секунды (протокол, п. 6.4).
*/
bool MessageParser::parseCoord(const Packet &p)
{
    const QDateTime time = logTime(readU8(p.payload, 8), readU8(p.payload, 9),
                                   readU8(p.payload, 10), p.rxTime);
    if (!time.isValid()) {
        sendBadValue(p, QStringLiteral("time"));
        return false;
    }

    RoutePoint r;
    r.time   = time;
    r.rxTime = p.rxTime;
    r.lat    = readFloat(p.payload, 0);
    r.lon    = readFloat(p.payload, 4);
    r.inArea = (p.id == 0x14);              // ID20 — в области, ID19 — вне
    r.msgId  = p.id;
    r.mode   = m_mode;
    emit routePoint(r);
    return true;
}

/**
    @brief ID22: измерение, 31 байт (протокол, п. 6.5).
    @note  Высота GPS (смещение 19) и число спутников (23) логгер пока не определяет — не читаются.
*/
bool MessageParser::parseMeasurement(const Packet &p)
{
    const QDateTime time = logTime(readU8(p.payload, 28), readU8(p.payload, 29),
                                   readU8(p.payload, 30), p.rxTime);
    if (!time.isValid()) {
        sendBadValue(p, QStringLiteral("time"));
        return false;
    }

    Measurement m;
    m.time          = time;
    m.rxTime        = p.rxTime;
    m.paed          = readFloat(p.payload, 0);
    m.tempCode      = readU16(p.payload, 4);
    m.bdbgStat      = readU8(p.payload, 6);
    m.lidarCm       = readU16(p.payload, 7);
    m.lidarStrength = readU16(p.payload, 9);
    m.lat           = readFloat(p.payload, 11);
    m.lon           = readFloat(p.payload, 15);
    m.battV         = readFloat(p.payload, 24);
    m.source        = DataSource::Air;

    // Координаты измерения — тоже точка маршрута
    RoutePoint r;
    r.time   = m.time;
    r.rxTime = m.rxTime;
    r.lat    = m.lat;
    r.lon    = m.lon;
    r.inArea = true;
    r.msgId  = p.id;
    r.mode   = m_mode;

    emit routePoint(r);
    emit measurement(m);
    return true;
}

/**
    @brief ID23: порция спектра — 1024 канала uint16, время набора uint16, ПАЕД float, Stat (п. 6.6).
*/
bool MessageParser::parsePortion(const Packet &p)
{
    SpectrumPortion portion;
    portion.channels.resize(CoreConst::SpectrumChannels);
    for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch)
        portion.channels[ch] = readU16(p.payload, ch * 2);

    const int tail = CoreConst::SpectrumChannels * 2;   // 2048 — после каналов
    portion.accTime = readU16(p.payload, tail);
    portion.paed    = readFloat(p.payload, tail + 2);
    portion.stat    = readU8(p.payload, tail + 6);
    portion.rxTime  = p.rxTime;

    emit spectrumPortion(portion);
    return true;
}

/**
    @brief ID27: начало набора спектра, 22 байта (протокол, п. 6.8).
           Год, месяц, день, часы, минуты — по 2 символа ASCII; номер uint16;
           широта, долгота float; высота LIDAR uint16, см.
*/
bool MessageParser::parseSpectrumStart(const Packet &p)
{
    // Два символа ASCII → число; -1, если это не цифры
    auto twoDigits = [&p](int offset) -> int {
        const char a = p.payload[offset], b = p.payload[offset + 1];
        if (a < '0' || a > '9' || b < '0' || b > '9')
            return -1;
        return (a - '0') * 10 + (b - '0');
    };

    const int yy = twoDigits(0), mo = twoDigits(2), dd = twoDigits(4);
    const int hh = twoDigits(6), mi = twoDigits(8);
    const QDate date(2000 + yy, mo, dd);
    const QTime time(hh, mi);
    if (yy < 0 || !date.isValid() || !time.isValid()) {
        sendBadValue(p, QStringLiteral("dateTime"));
        return false;
    }

    SpectrumInfo info;
    info.number    = readU16(p.payload, 10);
    info.startTime = QDateTime(date, time, QTimeZone::utc());
    info.lat       = readFloat(p.payload, 12);
    info.lon       = readFloat(p.payload, 16);
    info.lidarCm   = readU16(p.payload, 20);
    info.source    = DataSource::Air;
    // setDurationS — из ID04 / ID11, заполняет SpectrumCollector

    emit spectrumStarted(info);
    return true;
}

// ============================================================================
//  События
// ============================================================================

void MessageParser::sendEvent(EventCode code, const Packet &p, const QVariantMap &params, bool debug)
{
    CoreEvent e;
    e.code   = code;
    e.time   = p.rxTime;
    e.params = params;
    e.debug  = debug;
    e.source = DataSource::Air;
    emit event(e);
}

void MessageParser::sendBadValue(const Packet &p, const QString &field)
{
    sendEvent(EventCode::BadValue, p, {{QStringLiteral("msgId"), p.id},
                                       {QStringLiteral("field"), field}});
}

// ============================================================================
//  Числа и время
// ============================================================================

QDateTime MessageParser::logTime(quint8 h, quint8 m, quint8 s, const QDateTime &rxTime)
{
    const QTime t(h, m, s);
    if (!t.isValid())                       // часы > 23, минуты или секунды > 59
        return QDateTime();

    const QDateTime rx = rxTime.toUTC();
    QDateTime best;
    qint64 bestDiff = 0;
    for (int day = -1; day <= 1; ++day) {   // вчера, сегодня, завтра
        const QDateTime candidate(rx.date().addDays(day), t, QTimeZone::utc());
        const qint64 diff = qAbs(candidate.secsTo(rx));
        if (!best.isValid() || diff < bestDiff) {
            best = candidate;
            bestDiff = diff;
        }
    }
    return best;
}

float MessageParser::readFloat(const QByteArray &data, int offset)
{
    // Собрать 32 бита младшим байтом вперёд, затем те же биты прочитать как float (IEEE 754)
    const quint32 bits = quint32(quint8(data[offset]))
                       | quint32(quint8(data[offset + 1])) << 8
                       | quint32(quint8(data[offset + 2])) << 16
                       | quint32(quint8(data[offset + 3])) << 24;
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

quint16 MessageParser::readU16(const QByteArray &data, int offset)
{
    return quint16(quint8(data[offset]) | (quint8(data[offset + 1]) << 8));
}
