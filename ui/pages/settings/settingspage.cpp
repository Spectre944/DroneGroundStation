#include "settingspage.h"
#include "ui_settingspage.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>

#include "ui/modules/preflight/preflightchecklist.h"

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsPage)
{
    ui->setupUi(this);

    Settings::instance().loadSettings();

    setupInterfaceFields();
    setupDeviceFields();
    setupProgramFields();

    connections();
    // Загрузка после connections(): как и раньше, значения из файла
    // расходятся сигналами (settingsSave*Changed) по остальным классам
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

    // Приборы ДРОН-1. Радиомодем — на наземной станции, остальные — на борту
    // и опрашиваются логгером, поэтому вместо порта — «логер».
    using DS = SettingsDeviceItem::DeviceStatus;
    const QString onBoard = QStringLiteral("логер (радіоканал)");
    addDevice({"Радіомодем",        TypeDev::RADIO,   "-", "—",     DS::Disconnected});
    addDevice({"GPS",               TypeDev::GPS,     "-", onBoard, DS::Disconnected});
    addDevice({"LIDAR",             TypeDev::LIDAR,   "-", onBoard, DS::Disconnected});
    addDevice({"БДБГ-15",           TypeDev::BDBG,    "-", onBoard, DS::Disconnected});
    addDevice({"Акумулятор логера", TypeDev::BATTERY, "-", onBoard, DS::Disconnected});
    addDevice({"SD-карта",          TypeDev::SDCARD,  "-", onBoard, DS::Disconnected});

    loadSerialNumbersFromFile();
}

// ---------------------------------------------------------------------------
//  Состояние приборов от DroneCore
// ---------------------------------------------------------------------------

void SettingsPage::onLinkStatus(const LinkStatus &status)
{
    SettingsDeviceItem *radio = deviceItem(TypeDev::RADIO);
    if (!radio)
        return;

    radio->setDevicePort(status.portName.isEmpty() ? QStringLiteral("—") : status.portName);
    using DS = SettingsDeviceItem::DeviceStatus;
    switch (status.state) {
    case LinkState::Closed:  radio->setDeviceStatus(DS::Disconnected, true); break;
    case LinkState::Waiting: radio->setDeviceStatus(DS::Warning, true);      break;   // порт открыт, логгер молчит
    case LinkState::Ok:
    case LinkState::Silent:  radio->setDeviceStatus(DS::Active, true);       break;
    case LinkState::Lost:    radio->setDeviceStatus(DS::Inactive, true);     break;
    }
    if (status.secondsSinceLastPacket >= 0)
        radio->setLastUpdateTime(status.secondsSinceLastPacket);
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
        switch (status.devices.value(int(m.first)).check) {
        case DeviceCheck::Ok:         item->setDeviceStatus(DS::Active, true);       break;
        case DeviceCheck::Fail:       item->setDeviceStatus(DS::Inactive, true);     break;
        case DeviceCheck::NotChecked: item->setDeviceStatus(DS::Disconnected, true); break;
        }
    }
    if (status.noSd)
        if (SettingsDeviceItem *sd = deviceItem(TypeDev::SDCARD))
            sd->setDeviceStatus(DS::Warning, true);

    m_preflight->onLoggerStatus(status);
}

void SettingsPage::onRoutePoint(const RoutePoint &point)
{
    m_preflight->onRoutePoint(point);
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
//  Програма
// ---------------------------------------------------------------------------

void SettingsPage::setupProgramFields()
{
    ui->dataSaveDistance->setTextData("Період збереження даних, дистанція",
                                      "Через N метрів дані будуть автоматично збережені у БД");
    ui->dataSaveDistance->setDataRange(99, 10000);
    ui->dataSaveDistance->setTextUnits("метрів");

    ui->dataSavePeriod->setTextData("Період збереження даних, час",
                                    "Через N хвилин дані будуть автоматично збережені у БД");
    ui->dataSavePeriod->setDataRange(4, 120);
    ui->dataSavePeriod->setTextUnits("хвилин");

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

    connect(ui->dataSaveDistance, &SettingsFieldSpinBox::valueChanged, this, &SettingsPage::settingsSaveDistanceChanged);
    connect(ui->dataSavePeriod,   &SettingsFieldSpinBox::valueChanged, this, &SettingsPage::settingsSavePeriodChanged);

    connect(ui->mapSettingsField, &SettingsFieldButton::buttonClicked, this, &SettingsPage::selectMapFile);

    connect(m_preflightField, &SettingsFieldButton::buttonClicked, this, &SettingsPage::openPreflightWindow);
    connect(m_preflight, &PreflightChecklist::commandRequested, this, &SettingsPage::commandRequested);
}

QFrame *SettingsPage::createHorizontalLine()
{
    auto *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

void SettingsPage::saveSettings()
{
    Settings &settings = Settings::instance();

    settings.setValue(KEY_FONT_SIZE,     ui->fontSizeMod->getValue());
    settings.setValue(KEY_FONT_DELTA,    ui->fontDeltaSize->getValue());
    settings.setValue(KEY_APP_THEME,     ui->appTheme->currentIndex());

    settings.setValue(KEY_SAVE_DISTANCE, ui->dataSaveDistance->getValue());
    settings.setValue(KEY_SAVE_TIME,     ui->dataSavePeriod->getValue());

    if (!m_mapFilePath.isEmpty())
        settings.setValue(KEY_MAP_FILE, m_mapFilePath);

    settings.saveSettings();
    emit settingsSaved();

    NotificationManager::showInfo("Налаштування збережені");
}

void SettingsPage::updateSettings()
{
    Settings &settings = Settings::instance();

    ui->fontSizeMod     ->setValue(settings.getValue(KEY_FONT_SIZE));
    ui->fontDeltaSize   ->setValue(settings.getValue(KEY_FONT_DELTA));
    ui->appTheme        ->setCurrentIndex(settings.getValue(KEY_APP_THEME).toInt());

    ui->dataSaveDistance->loadValue(settings.getValue(KEY_SAVE_DISTANCE));
    ui->dataSavePeriod  ->loadValue(settings.getValue(KEY_SAVE_TIME));

    m_mapFilePath = settings.getValue(KEY_MAP_FILE).toString();
    updateMapFieldText();
}
