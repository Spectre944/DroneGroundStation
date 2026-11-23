#include "core_engine.h"
#include "drone_link.h"
#include "message_parser.h"
#include "drone_commands.h"
#include "logger_state.h"
#include "recon_area.h"
#include "session_store.h"
#include "spectrum_collector.h"

#include <QCoreApplication>
#include <QDir>

namespace {
    constexpr quint8 CmdArea = 0x04;        ///< ID04 — параметры разведки
    constexpr quint8 CmdSpectrumTime = 0x0B;    ///< ID11 — время набора спектра
    const char DefaultDbFile[] = "flights.db";  ///< база полётов по умолчанию — рядом с программой
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
    m_store    = new SessionStore(this);
    m_spectra  = new SpectrumCollector(this);

    // ---- приём: порт → база, разбор ----
    connect(m_link, &DroneLink::rawData,            m_store,  &SessionStore::saveRaw);      // сырой журнал (Ф1)
    connect(m_link, &DroneLink::packetReceived,     m_store,  &SessionStore::savePacket);   // пакет — для сверки с SD
    connect(m_link, &DroneLink::packetReceived,     m_parser, &MessageParser::parse);
    connect(m_link, &DroneLink::linkStatusChanged,  this,     &CoreEngine::linkStatusChanged);
    connect(m_link, &DroneLink::portError,          this,     &CoreEngine::onPortError);

    // ---- разбор → команды, состояние, база и наружу ----
    connect(m_parser, &MessageParser::answerReceived, m_commands, &DroneCommands::onAnswer);
    connect(m_parser, &MessageParser::loggerMessage,  m_state,    &LoggerState::onMessage);
    connect(m_parser, &MessageParser::routePoint,     this,       &CoreEngine::onRoutePoint);
    connect(m_parser, &MessageParser::measurement,    this,       &CoreEngine::onMeasurement);
    connect(m_parser, &MessageParser::event,          this,       &CoreEngine::onEvent);
    connect(m_parser, &MessageParser::spectrumStarted, m_spectra, &SpectrumCollector::onStart);
    connect(m_parser, &MessageParser::spectrumPortion, m_spectra, &SpectrumCollector::onPortion);

    // ---- команды → состояние, база, наружу ----
    connect(m_commands, &DroneCommands::commandFinished, this, &CoreEngine::onCommandFinished);
    connect(m_commands, &DroneCommands::event,           this, &CoreEngine::onEvent);

    // ---- состояние → команды, порт, разбор, наружу ----
    connect(m_state, &LoggerState::allowedCommandsChanged, m_commands, &DroneCommands::setAllowed);
    connect(m_state, &LoggerState::radioSilenceChanged,    m_commands, &DroneCommands::setRadioSilence);
    connect(m_state, &LoggerState::radioSilenceChanged,    m_link,     &DroneLink::setQuietExpected);
    connect(m_state, &LoggerState::modeChanged,            m_parser,   &MessageParser::setLoggerMode);
    connect(m_state, &LoggerState::statusChanged,          this,       &CoreEngine::onLoggerStatus);
    connect(m_state, &LoggerState::allowedCommandsChanged, this,       &CoreEngine::allowedCommandsChanged);
    connect(m_state, &LoggerState::spectrumFinished,       m_spectra,  &SpectrumCollector::finish);

    // ---- спектры → состояние, база, наружу ----
    connect(m_spectra, &SpectrumCollector::stoppedByItself, m_state, &LoggerState::onSpectrumStopped);
    connect(m_spectra, &SpectrumCollector::spectrumUpdated,  this,   &CoreEngine::spectrumUpdated);
    connect(m_spectra, &SpectrumCollector::spectrumFinished, this,   &CoreEngine::onSpectrumFinished);

    // ---- база → наружу ----
    connect(m_store, &SessionStore::sessionStarted,  this, &CoreEngine::sessionStarted);
    connect(m_store, &SessionStore::sessionFinished, this, &CoreEngine::sessionFinished);
    connect(m_store, &SessionStore::sessionsListed,  this, &CoreEngine::sessionsListed);
    connect(m_store, &SessionStore::sessionData,     this, &CoreEngine::sessionData);
    connect(m_store, &SessionStore::archiveResult,   this, &CoreEngine::archiveResult);
    connect(m_store, &SessionStore::timeSeriesReady, this, &CoreEngine::timeSeriesReady);
    connect(m_store, &SessionStore::dbError, this, [this](const QString &text) {
        emit coreError(QStringLiteral("db"), text);
    });
}

// ============================================================================
//  База
// ============================================================================

/**
    @brief Открыть базу при первой надобности: путь из настроек,
           иначе flights.db рядом с программой.
*/
bool CoreEngine::ensureDb()
{
    if (m_store->isOpen())
        return true;
    const QString path = m_settings.dbPath.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1(DefaultDbFile))
        : m_settings.dbPath;
    if (!m_store->open(path))
        return false;
    m_store->setThresholds(m_settings.warnLevel, m_settings.alarmLevel);
    return true;
}

