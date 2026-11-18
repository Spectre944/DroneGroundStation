#ifndef CORE_TYPES_H
#define CORE_TYPES_H

/**
    @file  core_types.h
    @brief Общие типы данных ядра НПО «ДРОН-1».

    Структуры передаются в сигналах между классами ядра и из ядра в интерфейс.
    Ядро не использует типы интерфейса (backend/data_types, map):
    перевод в типы интерфейса делает интерфейс.

    @note  Всё время — UTC.
    @note  Форматы сообщений — «Протокол обмена для наземного ПО», п. 6.
*/

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>


/////////////////////////////////// Константы протокола ///////////////////////////////////

/**
    @brief Константы протокола и логгера.
    @note  Значения взяты из протокола; в коде использовать только их, без чисел.
*/
namespace CoreConst {
    constexpr int     SpectrumChannels  = 1024;     ///< каналов в спектре БДБГ-15 (ID23)
    constexpr quint16 SpectrumMaxCount  = 65535;    ///< насыщение суммы в канале — как в логгере, иначе итог не совпадёт с SD
    constexpr quint16 SpectrumStopCount = 55705;    ///< 85 % от 65535: логгер сам прекращает набор, если в любом канале суммы это значение
    constexpr int     AreaMinVertices   = 3;        ///< наименьшее число вершин области (ID04)
    constexpr int     AreaMaxVertices   = 20;       ///< наибольшее число вершин области (ID04)
    constexpr int     RetryMinS         = 20;       ///< повтор команды без ответа — не раньше, с
}


/////////////////////////////////// Общие типы ///////////////////////////////////

/**
    @brief Точка на карте.
    @note  double, хотя логгер передаёт float: точность float около 1 м,
           для проверки области и сетки этого мало. Во float вершины
           переводятся только при сборке ID04.
*/
struct GeoPoint {
    double lat = 0.0;   ///< широта, градусы (северная — положительная)
    double lon = 0.0;   ///< долгота, градусы (восточная — положительная)
};

/**
    @brief Источник данных.
    @note  После импорта SD (Ф13) в базе есть данные из эфира и найденные только на SD.
*/
enum class DataSource : quint8 {
    Air,    ///< принято по радиолинии
    Sd      ///< добавлено с SD-карты
};

/**
    @brief Пакет радиолинии после проверки.
    @note  Маркеры начала и конца, Length и CRC уже отброшены (их проверил DroneLink).
           Пакет «без данных» — payload из одного байта 0x01.
*/
struct Packet {
    quint8     id = 0;      ///< ID сообщения
    QByteArray payload;     ///< данные сообщения
    QDateTime  rxTime;      ///< время приёма по часам компьютера: отсев копий, дата, журнал
};


/////////////////////////////////// Связь (Ф1) ///////////////////////////////////

/**
    @brief Состояние связи с логгером.
*/
enum class LinkState : quint8 {
    Closed,         ///< COM-порт закрыт
    Waiting,        ///< порт открыт, пакетов ещё не было
    Ok,             ///< пакеты идут
    Silent,         ///< пакетов нет, и так должно быть (радиомолчание)
    Lost            ///< пакетов нет дольше порога — связь потеряна
};

/**
    @brief Связь: состояние и счётчики для интерфейса.
    @note  Передаётся раз в секунду и при каждом изменении состояния.
*/
struct LinkStatus {
    LinkState state = LinkState::Closed;
    QString   portName;                     ///< открытый порт, например «COM5»
    int       secondsSinceLastPacket = -1;  ///< секунд с последнего пакета; -1 — пакетов ещё не было
    bool      commandPending = false;       ///< команда отправлена, ждём ответа — новую не отправить
};


/////////////////////////////////// Состояние логгера (Ф4) ///////////////////////////////////

/**
    @brief Режим логгера.
    @note  Логгер режим не сообщает — ядро вычисляет его по событиям (протокол, п. 3.5).
*/
enum class LoggerMode : quint8 {
    Unknown,    ///< не известен (после запуска программы)
    Test,       ///< проверка
    Expect,     ///< ожидание: полёт к области
    Work,       ///< разведка
    Return,     ///< возвращение
    Error       ///< авария — выход только выключением питания
};

