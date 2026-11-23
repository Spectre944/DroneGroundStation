#ifndef CORE_STUB_H
#define CORE_STUB_H

/**
    @file  core_stub.h
    @brief Заглушка ядра: тестовые данные вместо настоящих классов ядра.

    Изображает всё ядро, пока настоящих классов нет. Нужна, чтобы подключить
    и проверить интерфейс без дрона и без COM-порта.

    @note  Создаёт DroneCore и переносит в поток ядра. Методы заглушки
           вызываются только из DroneCore (через QMetaObject::invokeMethod),
           сигналы заглушки соединены с одноимёнными сигналами DroneCore.
    @note  COM-порт не открывает, базу и файлы не пишет.
*/

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QStringList>

#include "core_types.h"

class CoreStub : public QObject
{
    Q_OBJECT

public:
    /**
        @brief Создаёт заглушку; таймеры создаются здесь, а запускаются
               уже в потоке ядра (в openPort и importSd).
    */
    explicit CoreStub(QObject *parent = nullptr);

    // ---------------------------------------------------------------- вызовы из DroneCore
    // Те же методы, что у DroneCore; выполняются в потоке ядра.

    void openPort(const QString &name);                 ///< «открыть порт»: запуск тестового сценария
    void closePort();                                   ///< «закрыть порт»: остановка сценария
    void sendCommand(const CommandRequest &request);    ///< через 1 с — ответ Done
    void checkArea(const QVector<GeoPoint> &vertices, bool isRoute);    ///< проверка числа вершин и линии
    void startSession(const QString &name);             ///< полёт № 100
    void finishSession();
    void listSessions();                                ///< 3 тестовых полёта
    void openSession(qint64 sessionId);                 ///< данные тестового полёта
    void queryArchive(const SessionFilter &filter);     ///< точки тестовых полётов
    void requestTimeSeries(qint64 sessionId);           ///< измерения тестового полёта
    void importSd(const QStringList &files);            ///< прогресс 0–100 % за 3 с, затем отчёт
    void exportCsv(qint64 sessionId, const QString &folder, quint32 parts);    ///< сразу «готово», файлы не пишет
    void applySettings(const CoreSettings &settings);   ///< запоминает пороги и параметры сетки
    void acknowledgeAlarm(qint64 alarmId);              ///< сразу alarmAcknowledged

signals:
    // Те же сигналы, что у DroneCore — описание в drone_core.h
    void linkStatusChanged(const LinkStatus &status);
    void loggerStatusChanged(const LoggerStatus &status);
    void allowedCommandsChanged(const QList<quint8> &ids);
    void routePoint(const RoutePoint &point);
    void measurement(const Measurement &m);
    void spectrumUpdated(const Spectrum &spectrum);
    void spectrumFinished(const Spectrum &spectrum);
    void gridUpdated(const GridData &grid);
    void alarm(const Alarm &a);
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
    void exportFinished(bool ok, const QString &path);
    void alarmAcknowledged(qint64 alarmId);

private:
    /**
        @brief Этапы тестового сценария. Идут по кругу.
    */
    enum class Phase {
        Test,       ///< проверка ~10 с: приборы по очереди «исправен», ID18
        Expect,     ///< ожидание 20 с: ID19 раз в 5 с, полёт к области
        Work,       ///< разведка: ID20, затем ID22 раз в 2 с «змейкой» по области
        Spectrum,   ///< набор спектра: ID27, порции раз в 3 с
        Return      ///< возвращение 15 с: ID19 раз в 5 с, полёт от области
    };

    // ---- сценарий ----
    void onTick();                  ///< раз в секунду: шаг сценария и состояние связи
    void stepTest();
    void stepExpect();
    void stepWork();
    void stepSpectrum();
    void stepReturn();
    void setPhase(Phase phase);     ///< сменить этап: режим логгера, разрешённые команды

    // ---- выдача данных ----
    void sendLink();                ///< linkStatusChanged
    void sendStatus();              ///< loggerStatusChanged
    void sendAllowed();             ///< allowedCommandsChanged по режиму и этапу спектра
    void sendRoute(quint8 msgId, bool inArea);  ///< routePoint в текущей позиции
    void sendEvent(EventCode code, const QVariantMap &params = {});
    void sendMeasurement();         ///< measurement, тревоги, сетка
    void checkDoseAlarm(const Measurement &m);  ///< тревога один раз при входе в зону выше порога
    void sendGrid();                ///< gridUpdated по измерениям текущего полёта
    void onSdTimer();               ///< шаг прогресса импорта SD

    // ---- тестовые данные ----
    static QVector<GeoPoint> testArea();        ///< квадрат ~1 × 1 км
    static GeoPoint areaCenter();
    static GeoPoint snakePoint(int index);      ///< точка «змейки» по области
    static int snakeCount();                    ///< число точек «змейки»
    static float doseAt(const GeoPoint &p);     ///< ПАЕД: фон + «горячее пятно» в центре
    static Spectrum makeTestSpectrum(quint16 number, int portions);
    static SpectrumPortion makePortion();
    SessionData makeTestFlight(qint64 sessionId) const;    ///< данные прошлого полёта
    QVector<SessionInfo> testSessions() const;
    Measurement makeMeasurement(const GeoPoint &p, const QDateTime &time) const;

    QTimer       *m_tick = nullptr;     ///< такт сценария, 1 с
    QTimer       *m_sdTimer = nullptr;  ///< такт прогресса импорта SD

    bool          m_portOpen = false;
    QString       m_portName;
    int           m_sinceLastPacket = -1;   ///< секунд с последнего «пакета»

    Phase         m_phase = Phase::Test;
    int           m_phaseSec = 0;       ///< секунд от начала этапа
    int           m_workIndex = 0;      ///< номер точки «змейки»
    GeoPoint      m_pos;                ///< текущее положение дрона

    LoggerStatus  m_status;             ///< состояние логгера, которое выдаёт заглушка
    CoreSettings  m_settings;           ///< последние настройки (пороги, сетка)

    QVector<Measurement> m_measurements;    ///< измерения текущего круга — для сетки
    Spectrum      m_spectrum;           ///< набираемый спектр
    quint16       m_spectrumNumber = 0;

    bool          m_aboveWarn = false;  ///< сейчас выше порога предупреждения
    bool          m_aboveAlarm = false; ///< сейчас выше порога тревоги
    qint64        m_nextId = 1;         ///< номера измерений, тревог, событий

    SessionInfo   m_session;            ///< текущий полёт
    bool          m_sessionActive = false;

    int           m_sdPercent = 0;
    QStringList   m_sdFiles;
};

#endif // CORE_STUB_H
