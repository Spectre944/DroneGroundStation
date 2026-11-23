#include "core_stub.h"

#include <QRandomGenerator>
#include <QTimeZone>
#include <cmath>
#include <limits>
#include <utility>

/**
    @brief Постоянные тестового сценария.
    @note  Область — квадрат ~1 × 1 км под Киевом (там есть тайлы).
           Чтобы перенести сценарий в другое место — поменять AreaLat0 / AreaLon0.
*/
namespace {
    constexpr double AreaLat0   = 50.445;   ///< юго-западный угол области, широта
    constexpr double AreaLon0   = 30.515;   ///< юго-западный угол области, долгота
    constexpr double AreaDLat   = 0.009;    ///< размер области по широте (~1 км)
    constexpr double AreaDLon   = 0.014;    ///< размер области по долготе (~1 км на 50° с. ш.)

    constexpr int    SnakeRows  = 5;        ///< рядов «змейки»
    constexpr int    SnakeCols  = 9;        ///< точек в ряду

    constexpr int    TestSec    = 10;       ///< длительность проверки, с
    constexpr int    ExpectSec  = 20;       ///< длительность ожидания, с
    constexpr int    ReturnSec  = 15;       ///< длительность возвращения, с
    constexpr int    WorkStepS  = 2;        ///< интервал измерений, с
    constexpr int    SpectrumStartS   = 2;  ///< от начала этапа до ID27, с
    constexpr int    SpectrumPortionS = 3;  ///< интервал порций, с
    constexpr int    SpectrumPortions = 8;  ///< порций в наборе

    constexpr double MetersPerDegLat = 111320.0;    ///< метров в градусе широты
    constexpr double Pi = 3.14159265358979323846;   ///< своё число π: Pi в MSVC без _USE_MATH_DEFINES нет

    constexpr double HotspotPeak   = 2.0;   ///< прирост ПАЕД в центре пятна, мкЗв/ч
    constexpr double HotspotSigmaM = 120.0; ///< «ширина» пятна, м
    constexpr double Background    = 0.12;  ///< фон, мкЗв/ч

    /// Точка старта дрона — ~1,5 км к юго-западу от области
    const GeoPoint StartPoint{AreaLat0 - 0.012, AreaLon0 - 0.018};

    /// Случайное число 0…1
    double rnd() { return QRandomGenerator::global()->generateDouble(); }

    /// Текущее время UTC без миллисекунд — как у логгера (ЧЧ:ММ:СС)
    QDateTime nowUtc()
    {
        QDateTime t = QDateTime::currentDateTimeUtc();
        return t.addMSecs(-t.time().msec());
    }

    /// Точка на отрезке a → b, доля k = 0…1
    GeoPoint lerp(const GeoPoint &a, const GeoPoint &b, double k)
    {
        return { a.lat + (b.lat - a.lat) * k, a.lon + (b.lon - a.lon) * k };
    }

    /// Расстояние между точками, м (плоское приближение — для 1 км достаточно)
    double distanceM(const GeoPoint &a, const GeoPoint &b)
    {
        const double dy = (b.lat - a.lat) * MetersPerDegLat;
        const double dx = (b.lon - a.lon) * MetersPerDegLat * std::cos(a.lat * Pi / 180.0);
        return std::sqrt(dx * dx + dy * dy);
    }
}

// ============================================================================
//  Создание
// ============================================================================

CoreStub::CoreStub(QObject *parent)
    : QObject(parent)
{
    // Таймеры — дочерние объекты: при moveToThread переезжают в поток ядра
    // вместе с заглушкой. Запускать их можно только из потока ядра.
    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, &CoreStub::onTick);

    m_sdTimer = new QTimer(this);
    m_sdTimer->setInterval(300);
    connect(m_sdTimer, &QTimer::timeout, this, &CoreStub::onSdTimer);

    m_pos = StartPoint;
}

// ============================================================================
//  Связь
// ============================================================================

