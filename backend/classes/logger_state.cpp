#include "logger_state.h"

/**
    @brief ID сообщений и команд, которыми пользуется LoggerState.
*/
namespace {
    // ---- сообщения логгера ----
    constexpr quint8 MsgBatteryLow   = 0x04;    ///< ID04 аккумулятор разряжен
    constexpr quint8 MsgBatteryOk    = 0x05;    ///< ID05 аккумулятор заряжен
    constexpr quint8 MsgBatteryFault = 0x06;    ///< ID06 ошибка измерения
    constexpr quint8 MsgBdbgBad      = 0x07;    ///< ID07 БДБГ ответил некорректно
    constexpr quint8 MsgBdbgNoAnswer = 0x08;    ///< ID08 БДБГ не ответил
    constexpr quint8 MsgBdbgOk       = 0x09;    ///< ID09 БДБГ исправен
    constexpr quint8 MsgLidarOk      = 0x0B;    ///< ID11 LIDAR исправен
    constexpr quint8 MsgLidarFault   = 0x0C;    ///< ID12 LIDAR неисправен
    constexpr quint8 MsgGpsFault     = 0x0F;    ///< ID15 GPS неисправен
    constexpr quint8 MsgSdFault      = 0x10;    ///< ID16 SD-карта неисправна
    constexpr quint8 MsgCheckPassed  = 0x12;    ///< ID18 проверка пройдена
    constexpr quint8 MsgCoordOut     = 0x13;    ///< ID19 координата, вне области
    constexpr quint8 MsgCoordIn      = 0x14;    ///< ID20 вход в область
    constexpr quint8 MsgMeasurement  = 0x16;    ///< ID22 измерение
    constexpr quint8 MsgSpoofing     = 0x18;    ///< ID24 спуфинг
    constexpr quint8 MsgDeviceFailed = 0x19;    ///< ID25 прибор неисправен
    constexpr quint8 MsgClockError   = 0x1A;    ///< ID26 ошибка часов
    constexpr quint8 MsgSpectrumStart= 0x1B;    ///< ID27 начало набора спектра
    constexpr quint8 MsgSpectrumFail = 0x1C;    ///< ID28 спектр не запустился

    // ---- команды ----
    constexpr quint8 CmdCheck        = 0x01;    ///< ID01 предполётная проверка
    constexpr quint8 CmdNoSd         = 0x02;    ///< ID02 продолжить без SD
    constexpr quint8 CmdArea         = 0x04;    ///< ID04 параметры разведки
    constexpr quint8 CmdFinish       = 0x08;    ///< ID08 закончить разведку
    constexpr quint8 CmdReconMode    = 0x09;    ///< ID09 режим разведки (3 — спектр)
    constexpr quint8 CmdStopSpectrum = 0x0C;    ///< ID12 прекратить набор спектра
    constexpr quint8 CmdSilenceOn    = 0x0D;    ///< ID13 радиомолчание
    constexpr quint8 CmdSilenceOff   = 0x0E;    ///< ID14 выйти из радиомолчания

    constexpr int ReconModeSpectrum  = 3;       ///< ID09 = 3 — набор спектра
}

LoggerState::LoggerState(QObject *parent)
    : QObject(parent)
{
    m_allowed = allowedFor(m_status, false);
}

// ============================================================================
//  Сообщения логгера
// ============================================================================