// ============================================================================
//  Порт
// ============================================================================

void CoreEngine::openPort(const QString &name)
{
    // Новый сеанс связи: что было с логгером раньше — неизвестно
    m_state->reset();
    m_spectra->reset();
    m_acceptedArea.clear();
    ensureDb();                             // дальше всё принятое — в базу

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
//  Данные от логгера: в базу, затем наружу — с номером записи
// ============================================================================

void CoreEngine::onRoutePoint(const RoutePoint &point)
{
    m_store->saveRoutePoint(point);
    emit routePoint(point);
}

void CoreEngine::onMeasurement(const Measurement &m)
{
    Measurement saved = m;
    saved.id = m_store->saveMeasurement(m);     // номер в базе — интерфейс по нему выделяет точку
    m_state->onMeasurement(saved);
    emit measurement(saved);
}

void CoreEngine::onEvent(const CoreEvent &e)
{
    CoreEvent saved = e;
    saved.id = m_store->saveEvent(e);
    emit event(saved);
}

void CoreEngine::onSpectrumFinished(const Spectrum &spectrum)
{
    Spectrum saved = spectrum;
    saved.info.id = m_store->saveSpectrum(spectrum);   // итог и все порции — одной транзакцией
    emit spectrumFinished(saved);
}

void CoreEngine::onLoggerStatus(const LoggerStatus &status)
{
    m_store->setNoSd(status.noSd);          // полёт без SD — отметка в списке полётов
    emit loggerStatusChanged(status);
}

// ============================================================================
//  Команды и область
// ============================================================================

void CoreEngine::sendCommand(const CommandRequest &request)
{
    m_commands->send(request);
}

/**
    @brief Итог команды: сначала LoggerState (режим), затем база, затем наружу.
    @note  ID01 на ID04 — область принята логгером: запоминается и пишется в базу.
*/
void CoreEngine::onCommandFinished(const CommandOutcome &outcome)
{
    m_state->onCommandFinished(outcome);
    m_store->saveCommand(outcome);

    if (outcome.request.id == CmdArea && outcome.result == CommandResult::Done) {
        m_acceptedArea = outcome.request.vertices;
        m_store->saveArea(outcome.request, outcome.finishTime);
        m_spectra->setDuration(outcome.request.spectrumS);     // время набора спектра из ID04
    }
    if (outcome.request.id == CmdSpectrumTime && outcome.result == CommandResult::Done)
        m_spectra->setDuration(quint16(outcome.request.value)); // ID11

    emit commandFinished(outcome);
}

void CoreEngine::checkArea(const QVector<GeoPoint> &vertices, bool isRoute)
{
    emit areaChecked(ReconArea::check(vertices, isRoute, m_settings.maxAreaKm));
}

// ============================================================================
//  Полёты и архив
// ============================================================================

void CoreEngine::startSession(const QString &name)
{
    if (!ensureDb())
        return;
    m_store->startSession(name);
    m_store->saveSettings(m_settings);      // с какими настройками шёл полёт
}

void CoreEngine::finishSession()
{
    m_store->finishSession();
}

void CoreEngine::listSessions()
{
    if (ensureDb())
        m_store->listSessions();
}

void CoreEngine::openSession(qint64 sessionId)
{
    if (ensureDb())
        m_store->loadSession(sessionId);
}

void CoreEngine::queryArchive(const SessionFilter &filter)
{
    if (ensureDb())
        m_store->query(filter);
}

void CoreEngine::requestTimeSeries(qint64 sessionId)
{
    if (ensureDb())
        m_store->timeSeries(sessionId);
}

// ============================================================================
//  Настройки
// ============================================================================

void CoreEngine::applySettings(const CoreSettings &settings)
{
    m_settings = settings;
    m_link->setLinkTimeout(settings.linkTimeoutS);
    m_commands->setMaxAreaKm(settings.maxAreaKm);
    m_store->setThresholds(settings.warnLevel, settings.alarmLevel);
    m_store->saveSettings(settings);        // идёт полёт — новые настройки тоже в базу
}

// ============================================================================
//  События ядра (порт открыт, закрыт, ошибка)
// ============================================================================

void CoreEngine::sendEvent(EventCode code, const QVariantMap &params)
{
    CoreEvent e;
    e.code = code;
    e.time = QDateTime::currentDateTimeUtc();
    e.params = params;

    // «Порт закрыт» после «Завершить полёт» — не повод начинать новый полёт в базе
    if (code == EventCode::PortClosed && !m_store->hasSession()) {
        emit event(e);
        return;
    }
    onEvent(e);
}
