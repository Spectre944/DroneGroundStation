/**
    @file  commands_test.cpp
    @brief Проверка DroneCommands без окна и без порта.

    Вместо настоящего DroneLink — «поддельный» FakeLink: он не пишет в порт,
    а запоминает, что ему дали. Так проверяются байты каждой команды,
    причины отказа, ответы и «нет ответа».
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>
#include <QVector>
#include <cstring>

#include "classes/drone_commands.h"
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
    @brief «Поддельный» DroneLink: не пишет в порт, запоминает отправленное.
*/
class FakeLink : public DroneLink
{
public:
    SendResult result = SendResult::Ok;     ///< что вернуть на send
    QVector<QPair<quint8, QByteArray>> sent;    ///< что отправлено
    int answersReceived = 0;                ///< сколько раз вызван answerReceived

    SendResult send(quint8 id, const QByteArray &payload) override
    {
        if (result == SendResult::Ok)
            sent.append({id, payload});
        return result;
    }
    bool canSend() const override { return true; }
    void answerReceived() override { ++answersReceived; }
};

/// Приёмник: FakeLink, DroneCommands и всё, что DroneCommands выдал
struct Receiver {
    FakeLink link;
    DroneCommands commands{&link};
    QVector<CommandOutcome> finished;
    QVector<CommandRequest> sentSignals;
    QVector<CoreEvent> events;

    Receiver()
    {
        QObject::connect(&commands, &DroneCommands::commandFinished, [this](const CommandOutcome &o) { finished.append(o); });
        QObject::connect(&commands, &DroneCommands::commandSent,     [this](const CommandRequest &r) { sentSignals.append(r); });
        QObject::connect(&commands, &DroneCommands::event,           [this](const CoreEvent &e) { events.append(e); });
    }
};

/// Команда с одним значением
CommandRequest cmd(quint8 id, int value = 0)
{
    CommandRequest r;
    r.id = id;
    r.value = value;
    return r;
}

/// Правильная ID04: 3 вершины, по времени, 30 с, 100 м, коэффициент 3, спектр 60 с
CommandRequest goodId04()
{
    CommandRequest r;
    r.id = 4;
    r.vertices = { {50.445, 30.515}, {50.445, 30.529}, {50.454, 30.529} };
    r.reconMode = ReconMode::Time;
    r.intervalS = 30;
    r.distanceM = 100;
    r.dupFactor = 3;
    r.spectrumS = 60;
    return r;
}

/// float младшим байтом вперёд — для сравнения байтов ID04
QByteArray f32(float v)
{
    quint32 bits;
    std::memcpy(&bits, &v, 4);
    QByteArray b;
    for (int i = 0; i < 4; ++i)
        b.append(char((bits >> (8 * i)) & 0xFF));
    return b;
}

/// Подождать, пока работает цикл событий (для таймера ожидания)
void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

/// Причина отказа для команды
RejectReason rejectOf(const CommandRequest &r)
{
    Receiver rc;
    rc.commands.send(r);
    if (rc.finished.size() != 1 || rc.finished[0].result != CommandResult::Rejected)
        return RejectReason::None;
    return rc.finished[0].reason;
}