void LoggerState::onMessage(quint8 id, const QVariantMap &params)
{
    const bool inCheck = (m_status.mode == LoggerMode::Test);

    switch (id) {
    // ---- предполётная проверка: приборы ----
    case MsgCoordOut:                       // ID19
        if (inCheck) {
            setDevice(LoggerDevice::Gps, DeviceCheck::Ok, id);      // стартовая координата — GPS исправен
        } else if (m_status.spectrumStage == SpectrumStage::Preparing) {
            finishSpectrum(SpectrumStopReason::None);               // дрон вне области — подготовка отменена
        }
        break;
    case MsgGpsFault:                       // ID15
        setDevice(LoggerDevice::Gps, DeviceCheck::Fail, id);
        m_status.checking = false;          // проверка прекращена
        break;
    case MsgLidarOk:
        setDevice(LoggerDevice::Lidar, DeviceCheck::Ok, id);
        break;
    case MsgLidarFault:
        setDevice(LoggerDevice::Lidar, DeviceCheck::Fail, id);
        m_status.checking = false;
        break;
    case MsgBdbgOk:
        setDevice(LoggerDevice::Bdbg, DeviceCheck::Ok, id);
        break;
    case MsgBdbgBad:
    case MsgBdbgNoAnswer:
        setDevice(LoggerDevice::Bdbg, DeviceCheck::Fail, id);
        m_status.checking = false;
        break;
    case MsgBatteryOk:
    case MsgBatteryLow:
        m_status.battV = float(params.value(QStringLiteral("voltage")).toDouble());
        m_status.hasBattV = true;
        setDevice(LoggerDevice::Battery, id == MsgBatteryOk ? DeviceCheck::Ok : DeviceCheck::Fail, id);
        if (id == MsgBatteryLow)
            m_status.checking = false;
        break;
    case MsgBatteryFault:
        setDevice(LoggerDevice::Battery, DeviceCheck::Fail, id);
        m_status.checking = false;
        break;
    case MsgSdFault:                        // ID16: при проверке — ждать ID02 / ID03; в полёте — запись на SD прекращена
        setDevice(LoggerDevice::Sd, DeviceCheck::Fail, id);
        if (inCheck) {
            m_status.checking = false;
            m_afterSdError = true;
        } else {
            m_status.noSd = true;
        }
        break;
    case MsgCheckPassed:                    // ID18 — проверка пройдена, режим ожидания
        setDevice(LoggerDevice::Sd, DeviceCheck::Ok, id);
        m_status.checking = false;
        m_afterSdError = false;
        setMode(LoggerMode::Expect);
        break;

    // ---- полёт ----
    case MsgCoordIn:                        // ID20 — вход в область
        setMode(LoggerMode::Work);
        break;
    case MsgMeasurement:                    // ID22 — измерение
        setMode(LoggerMode::Work);          // ID22 бывает только в разведке — режим подтверждён
        if (m_status.spectrumStage == SpectrumStage::Collecting)
            finishSpectrum(SpectrumStopReason::Measurement);        // логгер вернулся к измерениям
        break;
    case MsgSpectrumStart:                  // ID27 — набор начат
        if (m_status.spectrumStage == SpectrumStage::Collecting)
            finishSpectrum(SpectrumStopReason::NewSpectrum);        // прежний не закрыт
        setMode(LoggerMode::Work);          // ID27 бывает только в разведке
        m_status.spectrumStage = SpectrumStage::Collecting;
        break;

    // ---- авария ----
    case MsgClockError:                     // ID26: при проверке — ошибка часов (GPS), иначе авария
        if (inCheck) {
            setDevice(LoggerDevice::Gps, DeviceCheck::Fail, id);
            m_status.checking = false;
        } else {
            enterError(id);
        }
        break;
    case MsgSpoofing:                       // ID24
    case MsgSpectrumFail:                   // ID28
        enterError(id);
        break;
    case MsgDeviceFailed: {                 // ID25: 0 БДБГ, 1 LIDAR, 2 GPS, 3 аккумулятор
        const int device = params.value(QStringLiteral("device")).toInt();
        static const LoggerDevice map[] = { LoggerDevice::Bdbg, LoggerDevice::Lidar,
                                            LoggerDevice::Gps,  LoggerDevice::Battery };
        if (device >= 0 && device <= 3)
            setDevice(map[device], DeviceCheck::Fail, id);
        m_status.errorDevice = quint8(device);
        enterError(id);
        break;
    }

    default:
        return;                             // ID21, ID29 и прочие — состояние не меняют
    }

    publish();
}

// ============================================================================
//  Итоги команд
// ============================================================================

void LoggerState::onCommandFinished(const CommandOutcome &outcome)
{
    const CommandRequest &r = outcome.request;

    // ---- ID03 «запрещено» ----
    if (outcome.result == CommandResult::Forbidden) {
        // Законные ID03, не говорящие о другом режиме:
        //  - ID01 в аварии (ID01 разрешена, чтобы заметить перезапуск логгера);
        //  - ID09 = 3 в ожидании (спектр только в разведке);
        //  - ID02 / ID03 — условие «после ID16» могло не выполниться.
        const bool legit = (r.id == CmdCheck && m_status.mode == LoggerMode::Error)
                        || (r.id == CmdReconMode && r.value == ReconModeSpectrum
                            && m_status.mode == LoggerMode::Expect)
                        || r.id == CmdNoSd || r.id == 0x03;
        if (!legit && m_status.mode != LoggerMode::Unknown && m_allowed.contains(r.id)) {
            m_status.modeMismatch = true;
            publish();
            emit modeMismatch(r);
        }
        return;
    }

    if (outcome.result != CommandResult::Done)
        return;                             // нет ответа, ошибка пакета, отказ — состояние не меняется

    // ---- ID01 «выполнено» ----
    m_status.modeMismatch = false;          // логгер подтвердил команду — режимы сходятся

    switch (r.id) {
    case CmdCheck: {                        // ID01 на ID01: проверка началась (или логгер перезапущен)
        LoggerStatus fresh;
        fresh.mode = LoggerMode::Test;
        fresh.checking = true;
        m_status = fresh;
        m_afterSdError = false;
        break;
    }
    case CmdNoSd:                           // ID01 на ID02: без SD, режим ожидания
        m_status.noSd = true;
        m_afterSdError = false;
        setMode(LoggerMode::Expect);
        break;
    case CmdArea:                           // ID01 на ID04: смена территории, режим ожидания
        finishSpectrum(SpectrumStopReason::NewArea);
        m_status.reconMode = r.reconMode;
        setMode(LoggerMode::Expect);
        break;
    case CmdFinish:                         // ID01 на ID08: возвращение
        finishSpectrum(SpectrumStopReason::CommandFinish);
        setMode(LoggerMode::Return);
        break;
    case CmdReconMode:                      // ID01 на ID09
        if (r.value == ReconModeSpectrum)
            m_status.spectrumStage = SpectrumStage::Preparing;
        else
            m_status.reconMode = ReconMode(r.value);
        break;
    case CmdStopSpectrum:                   // ID01 на ID12
        finishSpectrum(SpectrumStopReason::CommandStop);
        break;
    case CmdSilenceOn:                      // ID01 на ID13
        m_status.radioSilence = true;
        break;
    case CmdSilenceOff:                     // ID01 на ID14
        m_status.radioSilence = false;
        break;
    default:
        break;                              // ID03, ID05–ID07, ID10, ID11 — режим не меняют
    }

    publish();
}