void CoreStub::openPort(const QString &name)
{
    m_portOpen = true;
    m_portName = name;
    m_sinceLastPacket = -1;             // пакетов ещё не было

    sendEvent(EventCode::PortOpened, {{QStringLiteral("port"), name}});
    setPhase(Phase::Test);              // сценарий — с начала
    sendLink();
    m_tick->start();
}

void CoreStub::closePort()
{
    m_tick->stop();
    m_portOpen = false;
    sendEvent(EventCode::PortClosed);
    sendLink();
}

// ============================================================================
//  Команды и область
// ============================================================================

void CoreStub::sendCommand(const CommandRequest &request)
{
    CommandOutcome out;
    out.request = request;
    out.sentTime = nowUtc();

    // Порт закрыт — отказ сразу, как будет в настоящем ядре
    if (!m_portOpen) {
        out.result = CommandResult::Rejected;
        out.reason = RejectReason::PortClosed;
        out.finishTime = out.sentTime;
        emit commandFinished(out);
        return;
    }

    // Иначе — «логгер ответил ID01» через 1 с. Режим и значения не проверяются,
    // действие команды на сценарий не влияет.
    // this — контекст: если заглушку удалят раньше, таймер отменится.
    QTimer::singleShot(1000, this, [this, out]() mutable {
        out.result = CommandResult::Done;
        out.finishTime = nowUtc();
        out.attempts = 1;
        emit commandFinished(out);
    });
}

void CoreStub::checkArea(const QVector<GeoPoint> &vertices, bool isRoute)
{
    // Только простые проверки; полная — в ReconArea (шаг 7)
    AreaCheck check;
    check.vertices = vertices;

    if (isRoute)
        check.error = AreaError::RouteNotSupported;
    else if (vertices.size() < CoreConst::AreaMinVertices)
        check.error = AreaError::TooFewVertices;
    else if (vertices.size() > CoreConst::AreaMaxVertices)
        check.error = AreaError::TooManyVertices;

    check.ok = (check.error == AreaError::None);
    emit areaChecked(check);
}

// ============================================================================
//  Сессии и архив
// ============================================================================

void CoreStub::startSession(const QString &name)
{
    m_session = SessionInfo();
    m_session.id = 100;                 // номер тестового текущего полёта
    m_session.name = name;
    m_session.startTime = nowUtc();
    m_sessionActive = true;
    emit sessionStarted(m_session);
}

void CoreStub::finishSession()
{
    if (!m_sessionActive)
        return;
    m_session.endTime = nowUtc();
    m_session.measurementCount = m_measurements.size();
    m_sessionActive = false;
    emit sessionFinished(m_session);
}

void CoreStub::listSessions()
{
    emit sessionsListed(testSessions());
}

void CoreStub::openSession(qint64 sessionId)
{
    emit sessionData(makeTestFlight(sessionId));
}

void CoreStub::queryArchive(const SessionFilter &filter)
{
    ArchiveResult result;
    result.filter = filter;

    // Прямоугольник области фильтра — грубая проверка;
    // точная («внутри многоугольника») — в настоящем ядре
    double latMin = -90, latMax = 90, lonMin = -180, lonMax = 180;
    if (!filter.area.isEmpty()) {
        latMin = lonMin = std::numeric_limits<double>::max();
        latMax = lonMax = std::numeric_limits<double>::lowest();
        for (const GeoPoint &p : filter.area) {
            latMin = qMin(latMin, p.lat); latMax = qMax(latMax, p.lat);
            lonMin = qMin(lonMin, p.lon); lonMax = qMax(lonMax, p.lon);
        }
    }

    for (const SessionInfo &info : testSessions()) {
        if (filter.sessionId != -1 && filter.sessionId != info.id)
            continue;

        const SessionData flight = makeTestFlight(info.id);
        for (const Measurement &m : flight.measurements) {
            if (filter.from.isValid() && m.time < filter.from) continue;
            if (filter.to.isValid()   && m.time > filter.to)   continue;
            if (m.lat < latMin || m.lat > latMax || m.lon < lonMin || m.lon > lonMax) continue;

            // Тип точки — по порогам из настроек
            quint32 type = ApMeasurement;
            if (m.shownPaed() >= m_settings.warnLevel)  type |= ApWarn;
            if (m.shownPaed() >= m_settings.alarmLevel) type |= ApAlarm;
            if (!(type & filter.types))
                continue;

            ArchivePoint p;
            p.id = m.id;
            p.sessionId = info.id;
            p.time = m.time;
            p.lat = m.lat;
            p.lon = m.lon;
            p.type = type;
            p.paed = m.shownPaed();
            result.points.append(p);
        }
    }
    emit archiveResult(result);
}

