#include "alarm_manager.h"

namespace {
    constexpr quint8 MsgSdFault = 0x10;     ///< ID16 — SD-карта неисправна
}

AlarmManager::AlarmManager(QObject *parent)
    : QObject(parent)
{
}

// ============================================================================
//  ПАЕД
// ============================================================================

void AlarmManager::onMeasurement(const Measurement &m)
{
    m_pos = { m.lat, m.lon };
    m_hasPos = true;

    if (m.flags != MfNone)                  // недостоверное — не повод для тревоги
        return;

    const float v = m.shownPaed();

    if (v >= m_alarm) {
        if (m_aboveAlarm)
            return;                         // уже в зоне тревоги — не повторять
        m_aboveAlarm = m_aboveWarn = true;
        raise(AlarmType::DoseRate, AlarmLevel::Alarm, m.time, v);
    } else if (v >= m_warn) {
        m_aboveAlarm = false;               // опустились ниже тревоги — следующий подъём снова даст тревогу
        if (m_aboveWarn)
            return;                         // уже в зоне предупреждения
        m_aboveWarn = true;
        raise(AlarmType::DoseRate, AlarmLevel::Warning, m.time, v);
    } else {
        m_aboveWarn = m_aboveAlarm = false; // вышли из зоны — следующий вход снова даст тревогу
    }
}

void AlarmManager::onRoutePoint(const RoutePoint &p)
{
    m_pos = { p.lat, p.lon };
    m_hasPos = true;
}

// ============================================================================
//  Авария, SD, режим
// ============================================================================

void AlarmManager::onLoggerError(quint8 msgId)
{
    if (m_errorRaised)                      // из аварии — только выключением питания; одна тревога
        return;
    m_errorRaised = true;
    raise(AlarmType::LoggerError, AlarmLevel::Alarm, QDateTime::currentDateTimeUtc(), 0.0f, msgId);
}

void AlarmManager::onEvent(const CoreEvent &e)
{
    if (e.code == EventCode::SdFault && !m_sdRaised) {
        m_sdRaised = true;
        raise(AlarmType::SdError, AlarmLevel::Warning, e.time, 0.0f, MsgSdFault);
    }
}

void AlarmManager::onModeMismatch(const CommandRequest &request)
{
    raise(AlarmType::ModeMismatch, AlarmLevel::Warning, QDateTime::currentDateTimeUtc(), 0.0f, request.id);
}

// ============================================================================
//  Связь
// ============================================================================

void AlarmManager::onLinkStatus(const LinkStatus &status)
{
    switch (status.state) {
    case LinkState::Lost:
        if (!m_linkLost) {
            m_linkLost = true;
            sendEvent(EventCode::LinkLost);
            raise(AlarmType::LinkLost, AlarmLevel::Alarm, QDateTime::currentDateTimeUtc());
        }
        break;
    case LinkState::Ok:
        if (m_linkLost) {                   // пакеты снова идут
            m_linkLost = false;
            sendEvent(EventCode::LinkRestored);
        }
        break;
    case LinkState::Closed:
        m_linkLost = false;                 // порт закрыт оператором — это не потеря связи
        break;
    default:
        break;                              // ждём пакеты, радиомолчание — тревоги нет
    }
}

// ============================================================================
//  Настройки и сброс
// ============================================================================

void AlarmManager::setThresholds(float warn, float alarm)
{
    m_warn = warn;
    m_alarm = alarm;
}

void AlarmManager::reset()
{
    m_aboveWarn = m_aboveAlarm = false;
    m_errorRaised = m_sdRaised = m_linkLost = false;
    m_hasPos = false;
}

// ============================================================================
//  Внутреннее
// ============================================================================

void AlarmManager::raise(AlarmType type, AlarmLevel level, const QDateTime &time, float value, quint8 msgId)
{
    Alarm a;
    a.type   = type;
    a.level  = level;
    a.time   = time.isValid() ? time : QDateTime::currentDateTimeUtc();
    a.hasPos = m_hasPos;
    a.lat    = m_pos.lat;
    a.lon    = m_pos.lon;
    a.value  = value;
    a.msgId  = msgId;
    emit alarm(a);
}

void AlarmManager::sendEvent(EventCode code)
{
    CoreEvent e;
    e.code = code;
    e.time = QDateTime::currentDateTimeUtc();
    emit event(e);
}