/**
    @brief Режим разведки (ID04, ID09).
    @note  Значение 3 (набор спектра) — не режим, а этап, см. SpectrumStage.
*/
enum class ReconMode : quint8 {
    Operator = 0,   ///< измерение по команде оператора (ID10)
    Time     = 1,   ///< по времени
    Distance = 2,   ///< по расстоянию
    Unknown  = 255  ///< ещё не задан
};

/**
    @brief Этап набора спектра.
*/
enum class SpectrumStage : quint8 {
    None,           ///< набора нет
    Preparing,      ///< подготовка: после ID01 на ID09 = 3
    Collecting,     ///< идёт набор: после ID27
    Finished        ///< набор окончен
};

/**
    @brief Приборы, проверяемые при предполётной проверке.
    @note  Count — число приборов, размер массива LoggerStatus::devices.
*/
enum class LoggerDevice : quint8 {
    Gps,
    Lidar,
    Bdbg,
    Battery,
    Sd,
    Count
};

/**
    @brief Результат проверки прибора.
*/
enum class DeviceCheck : quint8 {
    NotChecked,     ///< не проверялся
    Ok,             ///< исправен
    Fail            ///< ошибка
};

/**
    @brief Состояние одного прибора.
    @note  msgId — сообщение, давшее результат, чтобы оператор понял причину:
           GPS — ID19 / ID15, ID26; LIDAR — ID11 / ID12; БДБГ — ID09 / ID07, ID08;
           батарея — ID05 / ID04, ID06; SD — ID18 / ID16; в полёте — ID25.
*/
struct DeviceState {
    DeviceCheck check = DeviceCheck::NotChecked;
    quint8      msgId = 0;      ///< 0 — сообщений не было
};

/**
    @brief Состояние логгера целиком — для панели состояния.
*/
struct LoggerStatus {
    LoggerMode    mode = LoggerMode::Unknown;
    ReconMode     reconMode = ReconMode::Unknown;   ///< из подтверждённых ID04 / ID09
    SpectrumStage spectrumStage = SpectrumStage::None;
    bool          checking = false;         ///< идёт проверка: от ID01 на ID01 до ID18 или ошибки
    bool          noSd = false;             ///< работа без SD: после ID01 на ID02
    bool          radioSilence = false;     ///< после ID01 на ID13, до ID01 на ID14
    bool          modeMismatch = false;     ///< ID03 на команду, разрешённую в нашем режиме: логгер в другом режиме
    QVector<DeviceState> devices = QVector<DeviceState>(int(LoggerDevice::Count)); ///< индекс — LoggerDevice
    float         battV = 0.0f;             ///< последнее напряжение аккумулятора (ID04, ID05, ID22), В
    bool          hasBattV = false;         ///< battV получено
    quint8        errorMsgId = 0;           ///< причина аварии: ID24, ID25, ID26, ID28; 0 — аварии нет
    quint8        errorDevice = 0;          ///< для ID25: 0 — БДБГ, 1 — LIDAR, 2 — GPS, 3 — аккумулятор
};


/////////////////////////////////// Маршрут и измерения (Ф5, Ф6, Ф9) ///////////////////////////////////

/**
    @brief Точка маршрута — из ID19, ID20, ID22.
    @note  Логгер передаёт только ЧЧ:ММ:СС; дата — по часам компьютера (UTC)
           с поправкой около полуночи.
*/
struct RoutePoint {
    QDateTime  time;                ///< время логгера + дата
    QDateTime  rxTime;              ///< время приёма
    double     lat = 0.0;
    double     lon = 0.0;
    bool       inArea = false;      ///< ID20, ID22 — да; ID19 — нет
    quint8     msgId = 0;           ///< 19, 20 или 22
    LoggerMode mode = LoggerMode::Unknown;  ///< режим логгера при приёме (стартовая точка проверки — для контроля спуфинга)
    DataSource source = DataSource::Air;
};

/**
    @brief Флаги достоверности измерения (Ф9), можно объединять через |.
    @note  Состав Ф9 — открытый вопрос; 0 — измерение достоверно.
           Недостоверные точки отмечаются на карте и не идут в интерполяцию.
*/
enum MeasurementFlag : quint32 {
    MfNone          = 0,
    MfOutOfArea     = 1 << 0,   ///< точка вне принятой области
    MfLidarRange    = 1 << 1,   ///< высота LIDAR вне допустимого диапазона
    MfBdbgStat      = 1 << 2,   ///< байт состояния БДБГ сообщает о проблеме (расшифровка Stat — открытый вопрос)
    MfDeviceFault   = 1 << 3    ///< в цикле был отказ прибора
};