void CoreStub::requestTimeSeries(qint64 sessionId)
{
    emit timeSeriesReady(sessionId, makeTestFlight(sessionId).measurements);
}

// ============================================================================
//  SD и экспорт
// ============================================================================

void CoreStub::importSd(const QStringList &files)
{
    m_sdFiles = files;
    m_sdPercent = 0;
    emit sdProgress(0);
    m_sdTimer->start();                 // дальше — onSdTimer, +10 % каждые 0,3 с
}

void CoreStub::onSdTimer()
{
    m_sdPercent += 10;
    emit sdProgress(m_sdPercent);
    if (m_sdPercent < 100)
        return;

    m_sdTimer->stop();

    // Тестовый отчёт — числа условные
    SdReport report;
    report.files = m_sdFiles;
    report.matched = 40;
    report.onlySd = 5;
    report.onlyAir = 3;
    report.spectraMismatch = 0;
    report.badLines = 1;
    report.details << QStringLiteral("Тест: 5 пакетов ID22 найдены только на SD");
    emit sdImported(report);
}

void CoreStub::exportCsv(qint64 sessionId, const QString &folder, quint32 parts)
{
    Q_UNUSED(sessionId)
    Q_UNUSED(parts)
    emit exportFinished(true, folder);  // файлы не пишутся
}

// ============================================================================
//  Настройки и тревоги
// ============================================================================

void CoreStub::applySettings(const CoreSettings &settings)
{
    m_settings = settings;
}

void CoreStub::acknowledgeAlarm(qint64 alarmId)
{
    emit alarmAcknowledged(alarmId);
}

// ============================================================================
//  Сценарий
// ============================================================================

void CoreStub::onTick()
{
    // Секунды с последнего «пакета»; шаг сценария обнулит счётчик, если что-то передаст
    if (m_sinceLastPacket >= 0)
        ++m_sinceLastPacket;

    const Phase before = m_phase;
    switch (m_phase) {
    case Phase::Test:     stepTest();     break;
    case Phase::Expect:   stepExpect();   break;
    case Phase::Work:     stepWork();     break;
    case Phase::Spectrum: stepSpectrum(); break;
    case Phase::Return:   stepReturn();   break;
    }

    // Этап не сменился — идёт следующая секунда этапа
    // (setPhase сам обнуляет счётчик при смене)
    if (m_phase == before)
        ++m_phaseSec;

    sendLink();
}

/**
    @brief Проверка: ответы приборов по очереди, как при ID01 (протокол, п. 6.2).
*/
void CoreStub::stepTest()
{
    auto setDevice = [this](LoggerDevice d, quint8 msgId) {
        m_status.devices[int(d)].check = DeviceCheck::Ok;
        m_status.devices[int(d)].msgId = msgId;
    };

    switch (m_phaseSec) {
    case 1:                             // ID01 на ID01 — проверка началась
        m_status.checking = true;
        sendStatus();
        break;
    case 2:                             // GPS: стартовая координата ID19
        setDevice(LoggerDevice::Gps, 0x13);
        sendRoute(0x13, false);
        sendStatus();
        break;
    case 4:
        setDevice(LoggerDevice::Lidar, 0x0B);
        sendEvent(EventCode::LidarOk);
        sendStatus();
        break;
    case 6:
        setDevice(LoggerDevice::Bdbg, 0x09);
        sendEvent(EventCode::BdbgOk);
        sendStatus();
        break;
    case 8:
        setDevice(LoggerDevice::Battery, 0x05);
        m_status.battV = 12.0f;         // тестовое значение
        m_status.hasBattV = true;
        sendEvent(EventCode::BatteryOk, {{QStringLiteral("voltage"), 12.0}});
        sendStatus();
        break;
    case TestSec:                       // ID18 — проверка пройдена
        setDevice(LoggerDevice::Sd, 0x12);
        m_status.checking = false;
        sendEvent(EventCode::CheckPassed);
        m_status.reconMode = ReconMode::Time;   // как будто ID04 принята
        setPhase(Phase::Expect);
        break;
    default:
        break;
    }
}

