#include "session_store.h"
#include "recon_area.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QUuid>

/**
    @brief Преобразования для базы: время, JSON, каналы спектра.
*/
namespace {
    constexpr int SchemaVersion = 1;        ///< версия схемы — PRAGMA user_version

    /// Время → миллисекунды UTC; пустое время → NULL
    QVariant ms(const QDateTime &t)
    {
        return t.isValid() ? QVariant(t.toMSecsSinceEpoch()) : QVariant();
    }

    /// Миллисекунды UTC → время; NULL → пустое
    QDateTime fromMs(const QVariant &v)
    {
        return v.isNull() ? QDateTime() : QDateTime::fromMSecsSinceEpoch(v.toLongLong(), QTimeZone::utc());
    }

    QString toJson(const QJsonObject &o) { return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)); }
    QString toJson(const QJsonArray &a)  { return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)); }
    QJsonObject jsonObject(const QString &s) { return QJsonDocument::fromJson(s.toUtf8()).object(); }
    QJsonArray  jsonArray(const QString &s)  { return QJsonDocument::fromJson(s.toUtf8()).array(); }

    /// Вершины → [[lat, lon], …]
    QJsonArray verticesToJson(const QVector<GeoPoint> &v)
    {
        QJsonArray a;
        for (const GeoPoint &p : v)
            a.append(QJsonArray{p.lat, p.lon});
        return a;
    }

    QVector<GeoPoint> verticesFromJson(const QJsonArray &a)
    {
        QVector<GeoPoint> v;
        for (const QJsonValue &p : a)
            v.append({ p.toArray().at(0).toDouble(), p.toArray().at(1).toDouble() });
        return v;
    }

    /// Каналы спектра → байты (uint16 младшим байтом вперёд) и обратно
    QByteArray channelsToBlob(const QVector<quint16> &ch)
    {
        QByteArray b;
        b.reserve(ch.size() * 2);
        for (quint16 v : ch) {
            b.append(char(v & 0xFF));
            b.append(char(v >> 8));
        }
        return b;
    }

    QVector<quint16> channelsFromBlob(const QByteArray &b)
    {
        QVector<quint16> ch(b.size() / 2);
        for (int i = 0; i < ch.size(); ++i)
            ch[i] = quint16(quint8(b[2 * i]) | (quint8(b[2 * i + 1]) << 8));
        return ch;
    }

    /// Команда → JSON (для таблицы commands)
    QJsonObject requestToJson(const CommandRequest &r)
    {
        return QJsonObject{
            {"seq", qint64(r.seq)}, {"id", r.id}, {"value", r.value},
            {"vertices", verticesToJson(r.vertices)}, {"reconMode", int(r.reconMode)},
            {"intervalS", r.intervalS}, {"distanceM", r.distanceM},
            {"dupFactor", r.dupFactor}, {"spectrumS", r.spectrumS}
        };
    }

    CommandRequest requestFromJson(const QJsonObject &o)
    {
        CommandRequest r;
        r.seq       = quint32(o.value("seq").toInteger());
        r.id        = quint8(o.value("id").toInt());
        r.value     = o.value("value").toInt();
        r.vertices  = verticesFromJson(o.value("vertices").toArray());
        r.reconMode = ReconMode(o.value("reconMode").toInt());
        r.intervalS = quint16(o.value("intervalS").toInt());
        r.distanceM = quint16(o.value("distanceM").toInt());
        r.dupFactor = quint8(o.value("dupFactor").toInt());
        r.spectrumS = quint16(o.value("spectrumS").toInt());
        return r;
    }

    /// Настройки → JSON (для таблицы session_settings)
    QJsonObject settingsToJson(const CoreSettings &s)
    {
        QJsonArray coeffs;
        for (double c : s.correctionCoeffs)
            coeffs.append(c);
        return QJsonObject{
            {"linkTimeoutS", s.linkTimeoutS}, {"maxAreaKm", s.maxAreaKm},
            {"warnLevel", double(s.warnLevel)}, {"alarmLevel", double(s.alarmLevel)},
            {"correctionOn", s.correctionOn}, {"correctionCoeffs", coeffs},
            {"correctionMinM", s.correctionMinM}, {"correctionMaxM", s.correctionMaxM},
            {"gridCellM", s.gridCellM}, {"gridRadiusM", s.gridRadiusM}, {"gridPower", s.gridPower}
        };
    }
}

