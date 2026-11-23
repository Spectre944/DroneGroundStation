/**
    @file  state_test.cpp
    @brief Проверка LoggerState без окна и без логгера.

    Проигрываются сценарии, как в «Программе проверки логгера на имитаторе»:
    сообщения логгера и ответы на команды подаются по очереди, после каждого
    шага проверяются режим, приборы, спектр и разрешённые команды.
    Итог каждой проверки — «ok» или «ОШИБКА», в конце — общий итог.
*/

#include <QCoreApplication>
#include <QTextStream>
#include <QVector>

#include "classes/logger_state.h"

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

QString modeName(LoggerMode m)
{
    switch (m) {
    case LoggerMode::Unknown: return "неизвестно";
    case LoggerMode::Test:    return "проверка";
    case LoggerMode::Expect:  return "ожидание";
    case LoggerMode::Work:    return "разведка";
    case LoggerMode::Return:  return "возвращение";
    case LoggerMode::Error:   return "авария";
    }
    return "?";
}

/// Приёмник: LoggerState и всё, что он выдал
struct Receiver {
    LoggerState state;
    QVector<SpectrumStopReason> spectrumEnds;
    QVector<quint8> errors;
    int mismatches = 0;
    QVector<bool> silence;

    Receiver()
    {
        QObject::connect(&state, &LoggerState::spectrumFinished, [this](SpectrumStopReason r) { spectrumEnds.append(r); });
        QObject::connect(&state, &LoggerState::enteredError,     [this](quint8 id) { errors.append(id); });
        QObject::connect(&state, &LoggerState::modeMismatch,     [this](const CommandRequest &) { ++mismatches; });
        QObject::connect(&state, &LoggerState::radioSilenceChanged, [this](bool on) { silence.append(on); });
    }

    /// Сообщение логгера
    void msg(quint8 id, const QVariantMap &params = {}) { state.onMessage(id, params); }

    /// Итог команды
    void answer(quint8 cmdId, CommandResult result = CommandResult::Done, int value = 0,
                ReconMode reconMode = ReconMode::Time)
    {
        CommandOutcome o;
        o.request.id = cmdId;
        o.request.value = value;
        o.request.reconMode = reconMode;
        o.result = result;
        state.onCommandFinished(o);
    }

    LoggerMode mode() const { return state.status().mode; }
    DeviceCheck device(LoggerDevice d) const { return state.status().devices[int(d)].check; }
    quint8 deviceMsg(LoggerDevice d) const { return state.status().devices[int(d)].msgId; }
    QList<quint8> allowed() const { return state.allowedCommands(); }

    /// Успешная проверка: ID01 на ID01, затем приборы по порядку протокола (п. 6.2)
    void passCheck()
    {
        answer(0x01);
        msg(0x13);                                  // GPS — стартовая координата
        msg(0x0B);                                  // LIDAR
        msg(0x09);                                  // БДБГ
        msg(0x05, {{"voltage", 15.6}});             // аккумулятор
        msg(0x12);                                  // ID18 — проверка пройдена
    }
};

QString list(const QList<quint8> &ids)
{
    QStringList s;
    for (quint8 id : ids)
        s << QString::number(id);
    return s.join(',');
}