/**
    @brief Ожидание: полёт от точки старта к области, ID19 раз в 5 с.
*/
void CoreStub::stepExpect()
{
    m_pos = lerp(StartPoint, snakePoint(0), double(m_phaseSec) / ExpectSec);

    if (m_phaseSec > 0 && m_phaseSec % 5 == 0)
        sendRoute(0x13, false);

    if (m_phaseSec >= ExpectSec) {
        m_workIndex = 0;
        setPhase(Phase::Work);
    }
}

/**
    @brief Разведка: ID20 при входе, затем ID22 раз в 2 с по «змейке».
*/
void CoreStub::stepWork()
{
    if (m_phaseSec == 0) {
        m_pos = snakePoint(0);
        sendRoute(0x14, true);          // ID20 — вход в область
    }

    if (m_phaseSec % WorkStepS != 0)
        return;

    if (m_workIndex >= snakeCount()) {  // «змейка» пройдена — набор спектра
        setPhase(Phase::Spectrum);
        return;
    }

    m_pos = snakePoint(m_workIndex++);
    sendMeasurement();
}

/**
    @brief Набор спектра: подготовка, ID27, порции раз в 3 с, окончание по времени.
*/
void CoreStub::stepSpectrum()
{
    const int lastPortionSec = SpectrumStartS + SpectrumPortionS * SpectrumPortions;

    if (m_phaseSec == 0) {              // ID01 на ID09 = 3 — подготовка
        m_status.spectrumStage = SpectrumStage::Preparing;
        sendStatus();
        sendAllowed();
        return;
    }

    if (m_phaseSec == SpectrumStartS) { // ID27 — набор начат
        m_spectrum = Spectrum();
        m_spectrum.info.id = m_nextId++;
        m_spectrum.info.number = ++m_spectrumNumber;
        m_spectrum.info.startTime = nowUtc();
        m_spectrum.info.lat = m_pos.lat;
        m_spectrum.info.lon = m_pos.lon;
        m_spectrum.info.lidarCm = 1500;
        m_spectrum.info.setDurationS = SpectrumPortionS * SpectrumPortions;

        m_status.spectrumStage = SpectrumStage::Collecting;
        m_sinceLastPacket = 0;
        sendStatus();
        sendAllowed();
        emit spectrumUpdated(m_spectrum);
        return;
    }

    if (m_phaseSec > SpectrumStartS && m_phaseSec <= lastPortionSec
        && (m_phaseSec - SpectrumStartS) % SpectrumPortionS == 0) {
        // ID23 — порция: прибавить к сумме с насыщением 65535
        const SpectrumPortion portion = makePortion();
        for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch) {
            const quint32 sum = quint32(m_spectrum.channels[ch]) + portion.channels[ch];
            m_spectrum.channels[ch] = quint16(qMin<quint32>(sum, CoreConst::SpectrumMaxCount));
        }
        m_spectrum.portionList.append(portion);
        m_spectrum.info.portions = m_spectrum.portionList.size();
        m_spectrum.info.lastPaed = portion.paed;
        m_spectrum.info.lastStat = portion.stat;
        m_sinceLastPacket = 0;

        // В spectrumUpdated — только последняя порция (см. core_types.h)
        Spectrum update = m_spectrum;
        update.portionList = { portion };
        emit spectrumUpdated(update);
    }

    if (m_phaseSec >= lastPortionSec) { // истекло заданное время
        m_spectrum.info.finished = true;
        m_spectrum.info.stopReason = SpectrumStopReason::TimeElapsed;
        emit spectrumFinished(m_spectrum);

        m_status.spectrumStage = SpectrumStage::None;
        setPhase(Phase::Return);
    }
}

