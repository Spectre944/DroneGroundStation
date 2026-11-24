#include "settingspage.h"
#include "ui_settingspage.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPair>
#include <QVBoxLayout>

#include "ui/modules/preflight/preflightchecklist.h"
#include "ui/modules/settingsfield/devicedetailswindow.h"

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsPage)
{
    ui->setupUi(this);

    Settings::instance().loadSettings();

    setupInterfaceFields();
    setupDeviceFields();
    setupReconFields();
    setupGridFields();
    setupProgramFields();

    connections();
    // Загрузка после connections(): как и раньше, значения из файла
    // расходятся сигналами по остальным классам
    updateSettings();
}

SettingsPage::~SettingsPage()
{
    delete ui;
}

// ---------------------------------------------------------------------------
//  Інтерфейс
// ---------------------------------------------------------------------------

void SettingsPage::setupInterfaceFields()
{
    ui->fontSizeMod->setTextData("Розмір тексту", "Основний розмір тексту в програмі");
    ui->fontSizeMod->setDataRange(10, 20);
    ui->fontSizeMod->setTextUnits("px");

    ui->fontDeltaSize->setTextData("Варіація розміру тексту",
                                   "Різниця між стандартним розміром тексту і мінімальним/максимальним значенням");
    ui->fontDeltaSize->setDataRange(0, 4);
    ui->fontDeltaSize->setTextUnits("px");

    ui->appTheme->setTextData("Тема програми", "Налаштування зовнішнього вигляду інтерфейсу");
    ui->appTheme->setComboboxData(QStringList() << "Системна" << "Світла" << "Windows 11");
}

// ---------------------------------------------------------------------------
//  Прилади
// ---------------------------------------------------------------------------

void SettingsPage::setupDeviceFields()
{
    auto *layout = ui->subFrameDevices->layout();

    // Первая строка блока — окно передполётной проверки (как «Діагностика» в РХБ)
    m_preflightField = new SettingsFieldButton(ui->subFrameDevices);
    m_preflightField->setTextData("Передпольотна перевірка",
                                  "Стан приладів логера і запуск перевірки (ID01)");
    m_preflightField->setButtonText("Відкрити");
    layout->addWidget(m_preflightField);
    layout->addWidget(createHorizontalLine());

    m_preflight = new PreflightChecklist(this);
    m_preflight->setWindowFlag(Qt::Window);     // отдельное окно, а не часть страницы
    m_preflight->hide();

    // Окно прибора (кнопка настроек в строке): сведения, для радиомодема — выбор порта
    m_details = new DeviceDetailsWindow(this);
    m_details->hide();

    // Приборы ДРОН-1. Радиомодем — на наземной станции, остальные — на борту
    // и опрашиваются логгером, поэтому вместо порта — «логер».
    using DS = SettingsDeviceItem::DeviceStatus;
    const QString onBoard = QStringLiteral("логер (радіоканал)");
    addDevice({"Радіомодем",        TypeDev::RADIO,   "-", "—",     DS::Disconnected});
    addDevice({"GPS",               TypeDev::GPS,     "-", onBoard, DS::Disconnected});
    addDevice({"LIDAR",             TypeDev::LIDAR,   "-", onBoard, DS::Disconnected});
    SettingsDeviceItem *bdbg = addDevice({"БДБГ-15", TypeDev::BDBG, "-", onBoard, DS::Disconnected});

    // Пороги ПАЕД — поля под строкой БДБГ-15, раскрываются кнопкой «пороги» (как в РХБ)
    m_warnLevel = new SettingsFieldDoubleSpinBox(ui->subFrameDevices);
    m_warnLevel->setTextData("Поріг ПАЕД «попередження»",
                             "Вимірювання вище — жовті на карті, попередження в журналі");
    m_warnLevel->setTextUnits("мкЗв/год");
    m_warnLevel->setDataRange(0.01, 10000.0);

    m_alarmLevel = new SettingsFieldDoubleSpinBox(ui->subFrameDevices);
    m_alarmLevel->setTextData("Поріг ПАЕД «тривога»",
                              "Вимірювання вище — тривога (шторка, звук, червоні на карті)");
    m_alarmLevel->setTextUnits("мкЗв/год");
    m_alarmLevel->setDataRange(0.01, 10000.0);

    layout->addWidget(m_warnLevel);
    layout->addWidget(m_alarmLevel);
    setupDeviceThresholds(bdbg, {m_warnLevel, m_alarmLevel});

    addDevice({"Акумулятор логера", TypeDev::BATTERY, "-", onBoard, DS::Disconnected});
    addDevice({"SD-карта",          TypeDev::SDCARD,  "-", onBoard, DS::Disconnected});

    // У остальных приборов порогов нет — кнопка неактивна
    for (SettingsDeviceItem *item : std::as_const(m_devices))
        if (!m_thresholds.contains(item))
            item->setThresholdBtnActive(false);

    loadSerialNumbersFromFile();
}