/**
    @brief Измерение — из ID22 (31 байт) плюс результаты обработки.
*/
struct Measurement {
    qint64     id = 0;              ///< номер в базе; 0 — ещё не записано
    QDateTime  time;                ///< время логгера + дата
    QDateTime  rxTime;              ///< время приёма
    float      paed = 0.0f;         ///< ПАЕД из ID22, без пересчёта, мкЗв/ч
    float      paed1m = 0.0f;       ///< ПАЕД на 1 м (Ф6)
    bool       hasPaed1m = false;   ///< paed1m посчитан (пересчёт включён и высота в допустимом диапазоне)
    quint16    tempCode = 0;        ///< температура БДБГ, код без пересчёта
    quint8     bdbgStat = 0;        ///< байт Stat БДБГ
    quint16    lidarCm = 0;         ///< высота LIDAR, см
    quint16    lidarStrength = 0;   ///< мощность отражённого сигнала LIDAR
    double     lat = 0.0;
    double     lon = 0.0;
    float      battV = 0.0f;        ///< напряжение аккумулятора, В
    quint32    flags = MfNone;      ///< флаги MeasurementFlag
    DataSource source = DataSource::Air;

    /**
        @brief  Значение для показа, сетки и тревог.
        @return ПАЕД на 1 м, если посчитана, иначе исходная.
    */
    float shownPaed() const { return hasPaed1m ? paed1m : paed; }
};


/////////////////////////////////// Спектры (Ф10) ///////////////////////////////////

/**
    @brief Причина окончания набора спектра.
    @note  Логгер причину не передаёт — ядро определяет её само.
*/
enum class SpectrumStopReason : quint8 {
    None,           ///< набор идёт
    Measurement,    ///< пришло ID22 — логгер вернулся к измерениям
    CommandStop,    ///< ID01 на ID12
    CommandFinish,  ///< ID01 на ID08
    NewArea,        ///< ID01 на ID04
    NewSpectrum,    ///< пришёл новый ID27, а старый не закрыт
    LoggerError,    ///< авария
    TimeElapsed,    ///< истекло заданное время набора
    ChannelFull     ///< в канале суммы 55705
};

/**
    @brief Порция спектра — из ID23 (2055 байт).
*/
struct SpectrumPortion {
    QVector<quint16> channels;      ///< 1024 канала порции
    quint16    accTime = 0;         ///< время набора порции (Acc Time БДБГ), без пересчёта
    float      paed = 0.0f;         ///< ПАЕД из ответа БДБГ
    quint8     stat = 0;            ///< байт Stat БДБГ
    QDateTime  rxTime;              ///< время приёма
};

/**
    @brief Сведения о спектре — из ID27 (22 байта) и по ходу набора.
*/
struct SpectrumInfo {
    qint64     id = 0;              ///< номер в базе
    quint16    number = 0;          ///< номер спектра логгера: с 1 после включения
    QDateTime  startTime;           ///< дата и время из ID27
    double     lat = 0.0;           ///< точка набора
    double     lon = 0.0;
    quint16    lidarCm = 0;         ///< высота LIDAR в точке набора, см
    quint16    setDurationS = 0;    ///< заданное время набора (ID04, ID11), с; 0 — до ID12
    int        portions = 0;        ///< принято порций
    float      lastPaed = 0.0f;     ///< ПАЕД из последней порции
    quint8     lastStat = 0;        ///< Stat из последней порции
    bool       finished = false;    ///< набор окончен
    SpectrumStopReason stopReason = SpectrumStopReason::None;
    DataSource source = DataSource::Air;
};

/**
    @brief Спектр: сведения, накопленная сумма, порции.
    @note  В сигнале spectrumUpdated portionList содержит только последнюю порцию,
           в spectrumFinished — все порции набора (иначе каждый сигнал копирует всё).
*/
struct Spectrum {
    SpectrumInfo info;
    QVector<quint16> channels = QVector<quint16>(CoreConst::SpectrumChannels, 0);  ///< сумма порций с насыщением 65535
    QList<SpectrumPortion> portionList;     ///< порции — для «водопада»
};