/**
    @brief Возвращение: полёт от центра области на северо-восток, ID19 раз в 5 с.
           В конце — сценарий с начала.
*/
void CoreStub::stepReturn()
{
    const GeoPoint away{AreaLat0 + AreaDLat + 0.012, AreaLon0 + AreaDLon + 0.018};
    m_pos = lerp(areaCenter(), away, double(m_phaseSec) / ReturnSec);

    if (m_phaseSec > 0 && m_phaseSec % 5 == 0)
        sendRoute(0x13, false);

    if (m_phaseSec >= ReturnSec)
        setPhase(Phase::Test);
}

void CoreStub::setPhase(Phase phase)
{
    m_phase = phase;
    m_phaseSec = 0;

    switch (phase) {
    case Phase::Test:                   // новый круг — логгер как после включения
        m_status = LoggerStatus();
        m_status.mode = LoggerMode::Test;
        m_measurements.clear();
        m_aboveWarn = m_aboveAlarm = false;
        m_workIndex = 0;
        m_pos = StartPoint;
        break;
    case Phase::Expect:
        m_status.mode = LoggerMode::Expect;
        break;
    case Phase::Work:
    case Phase::Spectrum:               // спектр — внутри режима «Разведка»
        m_status.mode = LoggerMode::Work;
        break;
    case Phase::Return:
        m_status.mode = LoggerMode::Return;
        break;
    }

    sendStatus();
    sendAllowed();
}

// ============================================================================
//  Выдача данных
// ============================================================================

void CoreStub::sendLink()
{
    LinkStatus s;
    s.portName = m_portName;
    s.secondsSinceLastPacket = m_sinceLastPacket;

    if (!m_portOpen)
        s.state = LinkState::Closed;
    else if (m_sinceLastPacket < 0)
        s.state = LinkState::Waiting;
    else
        s.state = LinkState::Ok;        // заглушка связь не теряет

    emit linkStatusChanged(s);
}

void CoreStub::sendStatus()
{
    emit loggerStatusChanged(m_status);
}

/**
    @brief Разрешённые команды — по матрице «команда × режим» (протокол, п. 5).
*/
void CoreStub::sendAllowed()
{
    QList<quint8> ids;
    switch (m_status.mode) {
    case LoggerMode::Test:
        ids = {1};
        break;
    case LoggerMode::Expect:
        ids = {4, 5, 6, 7, 8, 9, 11, 14};
        break;
    case LoggerMode::Work:
        if (m_status.spectrumStage == SpectrumStage::None)
            ids = {4, 5, 6, 7, 8, 9, 10, 11, 13, 14};
        else                            // при подготовке и наборе спектра ID09–ID11 запрещены, ID12 разрешена
            ids = {4, 5, 6, 7, 8, 12, 13, 14};
        break;
    case LoggerMode::Return:
        ids = {4, 5, 14};
        break;
    default:
        break;                          // Unknown, Error — ничего
    }
    emit allowedCommandsChanged(ids);
}

void CoreStub::sendRoute(quint8 msgId, bool inArea)
{
    RoutePoint p;
    p.time = nowUtc();
    p.rxTime = p.time;
    p.lat = m_pos.lat;
    p.lon = m_pos.lon;
    p.inArea = inArea;
    p.msgId = msgId;
    p.mode = m_status.mode;
    emit routePoint(p);
    m_sinceLastPacket = 0;
}