// ============================================================================
//  Создание и база
// ============================================================================

SessionStore::SessionStore(QObject *parent)
    : QObject(parent)
    , m_connection(QStringLiteral("session_store_") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

SessionStore::~SessionStore()
{
    close();
}

bool SessionStore::open(const QString &path)
{
    if (isOpen())
        close();

    // Соединение создаётся в потоке, где работает SessionStore, — в потоке ядра
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        emit dbError(QStringLiteral("%1: %2").arg(path, m_db.lastError().text()));
        return false;
    }
    m_path = path;

    // WAL: запись сразу в журнал на диске, чтение не мешает записи.
    // synchronous=NORMAL: при сбое питания теряется самое большее последняя транзакция.
    if (!exec(QStringLiteral("PRAGMA journal_mode=WAL")) ||
        !exec(QStringLiteral("PRAGMA synchronous=NORMAL")) ||
        !exec(QStringLiteral("PRAGMA foreign_keys=ON")) ||
        !createTables()) {
        close();
        return false;
    }
    return true;
}

void SessionStore::close()
{
    if (!m_db.isValid())
        return;
    if (m_db.isOpen() && hasSession())
        finishSession();
    m_db.close();
    m_db = QSqlDatabase();                          // отпустить соединение до removeDatabase
    QSqlDatabase::removeDatabase(m_connection);
}

bool SessionStore::exec(const QString &sql)
{
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        emit dbError(QStringLiteral("%1: %2").arg(sql.left(60), q.lastError().text()));
        return false;
    }
    return true;
}