/////////////////////////////////// Команды (Ф2) ///////////////////////////////////

/**
    @brief Команда наземной станции ID01–ID14.
    @note  Для ID04 заполняются поля области и параметров,
           для ID05, ID06, ID07, ID09, ID11 — value, для остальных — ничего.
*/
struct CommandRequest {
    quint32    seq = 0;             ///< номер запроса — присваивает ядро; по нему интерфейс узнаёт ответ
    quint8     id = 0;              ///< ID команды 1–14

    // ---- ID04 ----
    QVector<GeoPoint> vertices;     ///< вершины области, 3–20
    ReconMode  reconMode = ReconMode::Operator;     ///< 0–2
    quint16    intervalS = 0;       ///< интервал между измерениями, с; > 0
    quint16    distanceM = 0;       ///< расстояние между измерениями, м; > 0
    quint8     dupFactor = 2;       ///< коэффициент дублирования пакетов логгера, 2–5
    quint16    spectrumS = 0;       ///< время набора спектра, с; 0 — до ID12

    // ---- ID05, ID06, ID07, ID09, ID11 ----
    int        value = 0;           ///< коэффициент / интервал / расстояние / режим 0–3 / время спектра
};

/**
    @brief Итог команды.
*/
enum class CommandResult : quint8 {
    Done,       ///< ID01 — выполнено
    BadPacket,  ///< ID02 — ошибка пакета, неизвестный ID или неверный Payload
    Forbidden,  ///< ID03 — запрещено в режиме или не выполнено условие
    NoAnswer,   ///< ответа нет
    Rejected    ///< не отправлена — причина в RejectReason
};

/**
    @brief Почему команда не отправлена.
*/
enum class RejectReason : quint8 {
    None,
    PortClosed,         ///< порт закрыт
    Busy,               ///< ждём ответ на предыдущую команду
    NotAllowed,         ///< запрещена в текущем режиме (по матрице)
    ReadOnly,           ///< открыт прошлый полёт — только просмотр
    BadId,              ///< ID вне 1–14
    AreaNotValid,       ///< область не прошла проверку ReconArea
    BadReconMode,       ///< режим вне 0–2 (ID04) или 0–3 (ID09)
    BadInterval,        ///< интервал = 0
    BadDistance,        ///< расстояние = 0
    BadDupFactor,       ///< коэффициент вне 2–5
    BadValue            ///< прочие недопустимые значения
};

/**
    @brief Команда и её итог — для интерфейса, журнала и базы.
*/
struct CommandOutcome {
    CommandRequest request;
    CommandResult  result = CommandResult::NoAnswer;
    RejectReason   reason = RejectReason::None;     ///< только для Rejected
    QDateTime      sentTime;            ///< время первой отправки
    QDateTime      finishTime;          ///< время ответа или решения «нет ответа»
    int            attempts = 0;        ///< сколько раз отправлялась
};


/////////////////////////////////// События (журнал) ///////////////////////////////////

/**
    @brief Код события.
    @note  Коды 0x01–0xFF равны ID сообщения логгера, от 0x100 — события ядра.
           Текст по коду подставляет интерфейс на своём языке; ядро текстов не формирует.
*/
enum class EventCode : quint16 {
    // ---- сообщения логгера ----
    BatteryLow          = 0x04,     ///< ID04 аккумулятор разряжен (params: voltage)
    BatteryOk           = 0x05,     ///< ID05 аккумулятор заряжен (params: voltage)
    BatteryFault        = 0x06,     ///< ID06 аккумулятор неисправен
    BdbgBadAnswer       = 0x07,     ///< ID07 БДБГ ответил некорректно
    BdbgNoAnswer        = 0x08,     ///< ID08 БДБГ не ответил
    BdbgOk              = 0x09,     ///< ID09 БДБГ протестирован
    LidarOk             = 0x0B,     ///< ID11 LIDAR протестирован
    LidarFault          = 0x0C,     ///< ID12 LIDAR неисправен
    GpsFault            = 0x0F,     ///< ID15 GPS неисправен
    SdFault             = 0x10,     ///< ID16 SD-карта неисправна
    CheckPassed         = 0x12,     ///< ID18 проверка пройдена
    PointAlreadyMeasured= 0x15,     ///< ID21 измерение в точке уже проведено
    Spoofing            = 0x18,     ///< ID24 спуфинг GPS
    DeviceFailed        = 0x19,     ///< ID25 прибор неисправен (params: device 0–3)
    ClockError          = 0x1A,     ///< ID26 ошибка часов
    SpectrumStartFailed = 0x1C,     ///< ID28 не удалось запустить набор спектра
    SelectReconMode     = 0x1D,     ///< ID29 выберите режим разведки
    DebugState          = 0x46,     ///< ID70–ID84 отладочные (params: msgId)