void runTests()
{
    out << "Проверка DroneCommands" << Qt::endl;

    // 1. Данные команд с одним значением и «без данных»
    {
        struct Case { quint8 id; int value; const char *hex; };
        const Case cases[] = {
            {0x01, 0, ""}, {0x02, 0, ""}, {0x03, 0, ""},
            {0x05, 3, "03"}, {0x06, 30, "1e00"}, {0x07, 300, "2c01"},
            {0x08, 0, ""}, {0x09, 2, "02"}, {0x0A, 0, ""}, {0x0B, 0, "0000"},
            {0x0C, 0, ""}, {0x0D, 0, ""}, {0x0E, 0, ""}
        };
        bool ok = true;
        QString bad;
        for (const Case &c : cases) {
            const QByteArray p = DroneCommands::buildPayload(cmd(c.id, c.value));
            if (p.toHex() != QByteArray(c.hex)) {
                ok = false;
                bad += QString("ID%1=%2 ").arg(c.id).arg(QString(p.toHex()));
            }
        }
        check("Данные ID01–ID03, ID05–ID14 по протоколу", ok, bad);
    }

    // 2. ID04 побайтно по таблице протокола (п. 4.5): 9 + 8 × 3 = 33 байта
    {
        const CommandRequest r = goodId04();
        const QByteArray p = DroneCommands::buildPayload(r);
        QByteArray expected;
        expected.append(char(3));
        for (const GeoPoint &v : r.vertices)
            expected.append(f32(float(v.lat)) + f32(float(v.lon)));
        expected.append(QByteArray::fromHex("01" "1e00" "6400" "03" "3c00"));
        check("ID04: 33 байта — N, вершины, режим 1, 30 с, 100 м, коэффициент 3, 60 с",
              p.size() == 33 && p == expected, QString(p.toHex(' ')));
    }

    // 3. Причины отказа по значениям
    {
        CommandRequest twoVertices = goodId04();   twoVertices.vertices.removeLast();
        CommandRequest badLat = goodId04();        badLat.vertices[0].lat = 95.0;
        CommandRequest spectrumMode = goodId04();  spectrumMode.reconMode = ReconMode(3);
        CommandRequest zeroInterval = goodId04();  zeroInterval.intervalS = 0;
        CommandRequest zeroDistance = goodId04();  zeroDistance.distanceM = 0;
        CommandRequest dup6 = goodId04();          dup6.dupFactor = 6;

        check("ID0 и ID15 — неверный ID",
              rejectOf(cmd(0)) == RejectReason::BadId && rejectOf(cmd(15)) == RejectReason::BadId);
        check("ID04 с 2 вершинами — область не годится", rejectOf(twoVertices) == RejectReason::AreaNotValid);
        check("ID04 с широтой 95° — область не годится", rejectOf(badLat) == RejectReason::AreaNotValid);
        check("ID04 с режимом 3 (спектр) — неверный режим", rejectOf(spectrumMode) == RejectReason::BadReconMode);
        check("ID04 с интервалом 0 — неверный интервал", rejectOf(zeroInterval) == RejectReason::BadInterval);
        check("ID04 с расстоянием 0 — неверное расстояние", rejectOf(zeroDistance) == RejectReason::BadDistance);
        check("ID04 с коэффициентом 6 — неверный коэффициент", rejectOf(dup6) == RejectReason::BadDupFactor);
        check("ID05 = 1 — неверный коэффициент", rejectOf(cmd(5, 1)) == RejectReason::BadDupFactor);
        check("ID06 = 0 — неверный интервал", rejectOf(cmd(6, 0)) == RejectReason::BadInterval);
        check("ID09 = 4 — неверный режим", rejectOf(cmd(9, 4)) == RejectReason::BadReconMode);
        check("ID11 = 70000 — недопустимое значение", rejectOf(cmd(11, 70000)) == RejectReason::BadValue);
    }

    // 4. Отправка и ответ ID01 — «выполнено»
    {
        Receiver r;
        r.commands.send(cmd(0x01));
        const bool sentOk = r.link.sent.size() == 1 && r.link.sent[0].first == 0x01
                         && r.sentSignals.size() == 1 && r.finished.isEmpty() && r.commands.isWaiting();
        r.commands.onAnswer(0x01);
        const bool ok = sentOk && r.finished.size() == 1 && r.finished[0].result == CommandResult::Done
                     && r.finished[0].attempts == 1 && !r.commands.isWaiting() && r.link.answersReceived == 1;
        check("ID01 отправлена, ответ ID01 — «выполнено»", ok);
    }

    // 5. Ответы ID02 и ID03
    {
        Receiver r;
        r.commands.send(cmd(0x06, 30));
        r.commands.onAnswer(0x02);
        r.commands.send(cmd(0x06, 30));
        r.commands.onAnswer(0x03);
        const bool ok = r.finished.size() == 2
                     && r.finished[0].result == CommandResult::BadPacket
                     && r.finished[1].result == CommandResult::Forbidden;
        check("Ответ ID02 — «ошибка пакета», ID03 — «запрещено»", ok);
    }

    // 6. Вторая команда, пока ждём ответа на первую — «занято»
    {
        Receiver r;
        r.commands.send(cmd(0x01));
        r.commands.send(cmd(0x08));
        const bool ok = r.link.sent.size() == 1 && r.finished.size() == 1
                     && r.finished[0].request.id == 0x08 && r.finished[0].reason == RejectReason::Busy
                     && r.commands.isWaiting();
        check("Вторая команда до ответа на первую — «занято», первая ждёт", ok);
    }

    // 7. Запрещена в текущем режиме
    {
        Receiver r;
        r.commands.setAllowed({0x01});
        r.commands.send(cmd(0x08));
        const bool rejected = r.finished.size() == 1 && r.finished[0].reason == RejectReason::NotAllowed;
        r.commands.send(cmd(0x01));
        check("Разрешена только ID01: ID08 — «запрещена в режиме», ID01 ушла",
              rejected && r.link.sent.size() == 1 && r.link.sent[0].first == 0x01);
    }

    // 8. Радиомолчание: только ID14
    {
        Receiver r;
        r.commands.setRadioSilence(true);
        r.commands.send(cmd(0x0A));
        const bool rejected = r.finished.size() == 1 && r.finished[0].reason == RejectReason::NotAllowed;
        r.commands.send(cmd(0x0E));
        check("Радиомолчание: ID10 не отправлена, ID14 ушла",
              rejected && r.link.sent.size() == 1 && r.link.sent[0].first == 0x0E);
    }

    // 9. Только просмотр
    {
        check("Открыт прошлый полёт — «только просмотр»", [] {
            Receiver r;
            r.commands.setReadOnly(true);
            r.commands.send(cmd(0x01));
            return r.finished.size() == 1 && r.finished[0].reason == RejectReason::ReadOnly && r.link.sent.isEmpty();
        }());
    }

    // 10. Порт закрыт
    {
        Receiver r;
        r.link.result = DroneLink::SendResult::PortClosed;
        r.commands.send(cmd(0x01));
        const bool ok = r.finished.size() == 1 && r.finished[0].reason == RejectReason::PortClosed
                     && !r.commands.isWaiting() && r.sentSignals.isEmpty();
        check("Порт закрыт — «не отправлена», ожидания нет", ok);
    }

    // 11. Нет ответа (таймер 0,2 с вместо 20 с)
    {
        Receiver r;
        r.commands.setAnswerTimeoutMs(200);
        r.commands.send(cmd(0x01));
        wait(400);
        const bool ok = r.finished.size() == 1 && r.finished[0].result == CommandResult::NoAnswer
                     && !r.commands.isWaiting() && r.link.answersReceived == 0;
        check("Нет ответа за время ожидания — «нет ответа»", ok);

        // 12. Ответ после «нет ответа» — событие, итог не меняется
        r.commands.onAnswer(0x01);
        const bool late = r.finished.size() == 1 && r.events.size() == 1
                       && r.events[0].code == EventCode::UnexpectedAnswer;
        check("Опоздавший ответ — событие в журнал, итог прежний", late);
    }

    // 13. Ответ пришёл до таймера — таймер не даёт второго итога
    {
        Receiver r;
        r.commands.setAnswerTimeoutMs(200);
        r.commands.send(cmd(0x01));
        r.commands.onAnswer(0x01);
        wait(400);
        check("Ответ до таймера: один итог, «нет ответа» не появляется",
              r.finished.size() == 1 && r.finished[0].result == CommandResult::Done);
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