void SettingsPage::setupDeviceThresholds(SettingsDeviceItem *device, const QList<QWidget *> &fields)
{
    if (!device)
        return;
    m_thresholds.insert(device, fields);
    device->setThresholdBtnActive(!fields.isEmpty());
    for (QWidget *w : fields)
        w->setVisible(false);       // изначально свёрнуты
}

void SettingsPage::toggleDeviceThresholds(SettingsDeviceItem *device)
{
    const QList<QWidget *> fields = m_thresholds.value(device);
    if (fields.isEmpty())
        return;
    const bool show = !fields.first()->isVisible();
    for (QWidget *w : fields)
        w->setVisible(show);
}

// ---------------------------------------------------------------------------
//  Состояние приборов от DroneCore
// ---------------------------------------------------------------------------

void SettingsPage::onLinkStatus(const LinkStatus &status)
{
    m_details->onLinkStatus(status);

    SettingsDeviceItem *radio = deviceItem(TypeDev::RADIO);
    if (!radio)
        return;

    const QString port = status.portName == QLatin1String(DeviceDetailsWindow::STUB_PORT)
                       ? QStringLiteral("заглушка (тест)") : status.portName;
    radio->setDevicePort(port.isEmpty() ? QStringLiteral("—") : port);
    using DS = SettingsDeviceItem::DeviceStatus;
    switch (status.state) {
    case LinkState::Closed:  radio->setDeviceStatus(DS::Disconnected, true); break;
    case LinkState::Waiting: radio->setDeviceStatus(DS::Warning, true);      break;   // порт открыт, логгер молчит
    case LinkState::Ok:
    case LinkState::Silent:  radio->setDeviceStatus(DS::Active, true);       break;
    case LinkState::Lost:    radio->setDeviceStatus(DS::Inactive, true);     break;
    }
    radio->setLastUpdateTime(status.secondsSinceLastPacket);   // -1 — пакетов ещё не было
}

void SettingsPage::onLoggerStatus(const LoggerStatus &status)
{
    static const QPair<LoggerDevice, TypeDev> map[] = {
        {LoggerDevice::Gps,     TypeDev::GPS},
        {LoggerDevice::Lidar,   TypeDev::LIDAR},
        {LoggerDevice::Bdbg,    TypeDev::BDBG},
        {LoggerDevice::Battery, TypeDev::BATTERY},
        {LoggerDevice::Sd,      TypeDev::SDCARD},
    };
    using DS = SettingsDeviceItem::DeviceStatus;

    for (const auto &m : map) {
        SettingsDeviceItem *item = deviceItem(m.second);
        if (!item)
            continue;
        const DeviceState now  = status.devices.value(int(m.first));
        const DeviceState prev = m_prevLogger.devices.value(int(m.first));

        // Новое сообщение прибора (проверка: ID05, ID06, ID18 …) — прибор на связи
        if (now.msgId != 0 && now.msgId != prev.msgId)
            item->markAlive();

        switch (now.check) {
        case DeviceCheck::Ok:   item->setDeviceStatus(DS::Active, true);   break;
        case DeviceCheck::Fail: item->setDeviceStatus(DS::Inactive, true); break;
        case DeviceCheck::NotChecked:
            // Не проверялся, но данные от него уже приходили — работает
            item->setDeviceStatus(item->data().lastUpdate >= 0 ? DS::Active : DS::Disconnected, true);
            break;
        }
    }
    if (status.noSd)
        if (SettingsDeviceItem *sd = deviceItem(TypeDev::SDCARD))
            sd->setDeviceStatus(DS::Warning, true);

    // Новое напряжение (ID04, ID05, ID22) — аккумулятор на связи
    if (status.hasBattV && (!m_prevLogger.hasBattV || status.battV != m_prevLogger.battV))
        touchDevice(TypeDev::BATTERY);

    m_prevLogger = status;
    m_preflight->onLoggerStatus(status);
}