/**
    @brief Таблицы и индексы. IF NOT EXISTS — при повторном открытии ничего не меняется.
*/
bool SessionStore::createTables()
{
    const char *sql[] = {
        "CREATE TABLE IF NOT EXISTS sessions ("
        " id INTEGER PRIMARY KEY, name TEXT, start_ms INTEGER, end_ms INTEGER, no_sd INTEGER DEFAULT 0)",

        "CREATE TABLE IF NOT EXISTS raw_log ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, sent INTEGER, bytes BLOB)",

        "CREATE TABLE IF NOT EXISTS packets ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, msg_id INTEGER, payload BLOB, source INTEGER)",

        "CREATE TABLE IF NOT EXISTS route ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, rx_ms INTEGER, lat REAL, lon REAL, in_area INTEGER,"
        " msg_id INTEGER, mode INTEGER, source INTEGER)",

        "CREATE TABLE IF NOT EXISTS measurements ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, rx_ms INTEGER, paed REAL, paed1m REAL, has_paed1m INTEGER,"
        " temp_code INTEGER, bdbg_stat INTEGER, lidar_cm INTEGER, lidar_strength INTEGER,"
        " lat REAL, lon REAL, batt_v REAL, flags INTEGER, source INTEGER)",

        "CREATE TABLE IF NOT EXISTS commands ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " cmd_id INTEGER, request TEXT, result INTEGER, reason INTEGER,"
        " sent_ms INTEGER, finish_ms INTEGER, attempts INTEGER)",

        "CREATE TABLE IF NOT EXISTS areas ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, vertices TEXT, recon_mode INTEGER, interval_s INTEGER,"
        " distance_m INTEGER, dup_factor INTEGER, spectrum_s INTEGER)",

        "CREATE TABLE IF NOT EXISTS events ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, code INTEGER, params TEXT, debug INTEGER, source INTEGER)",

        "CREATE TABLE IF NOT EXISTS alarms ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, type INTEGER, level INTEGER, has_pos INTEGER, lat REAL, lon REAL,"
        " value REAL, msg_id INTEGER, acknowledged INTEGER DEFAULT 0)",

        "CREATE TABLE IF NOT EXISTS spectra ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " number INTEGER, start_ms INTEGER, lat REAL, lon REAL, lidar_cm INTEGER,"
        " set_duration_s INTEGER, portions INTEGER, last_paed REAL, last_stat INTEGER,"
        " finished INTEGER, stop_reason INTEGER, source INTEGER, channels BLOB)",

        "CREATE TABLE IF NOT EXISTS spectrum_portions ("
        " id INTEGER PRIMARY KEY, spectrum_id INTEGER REFERENCES spectra(id),"
        " rx_ms INTEGER, acc_time INTEGER, paed REAL, stat INTEGER, channels BLOB)",

        "CREATE TABLE IF NOT EXISTS session_settings ("
        " id INTEGER PRIMARY KEY, session_id INTEGER REFERENCES sessions(id),"
        " time_ms INTEGER, settings TEXT)",

        // Индексы: выборка по полёту и времени, поиск по координатам
        "CREATE INDEX IF NOT EXISTS ix_route_s   ON route(session_id, time_ms)",
        "CREATE INDEX IF NOT EXISTS ix_meas_s    ON measurements(session_id, time_ms)",
        "CREATE INDEX IF NOT EXISTS ix_meas_pos  ON measurements(lat, lon)",
        "CREATE INDEX IF NOT EXISTS ix_events_s  ON events(session_id, time_ms)",
        "CREATE INDEX IF NOT EXISTS ix_packets_s ON packets(session_id, time_ms)",
        "CREATE INDEX IF NOT EXISTS ix_raw_s     ON raw_log(session_id, time_ms)",
        "CREATE INDEX IF NOT EXISTS ix_portion_s ON spectrum_portions(spectrum_id)"
    };
    for (const char *s : sql)
        if (!exec(QString::fromLatin1(s)))
            return false;
    return exec(QStringLiteral("PRAGMA user_version=%1").arg(SchemaVersion));
}

// ============================================================================
//  Полёт
// ============================================================================

void SessionStore::startSession(const QString &name)
{
    if (!isOpen())
        return;
    if (hasSession())
        finishSession();

    SessionInfo s;
    s.name = name;
    s.startTime = QDateTime::currentDateTimeUtc();

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO sessions (name, start_ms) VALUES (?, ?)"));
    q.addBindValue(s.name);
    q.addBindValue(ms(s.startTime));
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return;
    }
    s.id = q.lastInsertId().toLongLong();
    m_session = s;
    emit sessionStarted(m_session);
}

void SessionStore::finishSession()
{
    if (!isOpen() || !hasSession())
        return;

    m_session.endTime = QDateTime::currentDateTimeUtc();
    m_session.measurementCount = measurementCount(m_session.id);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sessions SET end_ms = ? WHERE id = ?"));
    q.addBindValue(ms(m_session.endTime));
    q.addBindValue(m_session.id);
    if (!q.exec())
        emit dbError(q.lastError().text());

    const SessionInfo finished = m_session;
    m_session = SessionInfo();
    emit sessionFinished(finished);
}

bool SessionStore::ensureSession()
{
    if (!isOpen())
        return false;
    if (!hasSession())
        startSession(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd hh:mm 'UTC'")));
    return hasSession();
}

// ============================================================================
//  Запись
// ============================================================================

void SessionStore::saveRaw(bool sent, const QByteArray &bytes, const QDateTime &time)
{
    if (!ensureSession())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO raw_log (session_id, time_ms, sent, bytes) VALUES (?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(time));
    q.addBindValue(sent ? 1 : 0);
    q.addBindValue(bytes);
    if (!q.exec())
        emit dbError(q.lastError().text());
}

void SessionStore::savePacket(const Packet &p)
{
    if (!ensureSession())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO packets (session_id, time_ms, msg_id, payload, source) VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(p.rxTime));
    q.addBindValue(p.id);
    q.addBindValue(p.payload);
    q.addBindValue(int(DataSource::Air));
    if (!q.exec())
        emit dbError(q.lastError().text());
}