    // ---- события ядра ----
    UnknownId           = 0x100,    ///< неизвестный ID (params: msgId)
    BadLength           = 0x101,    ///< неверная длина Payload (params: msgId, length)
    PortOpened          = 0x110,    ///< порт открыт (params: port)
    PortClosed          = 0x111,    ///< порт закрыт
    PortError           = 0x112,    ///< ошибка порта (params: text)
    LinkLost            = 0x113,    ///< связь потеряна
    LinkRestored        = 0x114     ///< связь восстановлена
};

/**
    @brief Событие для журнала.
*/
struct CoreEvent {
    qint64      id = 0;             ///< номер в базе
    EventCode   code = EventCode::UnknownId;
    QDateTime   time;
    QVariantMap params;             ///< параметры: имя → значение (см. EventCode)
    bool        debug = false;      ///< отладочное (ID70–ID84) — интерфейс может скрыть
    DataSource  source = DataSource::Air;
};


/////////////////////////////////// Тревоги (Ф8) ///////////////////////////////////

/**
    @brief Вид тревоги.
*/
enum class AlarmType : quint8 {
    DoseRate,       ///< ПАЕД выше порога
    LoggerError,    ///< авария дрона (ID24, ID25, ID26, ID28)
    SdError,        ///< ошибка SD (ID16)
    LinkLost,       ///< потеря связи
    ModeMismatch    ///< режим не совпадает (ID03 на разрешённую команду)
};

/**
    @brief Уровень тревоги.
*/
enum class AlarmLevel : quint8 {
    Warning,        ///< предупреждение (жёлтый)
    Alarm           ///< тревога (красный)
};

/**
    @brief Тревога.
*/
struct Alarm {
    qint64     id = 0;              ///< номер в базе; по нему оператор отмечает «видел»
    AlarmType  type = AlarmType::DoseRate;
    AlarmLevel level = AlarmLevel::Warning;
    QDateTime  time;
    bool       hasPos = false;      ///< координаты известны
    double     lat = 0.0;
    double     lon = 0.0;
    float      value = 0.0f;        ///< для DoseRate — ПАЕД
    quint8     msgId = 0;           ///< для LoggerError — ID24 / ID25 / ID26 / ID28
    bool       acknowledged = false;    ///< оператор видел
};


/////////////////////////////////// Область разведки (Ф3) ///////////////////////////////////

/**
    @brief Ошибка проверки области.
*/
enum class AreaError : quint8 {
    None,
    TooFewVertices,     ///< меньше 3 вершин
    TooManyVertices,    ///< больше 20 вершин
    BadCoordinate,      ///< широта или долгота вне допустимого диапазона
    DuplicateVertex,    ///< повтор вершины
    Collinear,          ///< три вершины на одной прямой
    SelfIntersection,   ///< стороны пересекаются
    NotConvex,          ///< многоугольник невыпуклый — логгер будет ошибаться «внутри / снаружи»
    TooLarge,           ///< больше CoreSettings::maxAreaKm
    CrossesAntimeridian,///< пересекает 180-й меридиан
    RouteNotSupported   ///< линия (маршрут) вместо многоугольника — логгер не поддерживает
};

/**
    @brief Результат проверки области.
*/
struct AreaCheck {
    bool       ok = false;
    AreaError  error = AreaError::None;
    int        vertex = -1;         ///< номер вершины, к которой относится ошибка; -1 — ко всей области
    QVector<GeoPoint> vertices;     ///< проверенные вершины
};


/////////////////////////////////// Тепловая карта (Ф7) ///////////////////////////////////

