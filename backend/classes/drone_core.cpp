#include "drone_core.h"
#include "core_stub.h"
#include "core_engine.h"

#include <QSerialPortInfo>

/**
    @brief Конструктор: регистрация типов, создание заглушки и настоящего ядра
           в потоке ядра, соединение сигналов, запуск потока.
*/
DroneCore::DroneCore(QObject *parent)
    : QObject(parent)
{
    // До первого connect и до запуска потока: иначе сигналы со структурами
    // из core_types.h не пройдут между потоками
    registerCoreTypes();

    // Имя потока видно в отладчике
    m_thread.setObjectName(QStringLiteral("DroneCoreThread"));

    // ---- Заглушка ядра (шаг 3): тестовые данные, порт «STUB» ----

    // Создаётся без родителя: объект с родителем нельзя перенести в другой поток
    m_stub = new CoreStub;
    m_stub->moveToThread(&m_thread);

    // Удаление — в потоке ядра, когда поток закончит работу (см. деструктор)
    connect(&m_thread, &QThread::finished, m_stub, &QObject::deleteLater);

    // Сигналы заглушки → одноимённые сигналы DroneCore («сигнал → сигнал»).
    // Заглушка в потоке ядра, DroneCore в потоке интерфейса — Qt сам ставит
    // вызов в очередь потока интерфейса, слоты интерфейса выполняются в нём.
    connect(m_stub, &CoreStub::linkStatusChanged,      this, &DroneCore::linkStatusChanged);
    connect(m_stub, &CoreStub::loggerStatusChanged,    this, &DroneCore::loggerStatusChanged);
    connect(m_stub, &CoreStub::allowedCommandsChanged, this, &DroneCore::allowedCommandsChanged);
    connect(m_stub, &CoreStub::routePoint,             this, &DroneCore::routePoint);
    connect(m_stub, &CoreStub::measurement,            this, &DroneCore::measurement);
    connect(m_stub, &CoreStub::spectrumUpdated,        this, &DroneCore::spectrumUpdated);
    connect(m_stub, &CoreStub::spectrumFinished,       this, &DroneCore::spectrumFinished);
    connect(m_stub, &CoreStub::gridUpdated,            this, &DroneCore::gridUpdated);
    connect(m_stub, &CoreStub::alarm,                  this, &DroneCore::alarm);
    connect(m_stub, &CoreStub::event,                  this, &DroneCore::event);
    connect(m_stub, &CoreStub::coreError,              this, &DroneCore::coreError);

    connect(m_stub, &CoreStub::commandFinished,        this, &DroneCore::commandFinished);
    connect(m_stub, &CoreStub::areaChecked,            this, &DroneCore::areaChecked);
    connect(m_stub, &CoreStub::sessionStarted,         this, &DroneCore::sessionStarted);
    connect(m_stub, &CoreStub::sessionFinished,        this, &DroneCore::sessionFinished);
    connect(m_stub, &CoreStub::sessionsListed,         this, &DroneCore::sessionsListed);
    connect(m_stub, &CoreStub::sessionData,            this, &DroneCore::sessionData);
    connect(m_stub, &CoreStub::archiveResult,          this, &DroneCore::archiveResult);
    connect(m_stub, &CoreStub::timeSeriesReady,        this, &DroneCore::timeSeriesReady);
    connect(m_stub, &CoreStub::sdProgress,             this, &DroneCore::sdProgress);
    connect(m_stub, &CoreStub::sdImported,             this, &DroneCore::sdImported);
    connect(m_stub, &CoreStub::exportFinished,         this, &DroneCore::exportFinished);
    connect(m_stub, &CoreStub::alarmAcknowledged,      this, &DroneCore::alarmAcknowledged);

    // ---- Настоящее ядро (шаг 9): COM-порт и классы ядра ----
    m_engine = new CoreEngine;
    m_engine->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_engine, &QObject::deleteLater);

    // Сигналы настоящего ядра → те же сигналы DroneCore. Работает всегда кто-то один:
    // заглушка или ядро (см. openPort), поэтому сигналы не смешиваются.
    connect(m_engine, &CoreEngine::linkStatusChanged,      this, &DroneCore::linkStatusChanged);
    connect(m_engine, &CoreEngine::loggerStatusChanged,    this, &DroneCore::loggerStatusChanged);
    connect(m_engine, &CoreEngine::allowedCommandsChanged, this, &DroneCore::allowedCommandsChanged);
    connect(m_engine, &CoreEngine::routePoint,             this, &DroneCore::routePoint);    
    connect(m_engine, &CoreEngine::measurement,            this, &DroneCore::measurement);
    connect(m_engine, &CoreEngine::spectrumUpdated,        this, &DroneCore::spectrumUpdated);
    connect(m_engine, &CoreEngine::spectrumFinished,       this, &DroneCore::spectrumFinished);
    connect(m_engine, &CoreEngine::alarm,                  this, &DroneCore::alarm);
    connect(m_engine, &CoreEngine::alarmAcknowledged,      this, &DroneCore::alarmAcknowledged);
    connect(m_engine, &CoreEngine::event,                  this, &DroneCore::event);
    connect(m_engine, &CoreEngine::coreError,              this, &DroneCore::coreError);
    connect(m_engine, &CoreEngine::commandFinished,        this, &DroneCore::commandFinished);
    connect(m_engine, &CoreEngine::areaChecked,            this, &DroneCore::areaChecked);
    connect(m_engine, &CoreEngine::sessionStarted,         this, &DroneCore::sessionStarted);
    connect(m_engine, &CoreEngine::sessionFinished,        this, &DroneCore::sessionFinished);
    connect(m_engine, &CoreEngine::sessionsListed,         this, &DroneCore::sessionsListed);
    connect(m_engine, &CoreEngine::sessionData,            this, &DroneCore::sessionData);
    connect(m_engine, &CoreEngine::archiveResult,          this, &DroneCore::archiveResult);
    connect(m_engine, &CoreEngine::timeSeriesReady,        this, &DroneCore::timeSeriesReady);
    connect(m_engine, &CoreEngine::sdProgress,             this, &DroneCore::sdProgress);
    connect(m_engine, &CoreEngine::sdImported,             this, &DroneCore::sdImported);

    m_thread.start();
}

