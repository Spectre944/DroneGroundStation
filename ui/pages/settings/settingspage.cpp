#include "settingspage.h"
#include "ui_settingspage.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

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
    // Шаблон: один прибор. Остальные добавляются так же - addDevice({...}).
    addDevice({"GPS", TypeDev::GPS, "-", PORT_NAME::GpsPort,
               SettingsDeviceItem::DeviceStatus::Inactive});

    loadSerialNumbersFromFile();
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