void SettingsPage::onRoutePoint(const RoutePoint &point)
{
    if (point.source == DataSource::Air)
        touchDevice(TypeDev::GPS);          // ID19 / ID20 / ID22 — координата от GPS
    m_preflight->onRoutePoint(point);
}

void SettingsPage::onMeasurement(const Measurement &m)
{
    if (m.source != DataSource::Air)        // импорт SD — не «живые» пакеты
        return;
    touchDevice(TypeDev::BDBG);             // ПАЕД
    if (m.lat != 0.0 || m.lon != 0.0)
        touchDevice(TypeDev::GPS);
    if (m.lidarCm > 0 || m.lidarStrength > 0)
        touchDevice(TypeDev::LIDAR);
    if (m.battV > 0.0f)
        touchDevice(TypeDev::BATTERY);
}

void SettingsPage::onSpectrum(const Spectrum &spectrum)
{
    if (spectrum.info.source == DataSource::Air)
        touchDevice(TypeDev::BDBG);         // порция спектра (ID23)
}

void SettingsPage::touchDevice(TypeDev type)
{
    if (SettingsDeviceItem *item = deviceItem(type))
        item->markAlive();
}

void SettingsPage::onAllowedCommands(const QList<quint8> &ids)
{
    m_preflight->onAllowedCommands(ids);
}

void SettingsPage::openPreflightWindow()
{
    m_preflight->show();
    m_preflight->raise();
    m_preflight->activateWindow();
}

SettingsDeviceItem *SettingsPage::addDevice(const SettingsDeviceItem::DeviceData &data)
{
    if (SettingsDeviceItem *existing = m_devices.value(data.devType, nullptr))
        return existing;

    auto *layout = ui->subFrameDevices->layout();
    if (!m_devices.isEmpty())
        layout->addWidget(createHorizontalLine());

    auto *item = new SettingsDeviceItem(ui->subFrameDevices, data);
    layout->addWidget(item);
    m_devices.insert(data.devType, item);

    connect(item, &SettingsDeviceItem::additionalSettingsRequested, this, [this, item] {
        m_details->openFor(item);
    });
    connect(item, &SettingsDeviceItem::thredholdWindowRequested, this, [this, item] {
        toggleDeviceThresholds(item);
    });
    return item;
}

QString SettingsPage::deviceKey(TypeDev type)
{
    switch (type) {
    case TypeDev::AIRMAR: return "AIRMAR";
    case TypeDev::GPS:    return "GPS";
    case TypeDev::LCD:    return "LCD";
    case TypeDev::BDBG:   return "BDBG";
    case TypeDev::SVNG:   return "SVNG";
    case TypeDev::SRHR:   return "SRHB";   // так было в старом devices.json
    case TypeDev::MKSU:   return "MKSU";
    case TypeDev::RADIO:   return "RADIO";
    case TypeDev::LIDAR:   return "LIDAR";
    case TypeDev::BATTERY: return "BATTERY";
    case TypeDev::SDCARD:  return "SD";
    }
    return QString();
}

// Серийные номера из <папка программы>/settings/devices.json:
//   { "devices": { "GPS": "12345", "MKSU": "2100005", ... } }
void SettingsPage::loadSerialNumbersFromFile()
{
    QFile file(QCoreApplication::applicationDirPath() + "/settings/devices.json");
    if (!file.open(QIODevice::ReadOnly))
        return;   // файла нет - остаются серийники по умолчанию

    const QJsonObject devices = QJsonDocument::fromJson(file.readAll()).object().value("devices").toObject();
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        const QString serial = devices.value(deviceKey(it.key())).toString();
        if (!serial.isEmpty())
            it.value()->setSerialNumber(serial);
    }
}

// ---------------------------------------------------------------------------
//  Порт радиомодема (Р2-1)
// ---------------------------------------------------------------------------