void LoggerState::onMeasurement(const Measurement &m)
{
    m_status.battV = m.battV;
    m_status.hasBattV = true;
    publish();
}

void LoggerState::onSpectrumStopped()
{
    if (m_status.spectrumStage == SpectrumStage::None)
        return;
    m_status.spectrumStage = SpectrumStage::None;
    publish();
}


void LoggerState::reset()
{
    m_status = LoggerStatus();
    m_afterSdError = false;
    publish();
}

// ============================================================================
//  Внутреннее
// ============================================================================

void LoggerState::setDevice(LoggerDevice d, DeviceCheck check, quint8 msgId)
{
    m_status.devices[int(d)].check = check;
    m_status.devices[int(d)].msgId = msgId;
}

void LoggerState::setMode(LoggerMode mode)
{
    m_status.mode = mode;
    if (mode != LoggerMode::Error) {
        m_status.errorMsgId = 0;
    }
    m_status.modeMismatch = false;          // режим подтверждён событием логгера
}

void LoggerState::enterError(quint8 msgId)
{
    finishSpectrum(SpectrumStopReason::LoggerError);
    m_status.mode = LoggerMode::Error;
    m_status.errorMsgId = msgId;
    m_status.checking = false;
    emit enteredError(msgId);
}

void LoggerState::finishSpectrum(SpectrumStopReason reason)
{
    const bool collecting = (m_status.spectrumStage == SpectrumStage::Collecting);
    if (m_status.spectrumStage != SpectrumStage::None)
        m_status.spectrumStage = SpectrumStage::None;
    if (collecting)                         // подготовка без ID27 — набора не было, сообщать нечего
        emit spectrumFinished(reason);
}

void LoggerState::publish()
{
    emit statusChanged(m_status);

    if (m_status.mode != m_lastMode) {
        m_lastMode = m_status.mode;
        emit modeChanged(m_status.mode);
    }
    if (m_status.radioSilence != m_lastSilence) {
        m_lastSilence = m_status.radioSilence;
        emit radioSilenceChanged(m_status.radioSilence);
    }
    const QList<quint8> allowed = allowedFor(m_status, m_afterSdError);
    if (allowed != m_allowed) {
        m_allowed = allowed;
        emit allowedCommandsChanged(m_allowed);
    }
}

// ============================================================================
//  Матрица «команда × режим» (протокол, п. 5)
// ============================================================================

QList<quint8> LoggerState::allowedFor(const LoggerStatus &s, bool afterSdError)
{
    switch (s.mode) {
    case LoggerMode::Unknown:
        // Режим неизвестен (например, программу перезапустили в полёте) — не ограничиваем.
        // Ответы логгера покажут режим.
        return {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    case LoggerMode::Test:
        return afterSdError ? QList<quint8>{1, 2, 3} : QList<quint8>{1};
    case LoggerMode::Expect:
        return {4, 5, 6, 7, 8, 9, 11, 14};
    case LoggerMode::Work:
        switch (s.spectrumStage) {
        case SpectrumStage::Preparing:      // ID09–ID12 запрещены (ID12 — только во время набора)
            return {4, 5, 6, 7, 8, 13, 14};
        case SpectrumStage::Collecting:     // ID09–ID11 запрещены, ID12 разрешена
            return {4, 5, 6, 7, 8, 12, 13, 14};
        default:
            return {4, 5, 6, 7, 8, 9, 10, 11, 13, 14};
        }
    case LoggerMode::Return:
        return {4, 5, 14};
    case LoggerMode::Error:
        // По матрице в аварии всё — ID03. ID01 оставлена: после выключения и включения
        // питания логгер ждёт ID01, ответ ID01 покажет, что он перезапущен.
        return {1};
    }
    return {};
}
