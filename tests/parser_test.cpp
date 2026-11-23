/**
    @file  parser_test.cpp
    @brief Проверка MessageParser без окна и без логгера.

    Пакеты с известными значениями подаются в MessageParser,
    результат сверяется с ожидаемым. Значения ID20 и ID22 — из примера
    строк SD в протоколе (п. 10.3): те же байты, что передаёт логгер.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>
#include <cmath>
#include <cstring>

#include "classes/message_parser.h"

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

/// Числа с плавающей точкой сравниваются с допуском (имя не near: в windows.h near — макрос)
bool approx(double a, double b, double eps = 1e-4) { return std::fabs(a - b) < eps; }

/// Приёмник: свой MessageParser и всё, что он выдал
struct Receiver {
    MessageParser parser;
    QVector<quint8>          answers;
    QVector<QPair<quint8, QVariantMap>> messages;
    QVector<RoutePoint>      route;
    QVector<Measurement>     measurements;
    QVector<SpectrumInfo>    spectra;
    QVector<SpectrumPortion> portions;
    QVector<CoreEvent>       events;

    Receiver()
    {
        QObject::connect(&parser, &MessageParser::answerReceived,  [this](quint8 id) { answers.append(id); });
        QObject::connect(&parser, &MessageParser::loggerMessage,   [this](quint8 id, const QVariantMap &p) { messages.append({id, p}); });
        QObject::connect(&parser, &MessageParser::routePoint,      [this](const RoutePoint &r) { route.append(r); });
        QObject::connect(&parser, &MessageParser::measurement,     [this](const Measurement &m) { measurements.append(m); });
        QObject::connect(&parser, &MessageParser::spectrumStarted, [this](const SpectrumInfo &i) { spectra.append(i); });
        QObject::connect(&parser, &MessageParser::spectrumPortion, [this](const SpectrumPortion &s) { portions.append(s); });
        QObject::connect(&parser, &MessageParser::event,           [this](const CoreEvent &e) { events.append(e); });
    }

    /// Подать пакет: ID, данные в HEX, время приёма
    void feed(quint8 id, const QByteArray &payload, const QDateTime &rxTime)
    {
        Packet p;
        p.id = id;
        p.payload = payload;
        p.rxTime = rxTime;
        parser.parse(p);
    }
};

const QDateTime RX = QDateTime(QDate(2026, 11, 23), QTime(12, 40, 0), QTimeZone::utc());
const QByteArray NoData = QByteArray(1, 0x01);

// ID20 из протокола: широта 50,45°, долгота 30,523°, время 12:34:56
const QByteArray Id20 = QByteArray::fromHex("CDCC4942" "1B2FF441" "0C2238");

// ID22 из протокола: ПАЕД 0,125; температура 0x015A; состояние 0x11; высота 1000 см;
// мощность 0x0FFF; широта 50,45°; долгота 30,523°; высота GPS 0; спутники 0; батарея 12,4 В; 12:35:10
const QByteArray Id22 = QByteArray::fromHex("0000003E" "5A01" "11" "E803" "FF0F" "CDCC4942" "1B2FF441"
                                            "00000000" "00" "66664641" "0C230A");

/// Двухбайтовое число младшим байтом вперёд
QByteArray u16(quint16 v) { return QByteArray(1, char(v & 0xFF)) + QByteArray(1, char(v >> 8)); }

/// float младшим байтом вперёд
QByteArray f32(float v)
{
    quint32 bits;
    std::memcpy(&bits, &v, 4);
    QByteArray b;
    for (int i = 0; i < 4; ++i)
        b.append(char((bits >> (8 * i)) & 0xFF));
    return b;
}

void runTests()
{
    out << "Проверка MessageParser" << Qt::endl;

    // 1. ID20 — координата в области
    {
        Receiver r;
        r.feed(0x14, Id20, RX);
        const bool ok = r.route.size() == 1
                     && approx(r.route[0].lat, 50.45) && approx(r.route[0].lon, 30.523)
                     && r.route[0].time == QDateTime(QDate(2026, 11, 23), QTime(12, 34, 56), QTimeZone::utc())
                     && r.route[0].inArea && r.route[0].msgId == 0x14;
        check("ID20: 50,45° / 30,523° / 12:34:56, в области", ok);
    }

    // 2. ID19 — та же координата, вне области
    {
        Receiver r;
        r.feed(0x13, Id20, RX);
        check("ID19: точка маршрута вне области", r.route.size() == 1 && !r.route[0].inArea);
    }

    // 3. ID22 — все поля измерения
    {
        Receiver r;
        r.feed(0x16, Id22, RX);
        bool ok = r.measurements.size() == 1 && r.route.size() == 1;
        if (ok) {
            const Measurement &m = r.measurements[0];
            ok = approx(m.paed, 0.125) && m.tempCode == 0x015A && m.bdbgStat == 0x11
              && m.lidarCm == 1000 && m.lidarStrength == 0x0FFF
              && approx(m.lat, 50.45) && approx(m.lon, 30.523) && approx(m.battV, 12.4)
              && m.time == QDateTime(QDate(2026, 11, 23), QTime(12, 35, 10), QTimeZone::utc())
              && !m.hasPaed1m && m.flags == MfNone && r.route[0].inArea;
        }
        check("ID22: ПАЕД 0,125; 0x015A; 0x11; 1000 см; 0x0FFF; 50,45°; 30,523°; 12,4 В; 12:35:10", ok);
    }

    // 4. Ответы на команды
    {
        Receiver r;
        r.feed(0x01, NoData, RX);
        r.feed(0x02, NoData, RX);
        r.feed(0x03, NoData, RX);
        const bool ok = r.answers == QVector<quint8>({1, 2, 3}) && r.events.isEmpty() && r.messages.isEmpty();
        check("ID01, ID02, ID03 — ответы на команды, без событий", ok);
    }

    // 5. ID05 — аккумулятор заряжен, 12,4 В
    {
        Receiver r;
        r.feed(0x05, f32(12.4f), RX);
        const bool ok = r.events.size() == 1 && r.events[0].code == EventCode::BatteryOk
                     && approx(r.events[0].params.value("voltage").toDouble(), 12.4)
                     && r.messages.size() == 1 && r.messages[0].first == 0x05;
        check("ID05: событие «аккумулятор заряжен», 12,4 В", ok);
    }

    // 6. ID25 — неисправен аккумулятор (тип 3)
    {
        Receiver r;
        r.feed(0x19, QByteArray::fromHex("03"), RX);
        const bool ok = r.events.size() == 1 && r.events[0].code == EventCode::DeviceFailed
                     && r.events[0].params.value("device").toInt() == 3
                     && r.messages.size() == 1;
        check("ID25: прибор неисправен, тип 3 (аккумулятор)", ok);
    }

    // 7. ID25 с недопустимым типом прибора
    {
        Receiver r;
        r.feed(0x19, QByteArray::fromHex("07"), RX);
        const bool ok = r.events.size() == 1 && r.events[0].code == EventCode::BadValue && r.messages.isEmpty();
        check("ID25 с типом 7: недопустимое значение", ok);
    }

    // 8. ID28, ID18 — события без данных
    {
        Receiver r;
        r.feed(0x1C, NoData, RX);
        r.feed(0x12, NoData, RX);
        const bool ok = r.events.size() == 2
                     && r.events[0].code == EventCode::SpectrumStartFailed
                     && r.events[1].code == EventCode::CheckPassed
                     && r.messages.size() == 2;
        check("ID28, ID18: события и сообщения для состояния логгера", ok);
    }

    // 9. ID27 — начало спектра: «26» «11» «23» «12» «34», № 7, 50,45° / 30,523°, 1500 см
    {
        Receiver r;
        const QByteArray payload = QByteArray("2611231234") + u16(7)
                                 + f32(50.45f) + f32(30.523f) + u16(1500);
        r.feed(0x1B, payload, RX);
        bool ok = r.spectra.size() == 1;
        if (ok) {
            const SpectrumInfo &s = r.spectra[0];
            ok = s.number == 7 && approx(s.lat, 50.45) && approx(s.lon, 30.523) && s.lidarCm == 1500
              && s.startTime == QDateTime(QDate(2026, 11, 23), QTime(12, 34), QTimeZone::utc());
        }
        check("ID27: 23.11.2026 12:34, № 7, 50,45° / 30,523°, 1500 см", ok);
    }

    // 10. ID27 с испорченной датой
    {
        Receiver r;
        const QByteArray payload = QByteArray("26AB231234") + u16(7) + f32(50.45f) + f32(30.523f) + u16(1500);
        r.feed(0x1B, payload, RX);
        const bool ok = r.spectra.isEmpty() && r.events.size() == 1 && r.events[0].code == EventCode::BadValue;
        check("ID27 с буквами вместо месяца: недопустимое значение", ok);
    }

    // 11. ID23 — порция: в канале N значение N, время 6, ПАЕД 0,5, Stat 0x11
    {
        Receiver r;
        QByteArray payload;
        for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch)
            payload.append(u16(quint16(ch)));
        payload.append(u16(6));
        payload.append(f32(0.5f));
        payload.append(char(0x11));
        r.feed(0x17, payload, RX);
        bool ok = r.portions.size() == 1;
        if (ok) {
            const SpectrumPortion &s = r.portions[0];
            ok = s.channels.size() == 1024 && s.channels[0] == 0 && s.channels[300] == 300
              && s.channels[1023] == 1023 && s.accTime == 6 && approx(s.paed, 0.5) && s.stat == 0x11;
        }
        check("ID23: 1024 канала, время 6, ПАЕД 0,5, Stat 0x11", ok);
    }

    // 12. Неверная длина: ID22 с 30 байтами вместо 31
    {
        Receiver r;
        r.feed(0x16, Id22.left(30), RX);
        const bool ok = r.measurements.isEmpty() && r.route.isEmpty() && r.events.size() == 1
                     && r.events[0].code == EventCode::BadLength
                     && r.events[0].params.value("length").toInt() == 30;
        check("ID22 с 30 байтами: неверная длина, измерения нет", ok);
    }

    // 13. Неизвестный ID
    {
        Receiver r;
        r.feed(0x40, QByteArray::fromHex("0102"), RX);
        const bool ok = r.events.size() == 1 && r.events[0].code == EventCode::UnknownId
                     && r.events[0].params.value("msgId").toInt() == 0x40;
        check("ID 0x40: неизвестный ID — событие, работа продолжается", ok);
    }

    // 14. Отладочный ID70
    {
        Receiver r;
        r.feed(70, NoData, RX);
        const bool ok = r.events.size() == 1 && r.events[0].code == EventCode::DebugState
                     && r.events[0].debug && r.events[0].params.value("state").toString() == "COMMAND_INFO"
                     && r.messages.isEmpty();
        check("ID70: отладочное, COMMAND_INFO", ok);
    }

    // 15. Полночь: пакет 23:59:58, принят в 00:00:01 следующих суток — дата предыдущая
    {
        Receiver r;
        const QDateTime rx(QDate(2026, 11, 24), QTime(0, 0, 1), QTimeZone::utc());
        r.feed(0x14, QByteArray::fromHex("CDCC4942" "1B2FF441" "173B3A"), rx);   // 23:59:58
        const bool ok = r.route.size() == 1
                     && r.route[0].time == QDateTime(QDate(2026, 11, 23), QTime(23, 59, 58), QTimeZone::utc());
        check("Полночь: 23:59:58, принят 24.11 00:00:01 → дата 23.11", ok,
              r.route.isEmpty() ? QString() : r.route[0].time.toString(Qt::ISODate));
    }

    // 16. Полночь наоборот: пакет 00:00:02, принят в 23:59:59 — дата следующая
    {
        Receiver r;
        const QDateTime rx(QDate(2026, 11, 23), QTime(23, 59, 59), QTimeZone::utc());
        r.feed(0x14, QByteArray::fromHex("CDCC4942" "1B2FF441" "000002"), rx);
        const bool ok = r.route.size() == 1
                     && r.route[0].time == QDateTime(QDate(2026, 11, 24), QTime(0, 0, 2), QTimeZone::utc());
        check("Полночь: 00:00:02, принят 23.11 23:59:59 → дата 24.11", ok);
    }

    // 17. Недопустимое время: 25 часов
    {
        Receiver r;
        r.feed(0x14, QByteArray::fromHex("CDCC4942" "1B2FF441" "190000"), RX);
        const bool ok = r.route.isEmpty() && r.events.size() == 1 && r.events[0].code == EventCode::BadValue;
        check("ID20 со временем 25:00:00: недопустимое значение, точки нет", ok);
    }

    // 18. Режим логгера записывается в точку маршрута
    {
        Receiver r;
        r.parser.setLoggerMode(LoggerMode::Work);
        r.feed(0x16, Id22, RX);
        check("Режим логгера (Разведка) записан в точку маршрута",
              r.route.size() == 1 && r.route[0].mode == LoggerMode::Work);
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
