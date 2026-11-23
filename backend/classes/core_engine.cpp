#include "core_engine.h"
#include "drone_link.h"
#include "message_parser.h"
#include "drone_commands.h"
#include "logger_state.h"
#include "recon_area.h"

namespace {
    constexpr quint8 CmdArea = 0x04;        ///< ID04 — параметры разведки
}

/**
    @brief Создание классов цепочки и соединения между ними.
    @note  Классы создаются дочерними (родитель — this): при moveToThread
           переезжают в поток ядра вместе с CoreEngine и удаляются вместе с ним.
*/
CoreEngine::CoreEngine(DroneLink *link, QObject *parent)
    : QObject(parent)
{
    m_link     = link ? link : new DroneLink(this);
    m_parser   = new MessageParser(this);
    m_commands = new DroneCommands(m_link, this);
    m_state    = new LoggerState(this);

    // ---- приём: порт → разбор ----
    connect(m_link, &DroneLink::packetReceived,     m_parser, &MessageParser::parse);
    connect(m_link, &DroneLink::linkStatusChanged,  this,     &CoreEngine::linkStatusChanged);
    connect(m_link, &DroneLink::portError,          this,     &CoreEngine::onPortError);

    // ---- разбор → команды, состояние, наружу ----
    connect(m_parser, &MessageParser::answerReceived, m_commands, &DroneCommands::onAnswer);
    connect(m_parser, &MessageParser::loggerMessage,  m_state,    &LoggerState::onMessage);
    connect(m_parser, &MessageParser::measurement,    m_state,    &LoggerState::onMeasurement);
    connect(m_parser, &MessageParser::routePoint,     this,       &CoreEngine::routePoint);
    connect(m_parser, &MessageParser::measurement,    this,       &CoreEngine::measurement);
    connect(m_parser, &MessageParser::event,          this,       &CoreEngine::event);

    // ---- команды → состояние, наружу ----
    connect(m_commands, &DroneCommands::commandFinished, this, &CoreEngine::onCommandFinished);
    connect(m_commands, &DroneCommands::event,           this, &CoreEngine::event);

    // ---- состояние → команды, порт, разбор, наружу ----
    connect(m_state, &LoggerState::allowedCommandsChanged, m_commands, &DroneCommands::setAllowed);
    connect(m_state, &LoggerState::radioSilenceChanged,    m_commands, &DroneCommands::setRadioSilence);
    connect(m_state, &LoggerState::radioSilenceChanged,    m_link,     &DroneLink::setQuietExpected);
    connect(m_state, &LoggerState::modeChanged,            m_parser,   &MessageParser::setLoggerMode);
    connect(m_state, &LoggerState::statusChanged,          this,       &CoreEngine::loggerStatusChanged);
    connect(m_state, &LoggerState::allowedCommandsChanged, this,       &CoreEngine::allowedCommandsChanged);
}

// ============================================================================
//  Порт
// ============================================================================

void CoreEngine::openPort(const QString &name)
{
    // Новый сеанс связи: что было с логгером раньше — неизвестно
    m_state->reset();
    m_acceptedArea.clear();

    if (m_link->open(name))
        sendEvent(EventCode::PortOpened, {{QStringLiteral("port"), name}});
    // ошибка — через DroneLink::portError → onPortError
}

void CoreEngine::closePort()
{
    m_link->close();
    sendEvent(EventCode::PortClosed);
}

void CoreEngine::onPortError(const QString &text)
{
    sendEvent(EventCode::PortError, {{QStringLiteral("text"), text}});
    emit coreError(QStringLiteral("port"), text);
}

// ============================================================================
//  Команды и область
// ============================================================================

void CoreEngine::sendCommand(const CommandRequest &request)
{
    m_commands->send(request);
}

/**
    @brief Итог команды: сначала LoggerState (режим), затем наружу.
    @note  ID01 на ID04 — область принята логгером: запоминается.
*/
void CoreEngine::onCommandFinished(const CommandOutcome &outcome)
{
    m_state->onCommandFinished(outcome);

    if (outcome.request.id == CmdArea && outcome.result == CommandResult::Done)
        m_acceptedArea = outcome.request.vertices;

    emit commandFinished(outcome);
}

void CoreEngine::checkArea(const QVector<GeoPoint> &vertices, bool isRoute)
{
    emit areaChecked(ReconArea::check(vertices, isRoute, m_maxAreaKm));
}

// ============================================================================
//  Настройки
// ============================================================================

void CoreEngine::applySettings(const CoreSettings &settings)
{
    m_maxAreaKm = settings.maxAreaKm;
    m_link->setLinkTimeout(settings.linkTimeoutS);
    m_commands->setMaxAreaKm(settings.maxAreaKm);
}

// ============================================================================
//  События
// ============================================================================

void CoreEngine::sendEvent(EventCode code, const QVariantMap &params)
{
    CoreEvent e;
    e.code = code;
    e.time = QDateTime::currentDateTimeUtc();
    e.params = params;
    emit event(e);
}