QString SettingsPage::savedPort() const
{
    return Settings::instance().getValue(KEY_PORT, QString()).toString();
}

void SettingsPage::setPortProvider(std::function<QStringList()> provider)
{
    m_details->setPortProvider(std::move(provider));
}

// ---------------------------------------------------------------------------
//  Розвідка: типовые параметры ID04
// ---------------------------------------------------------------------------

QVBoxLayout *SettingsPage::addSection(const QString &title)
{
    // Как разделы из .ui: жирный заголовок + рамка StyledPanel
    auto *frame = new QFrame(ui->frameSettings);
    frame->setFrameShape(QFrame::NoFrame);
    auto *outer = new QVBoxLayout(frame);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);

    auto *label = new QLabel(title, frame);
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
    outer->addWidget(label);

    auto *sub = new QFrame(frame);
    sub->setFrameShape(QFrame::StyledPanel);
    auto *inner = new QVBoxLayout(sub);
    outer->addWidget(sub);

    auto *pageLayout = qobject_cast<QVBoxLayout *>(ui->frameSettings->layout());
    const int at = pageLayout ? pageLayout->indexOf(ui->FrameProg) : -1;
    if (pageLayout && at >= 0)
        pageLayout->insertWidget(at, frame);
    else
        ui->frameSettings->layout()->addWidget(frame);
    return inner;
}

void SettingsPage::setupReconFields()
{
    QVBoxLayout *l = addSection("Розвідка — типові параметри (ID04)");

    m_reconMode = new SettingsFieldCombobox(this);
    m_reconMode->setTextData("Режим розвідки", "Коли логер виконує вимірювання");
    m_reconMode->setComboboxData({"За командою оператора", "За часом", "За відстанню"});

    m_interval = new SettingsFieldSpinBox(this);
    m_interval->setTextData("Інтервал між вимірюваннями", "Режим «за часом»: вимірювання через N секунд");
    m_interval->setTextUnits("с");
    m_interval->setDataRange(ReconParams::INTERVAL_MIN, ReconParams::INTERVAL_MAX);

    m_distance = new SettingsFieldSpinBox(this);
    m_distance->setTextData("Відстань між вимірюваннями", "Режим «за відстанню»: вимірювання через N метрів");
    m_distance->setTextUnits("м");
    m_distance->setDataRange(ReconParams::DISTANCE_MIN, ReconParams::DISTANCE_MAX);

    m_spectrum = new SettingsFieldSpinBox(this);
    m_spectrum->setTextData("Час набору спектра", "0 — набір до команди «Зупинити набір» (ID12)");
    m_spectrum->setTextUnits("с");
    m_spectrum->setDataRange(ReconParams::SPECTRUM_MIN, ReconParams::SPECTRUM_MAX);
    m_spectrum->setSpecialValueText("до команди ID12");     // 0 — не «вимкнено», а «до зупинки»

    m_dup = new SettingsFieldCombobox(this);
    m_dup->setTextData("Коефіцієнт дублювання пакетів",
                       "Скільки разів логер повторює кожен пакет (надійніше, але повільніше)");
    m_dup->setDataRange(ReconParams::DUP_MIN, ReconParams::DUP_MAX);

    const QList<QWidget *> fields = {m_reconMode, m_interval, m_distance, m_spectrum, m_dup};
    for (int i = 0; i < fields.size(); ++i) {
        if (i > 0)
            l->addWidget(createHorizontalLine());
        l->addWidget(fields[i]);
    }
}

// ---------------------------------------------------------------------------
//  Теплова карта (сетка ядра DoseGrid)
// ---------------------------------------------------------------------------