/**
    @brief Сетка ПАЕД по области.
    @note  Клетки по строкам с юга на север, в строке — с запада на восток:
           index = row * cols + col.
*/
struct GridData {
    double     latMin = 0.0;        ///< границы сетки
    double     latMax = 0.0;
    double     lonMin = 0.0;
    double     lonMax = 0.0;
    double     cellM = 0.0;         ///< шаг клетки, м
    int        cols = 0;
    int        rows = 0;
    QVector<float>  values;         ///< значение клетки; NaN — нет данных (не ноль!)
    QVector<quint8> inArea;         ///< 1 — клетка в области, 0 — вне (не рисуется)
    QVector<int>    hotspots;       ///< индексы клеток — горячие точки
    QVector<GeoPoint> area;         ///< вершины области
};


/////////////////////////////////// Настройки ядра ///////////////////////////////////

/**
    @brief Настройки, которые ядро получает от интерфейса (DroneCore::applySettings).
    @note  Ядро файл настроек не читает. Значения по умолчанию — временные.
*/
struct CoreSettings {
    QString    portName;                ///< COM-порт радиомодема
    int        linkTimeoutS = 90;       ///< без пакетов дольше — «связь потеряна», с
    double     maxAreaKm = 50.0;        ///< предельный размер области, км
    float      warnLevel = 0.3f;        ///< порог «предупреждение», мкЗв/ч (открытый вопрос)
    float      alarmLevel = 1.0f;       ///< порог «тревога», мкЗв/ч (открытый вопрос)
    bool       correctionOn = false;    ///< пересчёт ПАЕД на 1 м включён
    QVector<double> correctionCoeffs;   ///< коэффициенты модели пересчёта (открытый вопрос)
    double     correctionMinM = 0.0;    ///< допустимые высоты пересчёта, м
    double     correctionMaxM = 0.0;
    double     gridCellM = 10.0;        ///< шаг клетки сетки, м
    double     gridRadiusM = 50.0;      ///< радиус IDW, м
    double     gridPower = 2.0;         ///< степень IDW
    QChar      csvDecimalSep = QLatin1Char(',');   ///< десятичный разделитель CSV
    QString    language = QStringLiteral("uk");    ///< язык заголовков CSV: «uk» / «en»
    QString    dbPath;                  ///< файл базы полётов
};


/////////////////////////////////// Сессии (Ф12) и архив ///////////////////////////////////

/**
    @brief Полёт (сессия) — для списка полётов.
*/
struct SessionInfo {
    qint64     id = 0;
    QString    name;                    ///< имя, которое ввёл оператор
    QDateTime  startTime;
    QDateTime  endTime;                 ///< пустое — полёт не закончен
    bool       readOnly = false;        ///< открыт для просмотра
    int        measurementCount = 0;
    bool       noSd = false;            ///< полёт без SD-карты
};

/**
    @brief Все данные полёта — при открытии прошлого полёта.
*/
struct SessionData {
    SessionInfo             info;
    QVector<RoutePoint>     route;
    QVector<Measurement>    measurements;
    QVector<Spectrum>       spectra;
    QVector<CoreEvent>      events;
    QVector<Alarm>          alarms;
    QVector<CommandOutcome> commands;
    QVector<QVector<GeoPoint>> areas;   ///< области из принятых ID04
};

/**
    @brief Типы точек архива — флаги, можно объединять через |.
*/
enum ArchivePointType : quint32 {
    ApMeasurement = 1 << 0,     ///< измерение
    ApWarn        = 1 << 1,     ///< измерение выше порога предупреждения
    ApAlarm       = 1 << 2,     ///< измерение выше порога тревоги
    ApSpectrum    = 1 << 3,     ///< точка набора спектра
    ApAll         = 0xFFFFFFFF
};

/**
    @brief Условия поиска по архиву (страница «База даних»).
    @note  Вместо готового SQL: интерфейс задаёт условия, ядро строит запрос.
*/
struct SessionFilter {
    qint64     sessionId = -1;          ///< -1 — все полёты
    QDateTime  from;                    ///< пустое — без ограничения
    QDateTime  to;                      ///< пустое — без ограничения
    QVector<GeoPoint> area;             ///< пустой — без ограничения по области
    quint32    types = ApAll;           ///< флаги ArchivePointType
};

