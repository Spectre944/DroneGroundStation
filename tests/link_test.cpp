/**
    @file  link_test.cpp
    @brief Проверка DroneLink без окна.

    Запуск без параметров — проверка сборки пакетов на заготовленных байтах:
    CRC, пакет кусками, мусор, испорченные пакеты, копии.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.

    Запуск с именем порта (например, COM5) — стенд: порт открывается,
    каждый принятый пакет печатается, раз в 10 с — счётчики.
    Второй параметр «id01» — сразу отправить ID01 (предполётная проверка).
    Остановка — закрыть окно программы или кнопка «Стоп» в Qt Creator.
*/

#include <QCoreApplication>
#include <QTimer>
#include <QTextStream>
#include <QTimeZone>
#include <QVector>

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

/// Приёмник пакетов: свой DroneLink и список всего, что он выдал
struct Receiver {
    DroneLink link;
    QVector<Packet> packets;

    Receiver()
    {
        QObject::connect(&link, &DroneLink::packetReceived,
                         [this](const Packet &p) { packets.append(p); });
    }
    void feed(const QByteArray &bytes, const QDateTime &time) { link.processBytes(bytes, time); }
};

const QDateTime T0 = QDateTime(QDate(2026, 11, 23), QTime(10, 0, 0), QTimeZone::utc());

/// Тестовый пакет ID22 с 31 байтом данных (значения не важны)
QByteArray packet22(char fill = 0x11)
{
    return DroneLink::buildPacket(0x16, QByteArray(31, fill));
}

// ---------------------------------------------------------------------------
//  Проверки на заготовленных байтах
// ---------------------------------------------------------------------------

void runSelfTest()
{
    out << "Проверка DroneLink на заготовленных байтах" << Qt::endl;

    // 1. CRC16: контрольный пример из описания алгоритма
    {
        const quint16 crc = DroneLink::crc16("123456789");
        check("CRC16 «123456789» = 0x29B1", crc == 0x29B1,
              QString("получено 0x%1").arg(crc, 4, 16, QChar('0')).toUpper());
    }

    // 2. Пакет «без данных»: длина 14, Length = 1, Payload = 0x01, маркеры на местах
    {
        const QByteArray p = DroneLink::buildPacket(0x01, {});
        const bool ok = p.size() == 14
                     && p.startsWith(QByteArray::fromHex("AA55AA55"))
                     && p.endsWith(QByteArray::fromHex("0D0A0D0A"))
                     && quint8(p[4]) == 0x01 && quint8(p[5]) == 0x01 && quint8(p[6]) == 0x00
                     && quint8(p[7]) == 0x01;
        check("Пакет «без данных» ID01 собран по формату", ok, p.toHex(' '));
    }

    // 3. Целый пакет
    {
        Receiver r;
        r.feed(packet22(), T0);
        const bool ok = r.packets.size() == 1 && r.packets[0].id == 0x16
                     && r.packets[0].payload == QByteArray(31, 0x11);
        check("Целый пакет принят, ID и данные верны", ok);
    }

    // 4. Пакет тремя кусками — выдаётся только после последнего куска
    {
        Receiver r;
        const QByteArray p = packet22();
        r.feed(p.left(5), T0);
        r.feed(p.mid(5, 20), T0);
        const bool none = r.packets.isEmpty();
        r.feed(p.mid(25), T0);
        check("Пакет тремя кусками собран", none && r.packets.size() == 1);
    }

    // 5. Маркер начала разрезан между кусками (AA 55 AA | 55 …)
    {
        Receiver r;
        const QByteArray p = packet22();
        r.feed(QByteArray::fromHex("0000") + p.left(3), T0);
        r.feed(p.mid(3), T0);
        check("Маркер начала, разрезанный между кусками", r.packets.size() == 1);
    }

    // 6. Мусор и 00 00 между двумя разными пакетами
    {
        Receiver r;
        const QByteArray bytes = QByteArray::fromHex("123456") + packet22(0x11)
                               + QByteArray::fromHex("0000") + QByteArray::fromHex("AA55FF")
                               + packet22(0x22) + QByteArray::fromHex("0000");
        r.feed(bytes, T0);
        check("Мусор и 00 00 между пакетами пропущены, оба пакета приняты", r.packets.size() == 2,
              QString("принято %1").arg(r.packets.size()));
    }

    // 7. Испорченный CRC — пакет выброшен, следующий правильный принят
    {
        Receiver r;
        QByteArray bad = packet22(0x11);
        bad[bad.size() - 6] = char(bad[bad.size() - 6] ^ 0xFF);    // испортить младший байт CRC
        r.feed(bad + packet22(0x22), T0);
        const bool ok = r.packets.size() == 1 && r.packets[0].payload[0] == 0x22
                     && r.link.rxStats().crcErrors >= 1;
        check("Испорченный CRC: пакет выброшен, следующий принят", ok,
              QString("принято %1, ошибок CRC %2").arg(r.packets.size()).arg(r.link.rxStats().crcErrors));
    }

    // 8. Испорченный конец — пакет выброшен, следующий принят
    {
        Receiver r;
        QByteArray bad = packet22(0x11);
        bad[bad.size() - 1] = 0x00;
        r.feed(bad + packet22(0x22), T0);
        const bool ok = r.packets.size() == 1 && r.packets[0].payload[0] == 0x22
                     && r.link.rxStats().endErrors >= 1;
        check("Испорченный конец: пакет выброшен, следующий принят", ok,
              QString("принято %1, ошибок конца %2").arg(r.packets.size()).arg(r.link.rxStats().endErrors));
    }

    // 9. Недопустимая длина (Length = 0) — выброшен, следующий принят
    {
        Receiver r;
        const QByteArray bad = QByteArray::fromHex("AA55AA55" "16" "0000");
        r.feed(bad + packet22(0x22), T0);
        const bool ok = r.packets.size() == 1 && r.link.rxStats().lengthErrors >= 1;
        check("Length = 0: выброшен, следующий принят", ok);
    }

    // 10. Копия через 0,5 с — отброшена
    {
        Receiver r;
        r.feed(packet22() + QByteArray::fromHex("0000"), T0);
        r.feed(packet22() + QByteArray::fromHex("0000"), T0.addMSecs(500));
        const bool ok = r.packets.size() == 1 && r.link.rxStats().duplicates == 1;
        check("Копия через 0,5 с отброшена", ok);
    }

    // 11. Тот же пакет через 1,5 с — это новый пакет
    {
        Receiver r;
        r.feed(packet22(), T0);
        r.feed(packet22(), T0.addMSecs(1500));
        check("Тот же пакет через 1,5 с принят как новый", r.packets.size() == 2);
    }

    // 12. Цепочка копий по 0,8 с (всего 2,4 с) — каждая считается от предыдущей копии
    {
        Receiver r;
        for (int i = 0; i < 4; ++i)
            r.feed(packet22(), T0.addMSecs(800 * i));
        check("Цепочка из 4 копий через 0,8 с — принят один пакет", r.packets.size() == 1,
              QString("принято %1").arg(r.packets.size()));
    }

    // 13. Длинный пакет ID23 (2055 байт данных) — в данных встречаются 0D 0A 0D 0A
    {
        Receiver r;
        QByteArray payload(2055, 0x00);
        payload.replace(100, 4, QByteArray::fromHex("0D0A0D0A"));
        payload.replace(200, 4, QByteArray::fromHex("AA55AA55"));
        r.feed(DroneLink::buildPacket(0x17, payload), T0);
        const bool ok = r.packets.size() == 1 && r.packets[0].payload == payload;
        check("ID23 2055 байт с 0D0A0D0A и AA55AA55 внутри данных", ok);
    }

    out << Qt::endl << (g_failed == 0 ? "ИТОГ: все проверки пройдены"
                                      : QString("ИТОГ: провалено проверок — %1").arg(g_failed))
        << Qt::endl;
}

