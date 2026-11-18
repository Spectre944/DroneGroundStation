#include "drone_core.h"

#include <QSerialPortInfo>

/**
    @brief Конструктор: регистрация типов и запуск потока ядра.
*/
DroneCore::DroneCore(QObject *parent)
    : QObject(parent)
{
    // До первого connect и до запуска потока: иначе сигналы со структурами
    // из core_types.h не пройдут между потоками
    registerCoreTypes();

    // Имя потока видно в отладчике
    m_thread.setObjectName(QStringLiteral("DroneCoreThread"));

    // Шаг 3 / шаг 9: здесь создаются классы ядра (заглушка или настоящие),
    // переносятся в поток (moveToThread) и соединяются со своими сигналами.

    m_thread.start();
}

/**
    @brief Деструктор: остановка потока ядра.
    @note  quit() завершает цикл событий потока, wait() ждёт, пока поток
           действительно закончится. Без wait() программа может закрыться,
           пока поток ещё работает (ошибка «QThread destroyed while running»).
*/
DroneCore::~DroneCore()
{
    m_thread.quit();
    m_thread.wait();
}

// ============================================================================
//  Связь (Ф1)
// ============================================================================

QStringList DroneCore::availablePorts() const
{
    QStringList names;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        names.append(info.portName());
    return names;
}

void DroneCore::openPort(const QString &name)
{
    Q_UNUSED(name)
    // Шаг 4: передача в DroneLink::open в потоке ядра
}

void DroneCore::closePort()
{
    // Шаг 4: передача в DroneLink::close
}



// ============================================================================
//  Команды (Ф2) и область (Ф3)
// ============================================================================

quint32 DroneCore::sendCommand(CommandRequest request)
{
    // Номер присваивается здесь, в потоке интерфейса: интерфейс сразу знает,
    // какой ответ commandFinished относится к этой команде
    request.seq = m_nextSeq++;

    // Шаг 6: передача в DroneCommands::send в потоке ядра
    return request.seq;
}

void DroneCore::checkArea(const QVector<GeoPoint> &vertices, bool isRoute)
{
    Q_UNUSED(vertices)
    Q_UNUSED(isRoute)
    // Шаг 7: передача в ReconArea::check
}

// ============================================================================
//  Сессии (Ф12) и архив
// ============================================================================

void DroneCore::startSession(const QString &name)
{
    Q_UNUSED(name)
    // Шаг 10: SessionStore::startSession
}

void DroneCore::finishSession()
{
    // Шаг 10: SessionStore::finishSession
}

void DroneCore::listSessions()
{
    // Шаг 10: SessionStore::listSessions
}

void DroneCore::openSession(qint64 sessionId)
{
    Q_UNUSED(sessionId)
    // Шаг 10: SessionStore::loadSession
}

void DroneCore::queryArchive(const SessionFilter &filter)
{
    Q_UNUSED(filter)
    // Шаг 10: SessionStore::query
}

void DroneCore::requestTimeSeries(qint64 sessionId)
{
    Q_UNUSED(sessionId)
    // Шаг 17: SessionStore::timeSeries
}

// ============================================================================
//  SD (Ф13) и экспорт (Ф14)
// ============================================================================

void DroneCore::importSd(const QStringList &files)
{
    Q_UNUSED(files)
    // Шаг 13: SdImporter::import
}

void DroneCore::exportCsv(qint64 sessionId, const QString &folder, quint32 parts)
{
    Q_UNUSED(sessionId)
    Q_UNUSED(folder)
    Q_UNUSED(parts)
    // Шаг 16: CsvExporter::exportSession
}

// ============================================================================
//  Настройки и тревоги
// ============================================================================

void DroneCore::applySettings(const CoreSettings &settings)
{
    m_settings = settings;
    // Шаг 9: раздать настройки классам ядра (setSettings в потоке ядра)
}

void DroneCore::acknowledgeAlarm(qint64 alarmId)
{
    Q_UNUSED(alarmId)
    // Шаг 12: AlarmManager::acknowledge
}