/**
    @brief Точка архива — строка результата поиска.
*/
struct ArchivePoint {
    qint64     id = 0;
    qint64     sessionId = 0;
    QDateTime  time;
    double     lat = 0.0;
    double     lon = 0.0;
    quint32    type = ApMeasurement;    ///< ArchivePointType
    float      paed = 0.0f;
};

/**
    @brief Результат поиска по архиву.
*/
struct ArchiveResult {
    SessionFilter filter;               ///< условия, по которым искали
    QVector<ArchivePoint> points;
};


/////////////////////////////////// SD (Ф13) и экспорт (Ф14) ///////////////////////////////////

/**
    @brief Отчёт о сверке SD с эфиром.
*/
struct SdReport {
    QStringList files;              ///< обработанные файлы
    int        matched = 0;         ///< совпало с эфиром
    int        onlySd = 0;          ///< только на SD — потеряны в эфире
    int        onlyAir = 0;         ///< только в эфире — нормально, часть сообщений на SD не пишется
    int        spectraMismatch = 0; ///< итоговых спектров, не совпавших с суммой порций
    int        badLines = 0;        ///< ошибочных строк
    QStringList details;            ///< подробности расхождений
};

/**
    @brief Что выгружать в CSV — флаги, можно объединять через |.
*/
enum ExportPart : quint32 {
    ExMeasurements = 1 << 0,    ///< <полёт>_measurements.csv
    ExTrack        = 1 << 1,    ///< <полёт>_track.csv
    ExSpectra      = 1 << 2,    ///< <полёт>_spectrum_<номер>.csv
    ExEvents       = 1 << 3,    ///< <полёт>_events.csv
    ExAll          = 0x0F
};


/////////////////////////////////// Регистрация типов ///////////////////////////////////

/**
    @brief Регистрирует типы для сигналов между потоками.
    @note  Без регистрации сигнал со структурой не пройдёт из потока ядра
           в поток интерфейса. Вызывается один раз — в конструкторе DroneCore.
*/
void registerCoreTypes();

Q_DECLARE_METATYPE(GeoPoint)
Q_DECLARE_METATYPE(DataSource)
Q_DECLARE_METATYPE(Packet)
Q_DECLARE_METATYPE(LinkState)
Q_DECLARE_METATYPE(LinkStatus)
Q_DECLARE_METATYPE(LoggerMode)
Q_DECLARE_METATYPE(ReconMode)
Q_DECLARE_METATYPE(SpectrumStage)
Q_DECLARE_METATYPE(LoggerDevice)
Q_DECLARE_METATYPE(DeviceCheck)
Q_DECLARE_METATYPE(DeviceState)
Q_DECLARE_METATYPE(LoggerStatus)
Q_DECLARE_METATYPE(RoutePoint)
Q_DECLARE_METATYPE(Measurement)
Q_DECLARE_METATYPE(SpectrumStopReason)
Q_DECLARE_METATYPE(SpectrumPortion)
Q_DECLARE_METATYPE(SpectrumInfo)
Q_DECLARE_METATYPE(Spectrum)
Q_DECLARE_METATYPE(CommandRequest)
Q_DECLARE_METATYPE(CommandResult)
Q_DECLARE_METATYPE(RejectReason)
Q_DECLARE_METATYPE(CommandOutcome)
Q_DECLARE_METATYPE(EventCode)
Q_DECLARE_METATYPE(CoreEvent)
Q_DECLARE_METATYPE(AlarmType)
Q_DECLARE_METATYPE(AlarmLevel)
Q_DECLARE_METATYPE(Alarm)
Q_DECLARE_METATYPE(AreaError)
Q_DECLARE_METATYPE(AreaCheck)
Q_DECLARE_METATYPE(GridData)
Q_DECLARE_METATYPE(CoreSettings)
Q_DECLARE_METATYPE(SessionInfo)
Q_DECLARE_METATYPE(SessionData)
Q_DECLARE_METATYPE(SessionFilter)
Q_DECLARE_METATYPE(ArchivePoint)
Q_DECLARE_METATYPE(ArchiveResult)
Q_DECLARE_METATYPE(SdReport)

#endif // CORE_TYPES_H