void SettingsPage::setupGridFields()
{
    QVBoxLayout *l = addSection("Теплова карта");

    m_gridCell = new SettingsFieldSpinBox(this);
    m_gridCell->setTextData("Розмір клітинки", "Крок сітки теплової карти");
    m_gridCell->setTextUnits("м");
    m_gridCell->setDataRange(1, 1000);

    m_gridRadius = new SettingsFieldSpinBox(this);
    m_gridRadius->setTextData("Радіус інтерполяції",
                              "Клітинки далі від найближчої точки — «немає даних». Має бути більше "
                              "відстані між точками (≈100 м при пакетах раз на 20 с), краще в 1,5 раза");
    m_gridRadius->setTextUnits("м");
    m_gridRadius->setDataRange(10, 5000);

    m_gridPower = new SettingsFieldDoubleSpinBox(this);
    m_gridPower->setTextData("Степінь IDW", "Більше — вплив найближчих точок сильніший");
    m_gridPower->setDataRange(0.5, 5.0);

    l->addWidget(m_gridCell);
    l->addWidget(createHorizontalLine());
    l->addWidget(m_gridRadius);
    l->addWidget(createHorizontalLine());
    l->addWidget(m_gridPower);
}

// ---------------------------------------------------------------------------
//  Програма
// ---------------------------------------------------------------------------

void SettingsPage::setupProgramFields()
{
    ui->mapSettingsField->setButtonText("Відкрити");
    updateMapFieldText();
}

void SettingsPage::updateMapFieldText()
{
    const QString subtext = m_mapFilePath.isEmpty()
        ? QStringLiteral("Файл бази тайлів карти не вибрано")
        : QDir::toNativeSeparators(m_mapFilePath);
    ui->mapSettingsField->setTextData("Шлях до карт", subtext);
    ui->mapSettingsField->setTooltip(subtext);
}

void SettingsPage::selectMapFile()
{
    const QString startDir = m_mapFilePath.isEmpty() ? QCoreApplication::applicationDirPath()
                                                     : QFileInfo(m_mapFilePath).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, "Вибір файлу карт", startDir,
                                                      "Файли карт (*.db *.mbtiles);;Всі файли (*)");
    if (path.isEmpty())
        return;

    m_mapFilePath = path;
    updateMapFieldText();

    // Путь к карте применяем и сохраняем сразу, без кнопки "Зберегти"
    Settings::instance().setValue(KEY_MAP_FILE, m_mapFilePath);
    Settings::instance().saveSettings();

    emit mapFileSelected(m_mapFilePath);
}

// ---------------------------------------------------------------------------

void SettingsPage::connections()
{
    connect(ui->pushButtonSave, &QPushButton::clicked, this, &SettingsPage::saveSettings);

    connect(ui->appTheme, &SettingsFieldCombobox::activated, this, [this](int themeId) {
        Settings::instance().setValue(KEY_APP_THEME, ui->appTheme->currentIndex());
        Settings::instance().saveSettings();
        emit styleChangeRequested(themeId);
    });
    connect(ui->fontSizeMod, &SettingsFieldCombobox::currentIndexChanged, this, [this]() {
        Settings::instance().setValue(KEY_FONT_SIZE, ui->fontSizeMod->getValue());
        Settings::instance().saveSettings();
        emit styleChangeRequested(-1);
    });
    connect(ui->fontDeltaSize, &SettingsFieldCombobox::currentIndexChanged, this, [this]() {
        Settings::instance().setValue(KEY_FONT_DELTA, ui->fontDeltaSize->getValue());
        Settings::instance().saveSettings();
        emit styleChangeRequested(-1);
    });

    connect(ui->mapSettingsField, &SettingsFieldButton::buttonClicked, this, &SettingsPage::selectMapFile);

    connect(m_preflightField, &SettingsFieldButton::buttonClicked, this, &SettingsPage::openPreflightWindow);
    connect(m_preflight, &PreflightChecklist::commandRequested, this, &SettingsPage::commandRequested);

    // Порт радиомодема: выбор запоминаем сразу — следующий запуск подключится к нему
    connect(m_details, &DeviceDetailsWindow::connectRequested, this, [this](const QString &port) {
        Settings::instance().setValue(KEY_PORT, port);
        Settings::instance().saveSettings();
        m_details->setSavedPort(port);
        emit portOpenRequested(port);
    });
    connect(m_details, &DeviceDetailsWindow::disconnectRequested, this, &SettingsPage::portCloseRequested);
}