void CoreStub::sendEvent(EventCode code, const QVariantMap &params)
{
    CoreEvent e;
    e.id = m_nextId++;
    e.code = code;
    e.time = nowUtc();
    e.params = params;
    emit event(e);
}

void CoreStub::sendMeasurement()
{
    Measurement m = makeMeasurement(m_pos, nowUtc());
    m.id = m_nextId++;
    m_measurements.append(m);

    m_status.battV = m.battV;
    m_status.hasBattV = true;

    sendRoute(0x16, true);              // координаты ID22 — тоже точка маршрута
    emit measurement(m);
    checkDoseAlarm(m);
    sendGrid();
}

void CoreStub::checkDoseAlarm(const Measurement &m)
{
    const float v = m.shownPaed();

    AlarmLevel level;
    if (v >= m_settings.alarmLevel) {
        if (m_aboveAlarm) return;       // уже в зоне тревоги — не повторять
        m_aboveAlarm = m_aboveWarn = true;
        level = AlarmLevel::Alarm;
    } else if (v >= m_settings.warnLevel) {
        m_aboveAlarm = false;
        if (m_aboveWarn) return;        // уже в зоне предупреждения
        m_aboveWarn = true;
        level = AlarmLevel::Warning;
    } else {
        m_aboveWarn = m_aboveAlarm = false;     // вышли из зоны — следующий вход снова даст тревогу
        return;
    }

    Alarm a;
    a.id = m_nextId++;
    a.type = AlarmType::DoseRate;
    a.level = level;
    a.time = m.time;
    a.hasPos = true;
    a.lat = m.lat;
    a.lon = m.lon;
    a.value = v;
    emit alarm(a);
}

/**
    @brief Тепловая карта: обратные расстояния (IDW) по измерениям текущего круга.
    @note  Упрощённо, только для заглушки: пересчёт всей сетки, область — прямоугольник.
*/
void CoreStub::sendGrid()
{
    GridData g;
    g.latMin = AreaLat0;
    g.latMax = AreaLat0 + AreaDLat;
    g.lonMin = AreaLon0;
    g.lonMax = AreaLon0 + AreaDLon;
    g.cellM = m_settings.gridCellM > 0 ? m_settings.gridCellM : 10.0;
    g.area = testArea();

    const double mLat = MetersPerDegLat;
    const double mLon = MetersPerDegLat * std::cos(g.latMin * Pi / 180.0);
    g.rows = qMax(1, int(std::ceil(AreaDLat * mLat / g.cellM)));
    g.cols = qMax(1, int(std::ceil(AreaDLon * mLon / g.cellM)));

    const int cells = g.rows * g.cols;
    g.values = QVector<float>(cells, std::numeric_limits<float>::quiet_NaN());
    g.inArea = QVector<quint8>(cells, 1);   // квадрат: все клетки в области

    const double radius = m_settings.gridRadiusM;
    const double power  = m_settings.gridPower;

    for (int r = 0; r < g.rows; ++r) {
        for (int c = 0; c < g.cols; ++c) {
            // центр клетки
            const GeoPoint cell{ g.latMin + (r + 0.5) * g.cellM / mLat,
                                 g.lonMin + (c + 0.5) * g.cellM / mLon };
            double sumW = 0.0, sumV = 0.0;
            for (const Measurement &m : std::as_const(m_measurements)) {
                const double d = distanceM(cell, {m.lat, m.lon});
                if (d > radius) continue;
                const double w = 1.0 / std::pow(qMax(d, 1.0), power);   // вес 1 / d^p; d < 1 м считаем 1 м
                sumW += w;
                sumV += w * m.shownPaed();
            }
            if (sumW > 0.0)
                g.values[r * g.cols + c] = float(sumV / sumW);
        }
    }

    // Горячая точка (упрощённо, только для заглушки): клетка с самым большим
    // измерением, если оно выше порога предупреждения. Настоящий поиск
    // локальных максимумов сетки — DoseGrid, шаг 15.
    const Measurement *top = nullptr;
    for (const Measurement &m : std::as_const(m_measurements))
        if (!top || m.shownPaed() > top->shownPaed())
            top = &m;
    if (top && top->shownPaed() >= m_settings.warnLevel) {
        const int r = qBound(0, int((top->lat - g.latMin) * mLat / g.cellM), g.rows - 1);
        const int c = qBound(0, int((top->lon - g.lonMin) * mLon / g.cellM), g.cols - 1);
        g.hotspots.append(r * g.cols + c);
    }

    emit gridUpdated(g);
}