void runTests()
{
    out << "Проверка LoggerState" << Qt::endl;

    // 0. Начало: режим неизвестен, команды не ограничены
    {
        Receiver r;
        check("Начало: режим «неизвестно», разрешены все 14 команд",
              r.mode() == LoggerMode::Unknown && r.allowed().size() == 14);
    }

    // 1. Успешная предполётная проверка
    {
        Receiver r;
        r.answer(0x01);
        const bool checking = r.mode() == LoggerMode::Test && r.state.status().checking
                           && r.allowed() == QList<quint8>{1};
        r.passCheck();
        const LoggerStatus &s = r.state.status();
        bool allOk = true;
        for (int d = 0; d < int(LoggerDevice::Count); ++d)
            allOk = allOk && s.devices[d].check == DeviceCheck::Ok;
        check("ID01 на ID01 — «проверка», идёт проверка, разрешена только ID01", checking);
        check("Проверка пройдена (ID18) — «ожидание», все 5 приборов исправны, 15,6 В",
              r.mode() == LoggerMode::Expect && !s.checking && allOk && s.hasBattV && qAbs(s.battV - 15.6f) < 0.01f,
              modeName(r.mode()));
        check("В ожидании разрешены ID04–ID09, ID11, ID14",
              r.allowed() == QList<quint8>({4, 5, 6, 7, 8, 9, 11, 14}), list(r.allowed()));
    }

    // 2. Ошибка SD при проверке, продолжить без SD
    {
        Receiver r;
        r.answer(0x01);
        r.msg(0x13); r.msg(0x0B); r.msg(0x09); r.msg(0x05, {{"voltage", 15.6}});
        r.msg(0x10);                                // ID16 — SD неисправна
        const bool sdError = r.mode() == LoggerMode::Test && r.device(LoggerDevice::Sd) == DeviceCheck::Fail
                          && r.allowed() == QList<quint8>({1, 2, 3});
        r.answer(0x02);                             // продолжить без SD
        check("ID16 при проверке — SD «ошибка», разрешены ID01, ID02, ID03", sdError, list(r.allowed()));
        check("ID01 на ID02 — «ожидание», «без SD»",
              r.mode() == LoggerMode::Expect && r.state.status().noSd);
    }

    // 3. БДБГ не ответил при проверке
    {
        Receiver r;
        r.answer(0x01);
        r.msg(0x13); r.msg(0x0B);
        r.msg(0x08);                                // ID08 — БДБГ не ответил
        check("ID08 при проверке — БДБГ «ошибка» (причина ID08), режим «проверка», проверка остановлена",
              r.device(LoggerDevice::Bdbg) == DeviceCheck::Fail && r.deviceMsg(LoggerDevice::Bdbg) == 0x08
              && r.mode() == LoggerMode::Test && !r.state.status().checking);
    }

    // 4. Полёт: область, вход, разведка, возвращение
    {
        Receiver r;
        r.passCheck();
        r.answer(0x04, CommandResult::Done, 0, ReconMode::Distance);
        const bool expect = r.mode() == LoggerMode::Expect && r.state.status().reconMode == ReconMode::Distance;
        r.msg(0x13);                                // ID19 в ожидании — просто координата
        const bool stillExpect = r.mode() == LoggerMode::Expect;
        r.msg(0x14);                                // ID20 — вход в область
        const bool work = r.mode() == LoggerMode::Work
                       && r.allowed() == QList<quint8>({4, 5, 6, 7, 8, 9, 10, 11, 13, 14});
        r.answer(0x09, CommandResult::Done, 0);     // режим «по оператору»
        const bool operatorMode = r.state.status().reconMode == ReconMode::Operator;
        r.answer(0x08);                             // закончить разведку
        check("ID01 на ID04 — «ожидание», режим разведки «по расстоянию»", expect);
        check("ID19 в ожидании режим не меняет", stillExpect);
        check("ID20 — «разведка», разрешены ID04–ID11, ID13, ID14", work, list(r.allowed()));
        check("ID01 на ID09 = 0 — режим разведки «по оператору»", operatorMode);
        check("ID01 на ID08 — «возвращение», разрешены ID04, ID05, ID14",
              r.mode() == LoggerMode::Return && r.allowed() == QList<quint8>({4, 5, 14}), list(r.allowed()));
    }

    // 5. Спектр: подготовка, набор, конец по ID22
    {
        Receiver r;
        r.passCheck();
        r.msg(0x14);
        r.answer(0x09, CommandResult::Done, 3);     // ID09 = 3
        const bool preparing = r.state.status().spectrumStage == SpectrumStage::Preparing
                            && r.allowed() == QList<quint8>({4, 5, 6, 7, 8, 13, 14});
        r.msg(0x1B);                                // ID27
        const bool collecting = r.state.status().spectrumStage == SpectrumStage::Collecting
                             && r.allowed() == QList<quint8>({4, 5, 6, 7, 8, 12, 13, 14});
        r.msg(0x16);                                // ID22 — логгер вернулся к измерениям
        check("ID01 на ID09 = 3 — подготовка спектра, ID09–ID12 запрещены", preparing, list(r.allowed()));
        check("ID27 — идёт набор, разрешена ID12", collecting, list(r.allowed()));
        check("ID22 после набора — набор окончен, причина «измерение»",
              r.state.status().spectrumStage == SpectrumStage::None && r.spectrumEnds.size() == 1
              && r.spectrumEnds[0] == SpectrumStopReason::Measurement);
    }

    // 6. Спектр: другие причины конца
    {
        Receiver r1, r2, r3;
        for (Receiver *r : {&r1, &r2, &r3}) {
            r->passCheck();
            r->msg(0x14);
            r->answer(0x09, CommandResult::Done, 3);
            r->msg(0x1B);
        }
        r1.answer(0x0C);                            // ID12
        r2.answer(0x08);                            // ID08
        r3.msg(0x1B);                               // новый ID27
        check("Конец набора: ID01 на ID12 — «прекращён командой»",
              r1.spectrumEnds == QVector<SpectrumStopReason>{SpectrumStopReason::CommandStop});
        check("Конец набора: ID01 на ID08 — «закончить разведку», режим «возвращение»",
              r2.spectrumEnds == QVector<SpectrumStopReason>{SpectrumStopReason::CommandFinish}
              && r2.mode() == LoggerMode::Return);
        check("Новый ID27 при незакрытом наборе — прежний закрыт, новый идёт",
              r3.spectrumEnds == QVector<SpectrumStopReason>{SpectrumStopReason::NewSpectrum}
              && r3.state.status().spectrumStage == SpectrumStage::Collecting);
    }

    // 7. Подготовка спектра отменена: дрон вне области (ID19)
    {
        Receiver r;
        r.passCheck();
        r.msg(0x14);
        r.answer(0x09, CommandResult::Done, 3);
        r.msg(0x13);
        check("ID19 во время подготовки — подготовка отменена, сообщения о наборе нет",
              r.state.status().spectrumStage == SpectrumStage::None && r.spectrumEnds.isEmpty());
    }

    // 8. Радиомолчание
    {
        Receiver r;
        r.passCheck();
        r.msg(0x14);
        r.answer(0x0D);
        const bool on = r.state.status().radioSilence;
        r.answer(0x0E);
        check("ID01 на ID13 / ID14 — радиомолчание вкл / выкл",
              on && !r.state.status().radioSilence && r.silence == QVector<bool>({true, false}));
    }

    // 9. Авария по ID25 (LIDAR) во время набора спектра, затем перезапуск логгера
    {
        Receiver r;
        r.passCheck();
        r.msg(0x14);
        r.answer(0x09, CommandResult::Done, 3);
        r.msg(0x1B);
        r.msg(0x19, {{"device", 1}});               // ID25, LIDAR
        const LoggerStatus s = r.state.status();
        check("ID25 (LIDAR) — «авария», LIDAR «ошибка», набор закрыт по аварии, разрешена только ID01",
              s.mode == LoggerMode::Error && s.errorMsgId == 0x19 && s.errorDevice == 1
              && r.device(LoggerDevice::Lidar) == DeviceCheck::Fail
              && r.spectrumEnds == QVector<SpectrumStopReason>{SpectrumStopReason::LoggerError}
              && r.errors == QVector<quint8>{0x19} && r.allowed() == QList<quint8>{1});

        r.answer(0x01, CommandResult::Forbidden);   // логгер ещё в аварии
        const bool noMismatch = r.mismatches == 0 && r.mode() == LoggerMode::Error;
        r.answer(0x01);                             // после перезапуска питания
        check("ID03 на ID01 в аварии — не «режим не совпадает»", noMismatch);
        check("ID01 на ID01 после перезапуска — «проверка», приборы сброшены",
              r.mode() == LoggerMode::Test && r.device(LoggerDevice::Lidar) == DeviceCheck::NotChecked
              && r.state.status().errorMsgId == 0);
    }

    // 10. ID26: при проверке — ошибка часов (GPS), в полёте — авария
    {
        Receiver r1, r2;
        r1.answer(0x01);
        r1.msg(0x1A);
        r2.passCheck();
        r2.msg(0x1A);
        check("ID26 при проверке — GPS «ошибка», режим «проверка»",
              r1.mode() == LoggerMode::Test && r1.device(LoggerDevice::Gps) == DeviceCheck::Fail);
        check("ID26 в ожидании — «авария»", r2.mode() == LoggerMode::Error);
    }

    // 11. Режим не совпадает
    {
        Receiver r;
        r.passCheck();
        r.msg(0x14);
        r.answer(0x0A, CommandResult::Forbidden);   // ID10 в разведке разрешена, а логгер ответил ID03
        const bool mismatch = r.state.status().modeMismatch && r.mismatches == 1;
        r.msg(0x16);                                // логгер прислал измерение — режим подтвердился
        check("ID03 на разрешённую ID10 — «режим не совпадает»", mismatch);
        check("Событие логгера (ID22) снимает «режим не совпадает»", !r.state.status().modeMismatch);

        Receiver e;
        e.passCheck();
        e.answer(0x09, CommandResult::Forbidden, 3);    // ID09 = 3 в ожидании — законный ID03
        check("ID03 на ID09 = 3 в ожидании — не «режим не совпадает»", e.mismatches == 0);
    }

    // 12. Напряжение из измерения и сброс
    {
        Receiver r;
        r.passCheck();
        Measurement m;
        m.battV = 14.8f;
        r.state.onMeasurement(m);
        const bool volt = qAbs(r.state.status().battV - 14.8f) < 0.01f;
        r.state.reset();
        check("Напряжение из ID22 — 14,8 В", volt);
        check("Сброс — «неизвестно», приборы «не проверялись», все команды разрешены",
              r.mode() == LoggerMode::Unknown && r.device(LoggerDevice::Gps) == DeviceCheck::NotChecked
              && r.allowed().size() == 14);
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