QFrame *SettingsPage::createHorizontalLine()
{
    auto *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

// ---------------------------------------------------------------------------
//  Настройки ядра
// ---------------------------------------------------------------------------

CoreSettings SettingsPage::coreSettings() const
{
    const Settings &s = Settings::instance();
    CoreSettings c;     // значения по умолчанию — из CoreSettings
    c.portName    = savedPort();
    c.linkTimeoutS = s.getValue(KEY_LINK_TIMEOUT, c.linkTimeoutS).toInt();
    c.maxAreaKm   = s.getValue(KEY_MAX_AREA, c.maxAreaKm).toDouble();
    c.warnLevel   = float(s.getValue(KEY_WARN,  double(c.warnLevel)).toDouble());
    c.alarmLevel  = float(s.getValue(KEY_ALARM, double(c.alarmLevel)).toDouble());
    c.gridCellM   = s.getValue(KEY_GRID_CELL, c.gridCellM).toDouble();
    c.gridRadiusM = s.getValue(KEY_GRID_RADIUS, DEFAULT_GRID_RADIUS_M).toDouble();
    c.gridPower   = s.getValue(KEY_GRID_POWER, c.gridPower).toDouble();
    return c;
}

void SettingsPage::saveSettings()
{
    // Пороги: «попередження» должен быть ниже «тривоги»
    const double warn  = m_warnLevel->getValue().toDouble();
    const double alarm = m_alarmLevel->getValue().toDouble();
    if (!(warn < alarm)) {
        NotificationManager::showError("Поріг «попередження» має бути меншим за поріг «тривога»",
                                       "Налаштування не збережені");
        for (QWidget *w : m_thresholds.value(deviceItem(TypeDev::BDBG)))
            w->setVisible(true);                                // раскрыть, чтобы было видно поля
        return;
    }

    Settings &settings = Settings::instance();

    settings.setValue(KEY_FONT_SIZE,     ui->fontSizeMod->getValue());
    settings.setValue(KEY_FONT_DELTA,    ui->fontDeltaSize->getValue());
    settings.setValue(KEY_APP_THEME,     ui->appTheme->currentIndex());

    if (!m_mapFilePath.isEmpty())
        settings.setValue(KEY_MAP_FILE, m_mapFilePath);

    // Ядро
    settings.setValue(KEY_WARN,        warn);
    settings.setValue(KEY_ALARM,       alarm);
    settings.setValue(KEY_GRID_CELL,   m_gridCell->getValue().toInt());
    settings.setValue(KEY_GRID_RADIUS, m_gridRadius->getValue().toInt());
    settings.setValue(KEY_GRID_POWER,  m_gridPower->getValue().toDouble());

    // Типовые параметры ID04
    ReconParams p;
    p.reconMode = ReconMode(qBound(0, m_reconMode->currentIndex(), 2));
    p.intervalS = m_interval->getValue().toInt();
    p.distanceM = m_distance->getValue().toInt();
    p.spectrumS = m_spectrum->getValue().toInt();
    p.dupFactor = ReconParams::DUP_MIN + qMax(0, m_dup->currentIndex());
    p.store();

    settings.saveSettings();
    emit settingsSaved();
    emit coreSettingsChanged(coreSettings());
    emit reconDefaultsChanged(p);

    NotificationManager::showInfo("Налаштування збережені");
}

void SettingsPage::updateSettings()
{
    Settings &settings = Settings::instance();

    ui->fontSizeMod     ->setValue(settings.getValue(KEY_FONT_SIZE));
    ui->fontDeltaSize   ->setValue(settings.getValue(KEY_FONT_DELTA));
    ui->appTheme        ->setCurrentIndex(settings.getValue(KEY_APP_THEME).toInt());

    m_mapFilePath = settings.getValue(KEY_MAP_FILE).toString();
    updateMapFieldText();

    const CoreSettings c = coreSettings();
    m_warnLevel->setValue(c.warnLevel);
    m_alarmLevel->setValue(c.alarmLevel);
    m_gridCell->setValue(int(c.gridCellM));
    m_gridRadius->setValue(int(c.gridRadiusM));
    m_gridPower->setValue(c.gridPower);

    const ReconParams p = ReconParams::load();
    m_reconMode->setCurrentIndex(int(p.reconMode));
    m_interval->setValue(p.intervalS);
    m_distance->setValue(p.distanceM);
    m_spectrum->setValue(p.spectrumS);
    m_dup->setCurrentIndex(p.dupFactor - ReconParams::DUP_MIN);

    m_details->setSavedPort(savedPort());
}