// ============================================================================
//  Тестовые данные
// ============================================================================

QVector<GeoPoint> CoreStub::testArea()
{
    // Против часовой стрелки: ЮЗ, ЮВ, СВ, СЗ
    return { {AreaLat0,            AreaLon0},
             {AreaLat0,            AreaLon0 + AreaDLon},
             {AreaLat0 + AreaDLat, AreaLon0 + AreaDLon},
             {AreaLat0 + AreaDLat, AreaLon0} };
}

GeoPoint CoreStub::areaCenter()
{
    return { AreaLat0 + AreaDLat / 2, AreaLon0 + AreaDLon / 2 };
}

int CoreStub::snakeCount()
{
    return SnakeRows * SnakeCols;
}

/**
    @brief Точка «змейки»: ряды с юга на север, в нечётных рядах — в обратную сторону.
    @note  Отступ от края области — полшага.
*/
GeoPoint CoreStub::snakePoint(int index)
{
    const int row = index / SnakeCols;
    int col = index % SnakeCols;
    if (row % 2 == 1)
        col = SnakeCols - 1 - col;

    const double stepLat = AreaDLat / SnakeRows;
    const double stepLon = AreaDLon / SnakeCols;
    return { AreaLat0 + (row + 0.5) * stepLat, AreaLon0 + (col + 0.5) * stepLon };
}

float CoreStub::doseAt(const GeoPoint &p)
{
    const double d = distanceM(p, areaCenter());
    return float(Background + HotspotPeak * std::exp(-d * d / (2 * HotspotSigmaM * HotspotSigmaM)));
}

Measurement CoreStub::makeMeasurement(const GeoPoint &p, const QDateTime &time) const
{
    Measurement m;
    m.time = time;
    m.rxTime = time;
    m.paed = doseAt(p) * float(0.95 + 0.1 * rnd());     // ±5 % шум
    m.tempCode = 0x015A;
    m.bdbgStat = 0x11;
    m.lidarCm = quint16(1450 + 100 * rnd());            // 14,5–15,5 м
    m.lidarStrength = 0x0FFF;
    m.lat = p.lat;
    m.lon = p.lon;
    m.battV = float(11.8 + 0.2 * rnd());
    return m;
}

/**
    @brief Порция спектра: спад фона + пик около канала 220.
*/
SpectrumPortion CoreStub::makePortion()
{
    SpectrumPortion p;
    p.channels.resize(CoreConst::SpectrumChannels);
    for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch) {
        const double base = 40.0 * std::exp(-ch / 250.0)
                          + 60.0 * std::exp(-(ch - 220.0) * (ch - 220.0) / (2 * 12.0 * 12.0));
        p.channels[ch] = quint16(base * (0.8 + 0.4 * rnd()));
    }
    p.accTime = 6;
    p.paed = float(Background + HotspotPeak);
    p.stat = 0x11;
    p.rxTime = nowUtc();
    return p;
}

Spectrum CoreStub::makeTestSpectrum(quint16 number, int portions)
{
    Spectrum s;
    s.info.number = number;
    s.info.lat = areaCenter().lat;
    s.info.lon = areaCenter().lon;
    s.info.lidarCm = 1500;
    s.info.setDurationS = quint16(portions * 6);
    for (int i = 0; i < portions; ++i) {
        const SpectrumPortion p = makePortion();
        for (int ch = 0; ch < CoreConst::SpectrumChannels; ++ch) {
            const quint32 sum = quint32(s.channels[ch]) + p.channels[ch];
            s.channels[ch] = quint16(qMin<quint32>(sum, CoreConst::SpectrumMaxCount));
        }
        s.portionList.append(p);
    }
    s.info.portions = portions;
    s.info.lastPaed = s.portionList.last().paed;
    s.info.lastStat = s.portionList.last().stat;
    s.info.finished = true;
    s.info.stopReason = SpectrumStopReason::TimeElapsed;
    return s;
}