/**
    @brief Деструктор: остановка потока ядра.
    @note  quit() завершает цикл событий потока, wait() ждёт, пока поток
           действительно закончится. Без wait() программа может закрыться,
           пока поток ещё работает (ошибка «QThread destroyed while running»).
           Заглушка и ядро удаляются в потоке ядра по сигналу finished (deleteLater).
*/
DroneCore::~DroneCore()
{
    m_thread.quit();
    m_thread.wait();
}

// ============================================================================
//  Передача вызовов в поток ядра.
//  QMetaObject::invokeMethod(объект, лямбда) выполняет лямбду в потоке объекта:
//  вызов ставится в очередь потока ядра, метод DroneCore сразу возвращается.
//  Параметры захватываются копией ([=]) — ссылки на данные интерфейса
//  в другой поток передавать нельзя.
// ============================================================================

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

/**
    @brief Открыть порт. «STUB» — заглушка (тестовые данные), иначе — настоящее ядро и COM-порт.
    @note  Если до этого работал другой, он сначала закрывается.
*/
void DroneCore::openPort(const QString &name)
{
    const bool useStub = (name == QStringLiteral("STUB"));

    if (m_portOpen && useStub != m_useStub)
        closePort();                        // закрыть того, кто работал до этого
    m_useStub = useStub;
    m_portOpen = true;

    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, name] { stub->openPort(name); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, name] { engine->openPort(name); });
}

void DroneCore::closePort()
{
    m_portOpen = false;
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub] { stub->closePort(); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine] { engine->closePort(); });
}

// ============================================================================
//  Команды (Ф2) и область (Ф3)
// ============================================================================

quint32 DroneCore::sendCommand(CommandRequest request)
{
    // Номер присваивается здесь, в потоке интерфейса: интерфейс сразу знает,
    // какой ответ commandFinished относится к этой команде
    request.seq = m_nextSeq++;

    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, request] { stub->sendCommand(request); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, request] { engine->sendCommand(request); });
    return request.seq;
}

void DroneCore::checkArea(const QVector<GeoPoint> &vertices, bool isRoute)
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, vertices, isRoute] { stub->checkArea(vertices, isRoute); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, vertices, isRoute] { engine->checkArea(vertices, isRoute); });
}

// ============================================================================
//  Сессии (Ф12) и архив
//  Работает заглушка — её тестовые полёты; работает ядро — база полётов.
// ============================================================================

void DroneCore::startSession(const QString &name)
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, name] { stub->startSession(name); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, name] { engine->startSession(name); });
}

void DroneCore::finishSession()
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub] { stub->finishSession(); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine] { engine->finishSession(); });
}

void DroneCore::listSessions()
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub] { stub->listSessions(); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine] { engine->listSessions(); });
}

void DroneCore::openSession(qint64 sessionId)
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, sessionId] { stub->openSession(sessionId); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, sessionId] { engine->openSession(sessionId); });
}

void DroneCore::queryArchive(const SessionFilter &filter)
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, filter] { stub->queryArchive(filter); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, filter] { engine->queryArchive(filter); });
}

void DroneCore::requestTimeSeries(qint64 sessionId)
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, sessionId] { stub->requestTimeSeries(sessionId); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, sessionId] { engine->requestTimeSeries(sessionId); });
}

// ============================================================================
//  SD (Ф13) и экспорт (Ф14)
// ============================================================================

void DroneCore::importSd(const QStringList &files)
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, files] { stub->importSd(files); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, files] { engine->importSd(files); });
}

void DroneCore::exportCsv(qint64 sessionId, const QString &folder, quint32 parts)
{
    QMetaObject::invokeMethod(m_stub, [stub = m_stub, sessionId, folder, parts] {
        stub->exportCsv(sessionId, folder, parts);
    });
}

// ============================================================================
//  Настройки и тревоги
// ============================================================================

void DroneCore::applySettings(const CoreSettings &settings)
{
    m_settings = settings;
    // Оба: заглушке нужны пороги, ядру — порог связи и размер области
    QMetaObject::invokeMethod(m_stub,   [stub = m_stub, settings]     { stub->applySettings(settings); });
    QMetaObject::invokeMethod(m_engine, [engine = m_engine, settings] { engine->applySettings(settings); });
}

void DroneCore::acknowledgeAlarm(qint64 alarmId)
{
    if (m_useStub)
        QMetaObject::invokeMethod(m_stub, [stub = m_stub, alarmId] { stub->acknowledgeAlarm(alarmId); });
    else
        QMetaObject::invokeMethod(m_engine, [engine = m_engine, alarmId] { engine->acknowledgeAlarm(alarmId); });
}