qint64 SessionStore::saveRoutePoint(const RoutePoint &r)
{
    if (!ensureSession())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO route (session_id, time_ms, rx_ms, lat, lon, in_area, msg_id, mode, source)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(r.time));
    q.addBindValue(ms(r.rxTime));
    q.addBindValue(r.lat);
    q.addBindValue(r.lon);
    q.addBindValue(r.inArea ? 1 : 0);
    q.addBindValue(r.msgId);
    q.addBindValue(int(r.mode));
    q.addBindValue(int(r.source));
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

qint64 SessionStore::saveMeasurement(const Measurement &m)
{
    if (!ensureSession())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO measurements (session_id, time_ms, rx_ms, paed, paed1m, has_paed1m,"
                             " temp_code, bdbg_stat, lidar_cm, lidar_strength, lat, lon, batt_v, flags, source)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(m.time));
    q.addBindValue(ms(m.rxTime));
    q.addBindValue(double(m.paed));
    q.addBindValue(double(m.paed1m));
    q.addBindValue(m.hasPaed1m ? 1 : 0);
    q.addBindValue(m.tempCode);
    q.addBindValue(m.bdbgStat);
    q.addBindValue(m.lidarCm);
    q.addBindValue(m.lidarStrength);
    q.addBindValue(m.lat);
    q.addBindValue(m.lon);
    q.addBindValue(double(m.battV));
    q.addBindValue(m.flags);
    q.addBindValue(int(m.source));
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

qint64 SessionStore::saveEvent(const CoreEvent &e)
{
    if (!ensureSession())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO events (session_id, time_ms, code, params, debug, source) VALUES (?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(e.time));
    q.addBindValue(int(e.code));
    q.addBindValue(toJson(QJsonObject::fromVariantMap(e.params)));
    q.addBindValue(e.debug ? 1 : 0);
    q.addBindValue(int(e.source));
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

qint64 SessionStore::saveAlarm(const Alarm &a)
{
    if (!ensureSession())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO alarms (session_id, time_ms, type, level, has_pos, lat, lon, value, msg_id, acknowledged)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(a.time));
    q.addBindValue(int(a.type));
    q.addBindValue(int(a.level));
    q.addBindValue(a.hasPos ? 1 : 0);
    q.addBindValue(a.lat);
    q.addBindValue(a.lon);
    q.addBindValue(double(a.value));
    q.addBindValue(a.msgId);
    q.addBindValue(a.acknowledged ? 1 : 0);
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

void SessionStore::setAlarmAcknowledged(qint64 alarmId)
{
    if (!isOpen())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE alarms SET acknowledged = 1 WHERE id = ?"));
    q.addBindValue(alarmId);
    if (!q.exec())
        emit dbError(q.lastError().text());
}

qint64 SessionStore::saveCommand(const CommandOutcome &o)
{
    if (!ensureSession())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO commands (session_id, cmd_id, request, result, reason, sent_ms, finish_ms, attempts)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(o.request.id);
    q.addBindValue(toJson(requestToJson(o.request)));
    q.addBindValue(int(o.result));
    q.addBindValue(int(o.reason));
    q.addBindValue(ms(o.sentTime));
    q.addBindValue(ms(o.finishTime));
    q.addBindValue(o.attempts);
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

qint64 SessionStore::saveArea(const CommandRequest &r, const QDateTime &time)
{
    if (!ensureSession())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO areas (session_id, time_ms, vertices, recon_mode, interval_s, distance_m, dup_factor, spectrum_s)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(time));
    q.addBindValue(toJson(verticesToJson(r.vertices)));
    q.addBindValue(int(r.reconMode));
    q.addBindValue(r.intervalS);
    q.addBindValue(r.distanceM);
    q.addBindValue(r.dupFactor);
    q.addBindValue(r.spectrumS);
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

/**
    @brief Спектр: итог и все порции — одной транзакцией: либо всё, либо ничего.
*/
qint64 SessionStore::saveSpectrum(const Spectrum &s)
{
    if (!ensureSession())
        return 0;
    m_db.transaction();

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO spectra (session_id, number, start_ms, lat, lon, lidar_cm, set_duration_s,"
                             " portions, last_paed, last_stat, finished, stop_reason, source, channels)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(s.info.number);
    q.addBindValue(ms(s.info.startTime));
    q.addBindValue(s.info.lat);
    q.addBindValue(s.info.lon);
    q.addBindValue(s.info.lidarCm);
    q.addBindValue(s.info.setDurationS);
    q.addBindValue(s.info.portions);
    q.addBindValue(double(s.info.lastPaed));
    q.addBindValue(s.info.lastStat);
    q.addBindValue(s.info.finished ? 1 : 0);
    q.addBindValue(int(s.info.stopReason));
    q.addBindValue(int(s.info.source));
    q.addBindValue(channelsToBlob(s.channels));
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        m_db.rollback();
        return 0;
    }
    const qint64 id = q.lastInsertId().toLongLong();

    QSqlQuery p(m_db);
    p.prepare(QStringLiteral("INSERT INTO spectrum_portions (spectrum_id, rx_ms, acc_time, paed, stat, channels)"
                             " VALUES (?, ?, ?, ?, ?, ?)"));
    for (const SpectrumPortion &portion : s.portionList) {
        p.addBindValue(id);
        p.addBindValue(ms(portion.rxTime));
        p.addBindValue(portion.accTime);
        p.addBindValue(double(portion.paed));
        p.addBindValue(portion.stat);
        p.addBindValue(channelsToBlob(portion.channels));
        if (!p.exec()) {
            emit dbError(p.lastError().text());
            m_db.rollback();
            return 0;
        }
    }
    m_db.commit();
    return id;
}

void SessionStore::saveSettings(const CoreSettings &s)
{
    if (!isOpen() || !hasSession())         // настройки без полёта не пишем: полёт ради них не начинается
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO session_settings (session_id, time_ms, settings) VALUES (?, ?, ?)"));
    q.addBindValue(m_session.id);
    q.addBindValue(ms(QDateTime::currentDateTimeUtc()));
    q.addBindValue(toJson(settingsToJson(s)));
    if (!q.exec())
        emit dbError(q.lastError().text());
}

void SessionStore::setNoSd(bool noSd)
{
    if (!isOpen() || !hasSession() || m_session.noSd == noSd)
        return;
    m_session.noSd = noSd;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sessions SET no_sd = ? WHERE id = ?"));
    q.addBindValue(noSd ? 1 : 0);
    q.addBindValue(m_session.id);
    if (!q.exec())
        emit dbError(q.lastError().text());
}

void SessionStore::setThresholds(float warn, float alarm)
{
    m_warn = warn;
    m_alarm = alarm;
}

// ============================================================================
//  Чтение
// ============================================================================

int SessionStore::measurementCount(qint64 sessionId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM measurements WHERE session_id = ?"));
    q.addBindValue(sessionId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

QVector<SessionInfo> SessionStore::sessions()
{
    QVector<SessionInfo> list;
    if (!isOpen())
        return list;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT s.id, s.name, s.start_ms, s.end_ms, s.no_sd,"
                               " (SELECT COUNT(*) FROM measurements m WHERE m.session_id = s.id)"
                               " FROM sessions s ORDER BY s.start_ms DESC"))) {
        emit dbError(q.lastError().text());
        return list;
    }
    while (q.next()) {
        SessionInfo s;
        s.id               = q.value(0).toLongLong();
        s.name             = q.value(1).toString();
        s.startTime        = fromMs(q.value(2));
        s.endTime          = fromMs(q.value(3));
        s.noSd             = q.value(4).toInt() != 0;
        s.measurementCount = q.value(5).toInt();
        list.append(s);
    }
    return list;
}

void SessionStore::listSessions()
{
    emit sessionsListed(sessions());
}

QVector<Measurement> SessionStore::measurementsOf(qint64 sessionId)
{
    QVector<Measurement> list;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id, time_ms, rx_ms, paed, paed1m, has_paed1m, temp_code, bdbg_stat,"
                             " lidar_cm, lidar_strength, lat, lon, batt_v, flags, source"
                             " FROM measurements WHERE session_id = ? ORDER BY time_ms, id"));
    q.addBindValue(sessionId);
    if (!q.exec()) {
        emit dbError(q.lastError().text());
        return list;
    }
    while (q.next()) {
        Measurement m;
        m.id            = q.value(0).toLongLong();
        m.time          = fromMs(q.value(1));
        m.rxTime        = fromMs(q.value(2));
        m.paed          = q.value(3).toFloat();
        m.paed1m        = q.value(4).toFloat();
        m.hasPaed1m     = q.value(5).toInt() != 0;
        m.tempCode      = quint16(q.value(6).toUInt());
        m.bdbgStat      = quint8(q.value(7).toUInt());
        m.lidarCm       = quint16(q.value(8).toUInt());
        m.lidarStrength = quint16(q.value(9).toUInt());
        m.lat           = q.value(10).toDouble();
        m.lon           = q.value(11).toDouble();
        m.battV         = q.value(12).toFloat();
        m.flags         = q.value(13).toUInt();
        m.source        = DataSource(q.value(14).toInt());
        list.append(m);
    }
    return list;
}

SessionData SessionStore::sessionDataFor(qint64 sessionId)
{
    SessionData d;
    if (!isOpen())
        return d;

    for (const SessionInfo &s : sessions())
        if (s.id == sessionId)
            d.info = s;
    d.info.readOnly = true;

    QSqlQuery q(m_db);

    // Маршрут
    q.prepare(QStringLiteral("SELECT time_ms, rx_ms, lat, lon, in_area, msg_id, mode, source"
                             " FROM route WHERE session_id = ? ORDER BY time_ms, id"));
    q.addBindValue(sessionId);
    if (q.exec()) {
        while (q.next()) {
            RoutePoint r;
            r.time   = fromMs(q.value(0));
            r.rxTime = fromMs(q.value(1));
            r.lat    = q.value(2).toDouble();
            r.lon    = q.value(3).toDouble();
            r.inArea = q.value(4).toInt() != 0;
            r.msgId  = quint8(q.value(5).toUInt());
            r.mode   = LoggerMode(q.value(6).toInt());
            r.source = DataSource(q.value(7).toInt());
            d.route.append(r);
        }
    }

    // Измерения
    d.measurements = measurementsOf(sessionId);

    // События
    q.prepare(QStringLiteral("SELECT id, time_ms, code, params, debug, source FROM events"
                             " WHERE session_id = ? ORDER BY time_ms, id"));
    q.addBindValue(sessionId);
    if (q.exec()) {
        while (q.next()) {
            CoreEvent e;
            e.id     = q.value(0).toLongLong();
            e.time   = fromMs(q.value(1));
            e.code   = EventCode(q.value(2).toInt());
            e.params = jsonObject(q.value(3).toString()).toVariantMap();
            e.debug  = q.value(4).toInt() != 0;
            e.source = DataSource(q.value(5).toInt());
            d.events.append(e);
        }
    }

    // Тревоги
    q.prepare(QStringLiteral("SELECT id, time_ms, type, level, has_pos, lat, lon, value, msg_id, acknowledged"
                             " FROM alarms WHERE session_id = ? ORDER BY time_ms, id"));
    q.addBindValue(sessionId);
    if (q.exec()) {
        while (q.next()) {
            Alarm a;
            a.id           = q.value(0).toLongLong();
            a.time         = fromMs(q.value(1));
            a.type         = AlarmType(q.value(2).toInt());
            a.level        = AlarmLevel(q.value(3).toInt());
            a.hasPos       = q.value(4).toInt() != 0;
            a.lat          = q.value(5).toDouble();
            a.lon          = q.value(6).toDouble();
            a.value        = q.value(7).toFloat();
            a.msgId        = quint8(q.value(8).toUInt());
            a.acknowledged = q.value(9).toInt() != 0;
            d.alarms.append(a);
        }
    }

    // Команды
    q.prepare(QStringLiteral("SELECT request, result, reason, sent_ms, finish_ms, attempts FROM commands"
                             " WHERE session_id = ? ORDER BY id"));
    q.addBindValue(sessionId);
    if (q.exec()) {
        while (q.next()) {
            CommandOutcome o;
            o.request    = requestFromJson(jsonObject(q.value(0).toString()));
            o.result     = CommandResult(q.value(1).toInt());
            o.reason     = RejectReason(q.value(2).toInt());
            o.sentTime   = fromMs(q.value(3));
            o.finishTime = fromMs(q.value(4));
            o.attempts   = q.value(5).toInt();
            d.commands.append(o);
        }
    }

    // Области
    q.prepare(QStringLiteral("SELECT vertices FROM areas WHERE session_id = ? ORDER BY time_ms, id"));
    q.addBindValue(sessionId);
    if (q.exec())
        while (q.next())
            d.areas.append(verticesFromJson(jsonArray(q.value(0).toString())));

    // Спектры с порциями
    q.prepare(QStringLiteral("SELECT id, number, start_ms, lat, lon, lidar_cm, set_duration_s, portions,"
                             " last_paed, last_stat, finished, stop_reason, source, channels"
                             " FROM spectra WHERE session_id = ? ORDER BY start_ms, id"));
    q.addBindValue(sessionId);
    if (q.exec()) {
        while (q.next()) {
            Spectrum s;
            s.info.id           = q.value(0).toLongLong();
            s.info.number       = quint16(q.value(1).toUInt());
            s.info.startTime    = fromMs(q.value(2));
            s.info.lat          = q.value(3).toDouble();
            s.info.lon          = q.value(4).toDouble();
            s.info.lidarCm      = quint16(q.value(5).toUInt());
            s.info.setDurationS = quint16(q.value(6).toUInt());
            s.info.portions     = q.value(7).toInt();
            s.info.lastPaed     = q.value(8).toFloat();
            s.info.lastStat     = quint8(q.value(9).toUInt());
            s.info.finished     = q.value(10).toInt() != 0;
            s.info.stopReason   = SpectrumStopReason(q.value(11).toInt());
            s.info.source       = DataSource(q.value(12).toInt());
            s.channels          = channelsFromBlob(q.value(13).toByteArray());

            QSqlQuery p(m_db);
            p.prepare(QStringLiteral("SELECT rx_ms, acc_time, paed, stat, channels FROM spectrum_portions"
                                     " WHERE spectrum_id = ? ORDER BY id"));
            p.addBindValue(s.info.id);
            if (p.exec()) {
                while (p.next()) {
                    SpectrumPortion portion;
                    portion.rxTime   = fromMs(p.value(0));
                    portion.accTime  = quint16(p.value(1).toUInt());
                    portion.paed     = p.value(2).toFloat();
                    portion.stat     = quint8(p.value(3).toUInt());
                    portion.channels = channelsFromBlob(p.value(4).toByteArray());
                    s.portionList.append(portion);
                }
            }
            d.spectra.append(s);
        }
    }
    return d;
}

void SessionStore::loadSession(qint64 sessionId)
{
    emit sessionData(sessionDataFor(sessionId));
}

void SessionStore::timeSeries(qint64 sessionId)
{
    emit timeSeriesReady(sessionId, isOpen() ? measurementsOf(sessionId) : QVector<Measurement>());
}

/**
    @brief Поиск по архиву: в SQL — полёт, время и прямоугольник области;
           точно «внутри области» — ReconArea::contains (алгоритм логгера).
*/
void SessionStore::query(const SessionFilter &filter)
{
    ArchiveResult result;
    result.filter = filter;
    if (!isOpen()) {
        emit archiveResult(result);
        return;
    }

    // Условия WHERE — общие для измерений и спектров
    QString where = QStringLiteral(" WHERE 1=1");
    QVariantList binds;
    if (filter.sessionId != -1) {
        where += QStringLiteral(" AND session_id = ?");
        binds << filter.sessionId;
    }
    const bool hasArea = filter.area.size() >= CoreConst::AreaMinVertices;
    if (hasArea) {
        const ReconArea::Bounds b = ReconArea::bounds(filter.area);
        where += QStringLiteral(" AND lat BETWEEN ? AND ? AND lon BETWEEN ? AND ?");
        binds << b.latMin << b.latMax << b.lonMin << b.lonMax;
    }

    auto inArea = [&](double lat, double lon) {
        return !hasArea || ReconArea::contains(filter.area, lat, lon);
    };

    // ---- измерения ----
    if (filter.types & (ApMeasurement | ApWarn | ApAlarm)) {
        QString sql = QStringLiteral("SELECT id, session_id, time_ms, lat, lon, paed, paed1m, has_paed1m"
                                     " FROM measurements") + where;
        QVariantList b = binds;
        if (filter.from.isValid()) { sql += QStringLiteral(" AND time_ms >= ?"); b << ms(filter.from); }
        if (filter.to.isValid())   { sql += QStringLiteral(" AND time_ms <= ?"); b << ms(filter.to); }
        sql += QStringLiteral(" ORDER BY time_ms, id");

        QSqlQuery q(m_db);
        q.prepare(sql);
        for (const QVariant &v : b)
            q.addBindValue(v);
        if (!q.exec()) {
            emit dbError(q.lastError().text());
        } else {
            while (q.next()) {
                const double lat = q.value(3).toDouble(), lon = q.value(4).toDouble();
                if (!inArea(lat, lon))
                    continue;
                const float paed = q.value(7).toInt() ? q.value(6).toFloat() : q.value(5).toFloat();   // как shownPaed()
                quint32 type = ApMeasurement;
                if (paed >= m_warn)  type |= ApWarn;
                if (paed >= m_alarm) type |= ApAlarm;
                if (!(type & filter.types))
                    continue;

                ArchivePoint p;
                p.id        = q.value(0).toLongLong();
                p.sessionId = q.value(1).toLongLong();
                p.time      = fromMs(q.value(2));
                p.lat       = lat;
                p.lon       = lon;
                p.type      = type;
                p.paed      = paed;
                result.points.append(p);
            }
        }
    }

    // ---- спектры ----
    if (filter.types & ApSpectrum) {
        QString sql = QStringLiteral("SELECT id, session_id, start_ms, lat, lon, last_paed FROM spectra") + where;
        QVariantList b = binds;
        if (filter.from.isValid()) { sql += QStringLiteral(" AND start_ms >= ?"); b << ms(filter.from); }
        if (filter.to.isValid())   { sql += QStringLiteral(" AND start_ms <= ?"); b << ms(filter.to); }

        QSqlQuery q(m_db);
        q.prepare(sql);
        for (const QVariant &v : b)
            q.addBindValue(v);
        if (q.exec()) {
            while (q.next()) {
                const double lat = q.value(3).toDouble(), lon = q.value(4).toDouble();
                if (!inArea(lat, lon))
                    continue;
                ArchivePoint p;
                p.id        = q.value(0).toLongLong();
                p.sessionId = q.value(1).toLongLong();
                p.time      = fromMs(q.value(2));
                p.lat       = lat;
                p.lon       = lon;
                p.type      = ApSpectrum;
                p.paed      = q.value(5).toFloat();
                result.points.append(p);
            }
        }
    }

    emit archiveResult(result);
}