QVector<SessionInfo> CoreStub::testSessions() const
{
    QVector<SessionInfo> list;
    const QDate today = QDate::currentDate();
    for (int i = 1; i <= 3; ++i) {
        SessionInfo s;
        s.id = i;
        s.name = QStringLiteral("Тест %1").arg(i);
        s.startTime = QDateTime(today.addDays(i - 4), QTime(9, 0));
        s.startTime.setTimeZone(QTimeZone::utc());     // 09:00 UTC, без перевода из местного времени
        s.endTime = s.startTime.addSecs(40 * 60);
        s.measurementCount = snakeCount();
        list.append(s);
    }
    return list;
}

/**
    @brief Прошлый полёт: «змейка» по области, один спектр, события, тревоги.
*/
SessionData CoreStub::makeTestFlight(qint64 sessionId) const
{
    SessionData d;
    for (const SessionInfo &s : testSessions())
        if (s.id == sessionId)
            d.info = s;
    if (d.info.id == 0) {               // неизвестный номер — пустой полёт с этим номером
        d.info.id = sessionId;
        d.info.name = QStringLiteral("Тест %1").arg(sessionId);
        d.info.startTime = nowUtc();
    }
    d.info.readOnly = true;
    d.areas = { testArea() };

    bool warnDone = false, alarmDone = false;
    for (int i = 0; i < snakeCount(); ++i) {
        const GeoPoint p = snakePoint(i);
        Measurement m = makeMeasurement(p, d.info.startTime.addSecs(60 + i * WorkStepS));
        m.id = sessionId * 1000 + i;    // номера не пересекаются между полётами
        d.measurements.append(m);

        RoutePoint r;
        r.time = r.rxTime = m.time;
        r.lat = p.lat;
        r.lon = p.lon;
        r.inArea = true;
        r.msgId = 0x16;
        r.mode = LoggerMode::Work;
        d.route.append(r);

        // Тревоги — один раз на каждый уровень
        const float v = m.shownPaed();
        const bool isAlarm = v >= m_settings.alarmLevel && !alarmDone;
        const bool isWarn  = !isAlarm && v >= m_settings.warnLevel && !warnDone;
        if (isAlarm || isWarn) {
            Alarm a;
            a.id = m.id;
            a.type = AlarmType::DoseRate;
            a.level = isAlarm ? AlarmLevel::Alarm : AlarmLevel::Warning;
            a.time = m.time;
            a.hasPos = true;
            a.lat = m.lat;
            a.lon = m.lon;
            a.value = v;
            a.acknowledged = true;
            d.alarms.append(a);
            (isAlarm ? alarmDone : warnDone) = true;
        }
    }

    Spectrum s = makeTestSpectrum(1, SpectrumPortions);
    s.info.id = sessionId * 1000 + 999;
    s.info.startTime = d.info.startTime.addSecs(60 + snakeCount() * WorkStepS);
    d.spectra.append(s);

    CoreEvent e;
    e.id = sessionId * 1000 + 998;
    e.code = EventCode::CheckPassed;
    e.time = d.info.startTime;
    d.events.append(e);

    CommandOutcome c;
    c.request.id = 4;
    c.request.vertices = testArea();
    c.request.reconMode = ReconMode::Time;
    c.request.intervalS = WorkStepS;
    c.result = CommandResult::Done;
    c.sentTime = c.finishTime = d.info.startTime.addSecs(30);
    c.attempts = 1;
    d.commands.append(c);

    return d;
}
