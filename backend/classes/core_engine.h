#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

/**
    @file  core_engine.h
    @brief Настоящее ядро: цепочка DroneLink → MessageParser → LoggerState,
           DroneCommands, ReconArea, SessionStore, SpectrumCollector, AlarmManager,
           SdImporter —
           соединённые по схеме
           «Классов ядра», разд. 4.

    Устроено как заглушка CoreStub: те же методы и сигналы. DroneCore
    выбирает, с кем работать: «STUB» — заглушка, имя COM-порта — CoreEngine.

    @note  Живёт в потоке ядра (DroneCore переносит его через moveToThread).
           Все классы цепочки — его дочерние объекты и переезжают вместе с ним.
    @note  Всё принятое и отправленное записывается в базу полётов (SessionStore)
           в момент события. Номер записи в базе (id) попадает в измерение и событие,
           которые уходят в интерфейс.
    @note  Пока без пересчёта на 1 м, сетки и экспорта —
           они подключаются на следующих шагах.
*/

#include <QObject>
#include <QVector>

#include "core_types.h"

class DroneLink;
class MessageParser;
class DroneCommands;
class LoggerState;
class SessionStore;
class SpectrumCollector;
class AlarmManager;
class SdImporter;

class CoreEngine : public QObject
{
    Q_OBJECT

public:
    /**
        @param link Для проверки (engine_test) — подставной DroneLink, владелец — вызывающий.
                    nullptr (обычно) — CoreEngine создаёт свой.
    */
    explicit CoreEngine(DroneLink *link = nullptr, QObject *parent = nullptr);

    // ---------------------------------------------------------------- вызовы из DroneCore
    // Выполняются в потоке ядра.

    void openPort(const QString &name);                 ///< открыть COM-порт; состояние логгера сбрасывается
    void closePort();
    void sendCommand(const CommandRequest &request);    ///< → DroneCommands
    void checkArea(const QVector<GeoPoint> &vertices, bool isRoute);   ///< → ReconArea, ответ areaChecked
    void applySettings(const CoreSettings &settings);   ///< раздать настройки классам

    void startSession(const QString &name);             ///< «Новый полёт»
    void finishSession();                               ///< «Завершить полёт»
    void listSessions();                                ///< → sessionsListed
    void openSession(qint64 sessionId);                 ///< → sessionData
    void queryArchive(const SessionFilter &filter);     ///< → archiveResult
    void requestTimeSeries(qint64 sessionId);           ///< → timeSeriesReady
    void acknowledgeAlarm(qint64 alarmId);              ///< оператор увидел тревогу → база, alarmAcknowledged
    void importSd(const QStringList &files);            ///< файлы SD → сверка, база; ход — sdProgress, итог — sdImported

    QVector<GeoPoint> acceptedArea() const { return m_acceptedArea; }   ///< область, принятая логгером (ID01 на ID04)
    SessionStore *store() const { return m_store; }     ///< для проверки (engine_test, session_test)

signals:
    // Те же сигналы, что у DroneCore (часть) — описание в drone_core.h
    void linkStatusChanged(const LinkStatus &status);
    void loggerStatusChanged(const LoggerStatus &status);
    void allowedCommandsChanged(const QList<quint8> &ids);
    void routePoint(const RoutePoint &point);
    void measurement(const Measurement &m);
    void spectrumUpdated(const Spectrum &spectrum);
    void spectrumFinished(const Spectrum &spectrum);
    void alarm(const Alarm &a);
    void alarmAcknowledged(qint64 alarmId);
    void event(const CoreEvent &e);
    void coreError(const QString &source, const QString &text);
    void commandFinished(const CommandOutcome &outcome);
    void areaChecked(const AreaCheck &check);

    void sessionStarted(const SessionInfo &info);
    void sessionFinished(const SessionInfo &info);
    void sessionsListed(const QVector<SessionInfo> &list);
    void sessionData(const SessionData &data);
    void archiveResult(const ArchiveResult &result);
    void timeSeriesReady(qint64 sessionId, const QVector<Measurement> &series);
    void sdProgress(int percent);
    void sdImported(const SdReport &report);

private:
    bool ensureDb();                                    ///< база открыта? если нет — открыть (путь из настроек)
    void onCommandFinished(const CommandOutcome &outcome);
    void onRoutePoint(const RoutePoint &point);
    void onMeasurement(const Measurement &m);
    void onEvent(const CoreEvent &e);
    void onSpectrumFinished(const Spectrum &spectrum);
    void onAlarm(const Alarm &a);
    void onLoggerStatus(const LoggerStatus &status);
    void onPortError(const QString &text);
    void sendEvent(EventCode code, const QVariantMap &params = {});

    DroneLink     *m_link = nullptr;
    MessageParser *m_parser = nullptr;
    DroneCommands *m_commands = nullptr;
    LoggerState   *m_state = nullptr;
    SessionStore  *m_store = nullptr;
    SpectrumCollector *m_spectra = nullptr;
    AlarmManager  *m_alarms = nullptr;
    SdImporter    *m_sd = nullptr;

    qint64         m_localAlarmId = 0;      ///< номер тревоги, если база не открыта: −1, −2, … (не пересекаются с номерами базы)

    CoreSettings      m_settings;           ///< последние настройки
    QVector<GeoPoint> m_acceptedArea;       ///< принятая логгером область
};

#endif // CORE_ENGINE_H