// ---------------------------------------------------------------------------
//  Стенд: настоящий порт
// ---------------------------------------------------------------------------

void runPort(const QString &portName, bool sendId01)
{
    auto *link = new DroneLink(qApp);

    QObject::connect(link, &DroneLink::packetReceived, [](const Packet &p) {
        out << p.rxTime.toString("hh:mm:ss.zzz")
            << "  ID" << QString::number(p.id).rightJustified(2, '0')
            << " (0x" << QString::number(p.id, 16).rightJustified(2, '0').toUpper() << ")"
            << "  данных " << p.payload.size() << " байт" << Qt::endl;
    });
    QObject::connect(link, &DroneLink::portError, [](const QString &text) {
        out << "ОШИБКА ПОРТА: " << text << Qt::endl;
    });

    if (!link->open(portName)) {
        QTimer::singleShot(0, qApp, [] { qApp->exit(1); });
        return;
    }
    out << "Порт " << portName << " открыт (115200 8N1). Ждём пакеты…" << Qt::endl;

    if (sendId01) {
        const auto result = link->send(0x01, {});
        out << "Отправлена ID01: " << (result == DroneLink::SendResult::Ok ? "ok" : "не отправлена")
            << Qt::endl;
    }

    // Раз в 10 с — счётчики
    auto *timer = new QTimer(qApp);
    QObject::connect(timer, &QTimer::timeout, [link] {
        const auto s = link->rxStats();
        out << "--- принято " << s.packets << ", копий " << s.duplicates
            << ", ошибок CRC " << s.crcErrors << ", конца " << s.endErrors
            << ", длины " << s.lengthErrors << Qt::endl;
    });
    timer->start(10000);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    registerCoreTypes();

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);    // консоль Windows: показывать вывод в UTF-8, иначе вместо русского — кракозябры
#endif

    const QStringList args = app.arguments();

    if (args.size() < 2) {          // без параметров — проверка на заготовленных байтах
        runSelfTest();
        return g_failed == 0 ? 0 : 1;
    }

    runPort(args[1], args.size() > 2 && args[2].compare("id01", Qt::CaseInsensitive) == 0);
    return app.exec();
}
