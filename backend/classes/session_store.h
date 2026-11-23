#ifndef SESSION_STORE_H
#define SESSION_STORE_H

/**
    @file  session_store.h
    @brief База полётов SQLite (Ф12): запись в момент события, список полётов,
           открытие прошлого полёта, поиск по архиву.

    Один файл на все полёты (по умолчанию flights.db рядом с программой).
    Режим журнала WAL: каждая запись сразу на диске; при сбое программы
    или компьютера всё записанное до этого сохраняется.

    @note  Работает в потоке ядра; соединение с базой создаётся в нём.
    @note  Время в базе — миллисекунды от 1970-01-01 UTC (целое число):
           удобно сравнивать и сортировать.
    @note  Файл тайлов карты — другой файл, его SessionStore не трогает.
*/

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "core_types.h"

class SessionStore : public QObject
{
    Q_OBJECT

public:
    explicit SessionStore(QObject *parent = nullptr);
    ~SessionStore() override;

    // ---------------------------------------------------------------- база

    /**
        @brief  Открыть или создать базу; создать таблицы, если их нет.
        @return false — ошибка (текст — сигнал dbError).
    */
    bool open(const QString &path);
    void close();                               ///< закончить текущий полёт и закрыть базу
    bool isOpen() const { return m_db.isOpen(); }
    QString path() const { return m_path; }

    // ---------------------------------------------------------------- полёт

    void startSession(const QString &name);     ///< «Новый полёт»: прежний, если был, заканчивается
    void finishSession();                       ///< «Завершить полёт»
    bool hasSession() const { return m_session.id != 0; }
    SessionInfo currentSession() const { return m_session; }

    /**
        @brief Полёт начат? Если нет — начать автоматически (имя — дата и время UTC).
        @note  Вызывается перед каждой записью: данные не пропадут,
               даже если оператор забыл нажать «Новый полёт».
    */
    bool ensureSession();

    // ---------------------------------------------------------------- запись (в момент события)
    // Без открытой базы — ничего не делают. Возвращают номер записи (0 — не записано).

    void   saveRaw(bool sent, const QByteArray &bytes, const QDateTime &time);   ///< сырой журнал обмена (Ф1)
    void   savePacket(const Packet &p);                 ///< принятый пакет байт в байт — для сверки с SD (Ф13)
    qint64 saveRoutePoint(const RoutePoint &r);
    qint64 saveMeasurement(const Measurement &m);
    qint64 saveEvent(const CoreEvent &e);
    qint64 saveAlarm(const Alarm &a);
    void   setAlarmAcknowledged(qint64 alarmId);
    qint64 saveCommand(const CommandOutcome &o);
    qint64 saveArea(const CommandRequest &id04, const QDateTime &time);    ///< область и параметры принятой ID04
    qint64 saveSpectrum(const Spectrum &s);             ///< итог и порции — одной транзакцией
    void   saveSettings(const CoreSettings &s);         ///< настройки, с которыми идёт полёт
    void   setNoSd(bool noSd);                          ///< полёт без SD-карты

    void setThresholds(float warn, float alarm);        ///< пороги — для типов точек в поиске по архиву

    // ---------------------------------------------------------------- чтение (ответ — сигналом)

    void listSessions();                                ///< → sessionsListed
    void loadSession(qint64 sessionId);                 ///< → sessionData
    void query(const SessionFilter &filter);            ///< → archiveResult
    void timeSeries(qint64 sessionId);                  ///< → timeSeriesReady (Ф11)

    QVector<SessionInfo> sessions();                    ///< список полётов (без сигнала)
    SessionData sessionDataFor(qint64 sessionId);       ///< все данные полёта (без сигнала)

signals:
    void sessionStarted(const SessionInfo &info);
    void sessionFinished(const SessionInfo &info);
    void sessionsListed(const QVector<SessionInfo> &list);
    void sessionData(const SessionData &data);
    void archiveResult(const ArchiveResult &result);
    void timeSeriesReady(qint64 sessionId, const QVector<Measurement> &series);
    void dbError(const QString &text);

private:
    bool createTables();
    bool exec(const QString &sql);                      ///< выполнить запрос без параметров; ошибка → dbError
    QVector<Measurement> measurementsOf(qint64 sessionId);
    int  measurementCount(qint64 sessionId);

    QSqlDatabase m_db;
    QString      m_connection;          ///< имя соединения Qt Sql — своё у каждого SessionStore
    QString      m_path;
    SessionInfo  m_session;             ///< текущий полёт; id = 0 — полёта нет

    float        m_warn = 0.3f;         ///< порог предупреждения — для поиска по архиву
    float        m_alarm = 1.0f;        ///< порог тревоги
};

#endif // SESSION_STORE_H
