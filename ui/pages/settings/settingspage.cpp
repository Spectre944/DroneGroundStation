#include "settingspage.h"
#include "ui_settingspage.h"

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsPage)
{
    ui->setupUi(this);

    Settings& settings = Settings::instance();
    settings.loadSettings();

    setupUI();
    fillData();
    connections();

    // Set loaded setting after set all fields
    updateSettings();
    hideUnusedUI();
    hideHeadquartersUI();
    lockAdminFunctions();

    // Force signal to lock/unclock cript window (in contstructor it does not send)
    QTimer::singleShot(0, this, &SettingsPage::updateAdminFunctions);
    QTimer::singleShot(1500, this, &SettingsPage::forceUpdateComboBoxSettings);

}

SettingsPage::~SettingsPage()
{
    delete ui;
}

void SettingsPage::setupUI()
{
    // Настройка основных полей интерфейса
    setupInterfaceFields();
    setupGeneralFields();

    // Создание устройств с их пороговыми значениями
    setupDevices();
    setupSerialNumberFromFile();

    hideUnusedUI();

    // Построение layout
    buildLayout();
}


void SettingsPage::fillData()
{

}

void SettingsPage::connections()
{
    // Основные подключения
    connect(ui->diagnosticField, &SettingsFieldButton::buttonClicked, this, &SettingsPage::openDiagnosticWindow);
    connect(ui->pushButtonSave, &QPushButton::clicked, this, &SettingsPage::saveSettings);

    connect(ui->appTheme, &SettingsFieldCombobox::activated, this, [this](int themeId){
        Settings::instance().setValue("interface/appTheme", ui->appTheme->currentIndex());
        Settings::instance().saveSettings();
        emit styleChangeRequested(themeId);
    });
    connect(ui->fontSizeMod, &SettingsFieldCombobox::currentIndexChanged, this, [this]() {
        Settings::instance().setValue("interface/fontSize", ui->fontSizeMod->getValue());
        Settings::instance().saveSettings();
        emit styleChangeRequested(-1);
    });
    connect(ui->fontDeltaSize, &SettingsFieldCombobox::currentIndexChanged, this, [this]() {
        Settings::instance().setValue("interface/fontDelta", ui->fontDeltaSize->getValue());
        Settings::instance().saveSettings();
        emit styleChangeRequested(-1);
    });



    connect(ui->workMode, &SettingsFieldCombobox::currentIndexChanged,this, [this](int index) {
       emit settingsWorkModeChanged(static_cast<PROGRAMM_SETTINGS::ReconMode>(index));
    });

    connect(ui->audioEnable, &SettingsFieldCombobox::currentIndexChanged,this, [this](int index) {
       emit settingsAudioModeChanged(static_cast<PROGRAMM_SETTINGS::SoundMode>(index));
    });
    connect(ui->dataSaveDistance, &SettingsFieldSpinBox::valueChanged, this, &SettingsPage::settingsSaveDistanceChanged);
    connect(ui->dataSavePeriod, &SettingsFieldSpinBox::valueChanged, this, &SettingsPage::settingsSavePeriodChanged);

    connect(ui->mapSettingsField, &SettingsFieldButton::buttonClicked, this, [this](){
        if (mapSelectDialog.exec()) {
            mapFilePath.clear();
            QStringList fileNames = mapSelectDialog.selectedFiles();
            if (!fileNames.isEmpty()) {
                mapFilePath = fileNames.first();
                emit mapFileSelected(fileNames.first());
            }
        }
    });

#ifdef BRDM
    connect(ui->SVDERThreshold, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsSVDERThresholdChanged);
    connect(ui->SRDERThreshold, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsSRDERThresholdChanged);
    connect(ui->SRChemThresholdNHO, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsHNOThresholdChanged);
    connect(ui->SRChemThresholdAs, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsAsThresholdChanged);
    connect(ui->SRChemThresholdS, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsSThresholdChanged);
    connect(ui->SRChemThresholdP, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsPThresholdChanged);
    connect(ui->SRChemThresholdCH, &SettingsFieldSpinBox::valueChanged, this, &SettingsPage::settingsCHThresholdChanged);
#endif

#ifdef DZURA
    connect(ui->BDDERThreshold, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsBDBGDERThresholdChanged);
    connect(ui->MKDERThreshold, &SettingsFieldDoubleSpinBox::valueChanged, this, &SettingsPage::settingsMKSUDERThresholdChanged);
    connect(ui->MKDoseThreshold, &SettingsFieldDoubleSpinBox::valueChanged, this, [this](QVariant dose) {
        // Dose spin box in mZv, multiply by 1000 to convert in mkZv
        emit settingsMKSUDoseThresholdChanged(dose.isValid() ? (dose.toDouble() * MSV_TO_MKSV) : QVariant());
    });


#endif

}

void SettingsPage::lockAdminFunctions()
{
    ui->adminField->setEnabled(false);
    updateAdminFunctions();
}

void SettingsPage::updateAdminFunctions()
{

}

void SettingsPage::forceUpdateComboBoxSettings()
{
    // После загрузки настроект сигнал currentIndexChanged слишком рано отправляет данные в другие классы, поэтому дулируем отправку спустя 1.5 секунды по таймеру
    emit settingsAudioModeChanged(static_cast<PROGRAMM_SETTINGS::SoundMode>(ui->audioEnable->currentIndex()));
}

void SettingsPage::setupInterfaceFields()
{
    ui->fontSizeMod->setTextData("Розмір тексту", "Основний розмір тексту в програмі");
    ui->fontSizeMod->setDataRange(10, 20);
    ui->fontSizeMod->setTextUnits("px");


    ui->fontDeltaSize->setTextData("Варіація розміру тексту", "Ця змінна визначає різницю між стандартним розміром тексту і мінімальним/максимальним значенням");
    ui->fontDeltaSize->setDataRange(0, 4);
    ui->fontDeltaSize->setTextUnits("px");

    ui->appTheme->setTextData("Тема програми", "Налаштування зовнішнього вигляду інтерфейсу");
    QStringList themes;
    themes << "Системна" << "Світла" << "Windows 11";
    ui->appTheme->setComboboxData(themes);

}

void SettingsPage::setupGeneralFields()
{
    ui->diagnosticField->setTextData("Діагностика системи","Вікно для відображення стану і діагностики системи");
    ui->diagnosticField->setButtonText("Відкрити");


    ui->workMode->setTextData("Режим роботи", "");
    QStringList modes;
    modes << "Очікування"           // ReconMode::WAIT = 0
          << "Розвідка району"      // ReconMode::AREA = 1
          << "Розвідка маршруту"    // ReconMode::ROUTE = 2
          << "Обмін данними";       // ReconMode::DATA_EXCHANGE = 3
    Q_ASSERT_X(modes.size() == PROGRAMM_SETTINGS::RECON_MODE_COUNT, "setupWorkModeCombo", "Mismatch between enum and combo items count!");
    ui->workMode->setComboboxData(modes);


    ui->audioEnable->setTextData("Голосові повідомлення", "Відтворює звук загроз та іншу інформацію");
    QStringList audio;

#ifdef DZURA
    audio << "Вимкнено" << "Комп'ютер";
#endif

#ifdef BDRM
    audio << "Вимкнено" << "Комп'ютер" << "АВЗК";
#endif

    ui->audioEnable->setComboboxData(audio);


    ui->dataSaveDistance->setTextData("Період збереження даних, дистанція", "Через N метрів дані будуть автоматично збережені у БД");
    ui->dataSaveDistance->setDataRange(99, 10000);
    ui->dataSaveDistance->setTextUnits("метрів");


    ui->dataSavePeriod->setTextData("Період збереження даних, час", "Через N хвилин дані будуть автоматично збережені у БД");
    ui->dataSavePeriod->setDataRange(4, 120);
    ui->dataSavePeriod->setTextUnits("хвилин");

    ui->mapSettingsField->setTextData("Вибір карти", "Налаштування бази даних тайлів карти для відображення");
    ui->mapSettingsField->setButtonText("Відкрити");

    ui->adminField->setTextData("Права доступу", "Налаштування прав доступу та блокування функцій системи");
    ui->adminField->setButtonText("Налаштувати");

    QString programPath = QCoreApplication::applicationDirPath();
    mapSelectDialog.setDirectory(programPath);
    QStringList mapTilesFilters;
    mapTilesFilters << tr("Файли карт (*.db)") << tr("Всі файли (*)");
    mapSelectDialog.setNameFilters(mapTilesFilters);
    mapSelectDialog.setDefaultSuffix("db");
}

void SettingsPage::setupDevices()
{

#ifdef BRDM
    //------------------------------------------------------------------------------------------------------------------------------//
    svng = new SettingsDeviceItem(this, {"СВНГ-Т", TypeDev::SVNG, "2300002", PORT_NAME::SvngPort, SettingsDeviceItem::DeviceStatus::Active});
    ui->SVDERThreshold->setTextData("Поріг ПАЕД СВНГ-Т","Поріг для радіаційної загрози поза межами машини");
    ui->SVDERThreshold->setTextUnits("мкЗв/год");

    setupDeviceWithThresholds(svng, {ui->SVDERThreshold});

    //------------------------------------------------------------------------------------------------------------------------------//
    srhb = new SettingsDeviceItem(this, {"СРХБР-Т", TypeDev::SRHR,"2300001", PORT_NAME::SrhrPort,  SettingsDeviceItem::DeviceStatus::Active});

    ui->SRDERThreshold->setTextData("Поріг ПАЕД СРХБР-Т","Поріг для радіаційної загрози всередені машини");
    ui->SRDERThreshold->setTextUnits("мкЗв/год");

    ui->SRChemThresholdNHO->setTextData("Поріг NHO","Поріг для хімічної загрози, нітратна кислота");
    ui->SRChemThresholdNHO->setTextUnits("мг/м³");

    ui->SRChemThresholdAs->setTextData("Поріг As","Поріг для хімічної загрози, миш'як");
    ui->SRChemThresholdAs->setTextUnits("мг/м³");

    ui->SRChemThresholdS->setTextData("Поріг S","Поріг для хімічної загрози, сірка");
    ui->SRChemThresholdS->setTextUnits("мг/м³");

    ui->SRChemThresholdP->setTextData("Поріг P","Поріг для хімічної загрози, фосфор");
    ui->SRChemThresholdP->setTextUnits("мкг/м³");

    ui->SRChemThresholdCH->setTextData("Поріг CH","Поріг для показів вибухонебеспечних сполук, вуглеводень");
    ui->SRChemThresholdCH->setTextUnits("%");


    setupDeviceWithThresholds(srhb, {
        ui->SRDERThreshold, ui->SRChemThresholdNHO, ui->SRChemThresholdAs,
        ui->SRChemThresholdP, ui->SRChemThresholdS, ui->SRChemThresholdCH
    });
#endif


#ifdef DZURA
    //------------------------------------------------------------------------------------------------------------------------------//
    mksu = new SettingsDeviceItem(this, {"МКС-УМ", TypeDev::MKSU,"2100005", PORT_NAME::MKSUPort, SettingsDeviceItem::DeviceStatus::Active});

    ui->MKDERThreshold->setTextData("Поріг ПАЕД МКС-УМ","Поріг для радіаційної загрози всередені машини");
    ui->MKDERThreshold->setTextUnits("мкЗв/год");

    ui->MKDoseThreshold->setTextData("Поріг дози МКС-УМ", "Поріг для накопиченої радіаційногої дози");
    ui->MKDoseThreshold->setTextUnits("мЗв");

    setupDeviceWithThresholds(mksu, {ui->MKDERThreshold, ui->MKDoseThreshold});

    //------------------------------------------------------------------------------------------------------------------------------//
    bdbg = new SettingsDeviceItem(this, {"БДБГ-Т", TypeDev::BDBG, "2100002/2300003", PORT_NAME::BdbgPort, SettingsDeviceItem::DeviceStatus::Active});

    ui->BDDERThreshold->setTextData("Поріг ПАЕД БДБГ-Т","Поріг для радіаційної загрози поза межами машини");
    ui->BDDERThreshold->setTextUnits("мкЗв/год");

    setupDeviceWithThresholds(bdbg, {ui->BDDERThreshold});

#endif

}

void SettingsPage::setupSerialNumberFromFile()
{
    QString programmPath = QCoreApplication::applicationDirPath();
    QString deviceConfigPath = programmPath + "/settings/devices.json";

    QFile file(deviceConfigPath);
    if (!file.open(QIODevice::ReadOnly))
        return; // файлов нет, оставляем дефолтные серийники

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject devices = doc.object().value("devices").toObject();
    file.close();

#ifdef BRDM
    svng->setSerialNumber(devices.value("SVNG").toString());
    srhb->setSerialNumber(devices.value("SRHB").toString());
#endif

#ifdef DZURA
    mksu->setSerialNumber(devices.value("MKSU").toString());
    bdbg->setSerialNumber(devices.value("BDBG").toString());
#endif

}

void SettingsPage::buildLayout()
{
    auto layout = ui->subFrameDevices->layout();

    // Добавляем поля диагностики
    layout->addWidget(ui->diagnosticField);
    layout->addWidget(createHorizontalLine());

#ifdef BRDM
    // Добавляем устройства и их пороговые значения
    layout->addWidget(svng);
    for (auto threshold : deviceThresholds[svng].thresholds) {
        layout->addWidget(threshold);
    }
    layout->addWidget(createHorizontalLine());

    layout->addWidget(srhb);
    for (auto threshold : deviceThresholds[srhb].thresholds) {
        layout->addWidget(threshold);
    }
    layout->addWidget(createHorizontalLine());
#endif

#ifdef DZURA

    layout->addWidget(mksu);
    for (auto threshold : deviceThresholds[mksu].thresholds) {
        layout->addWidget(threshold);
    }
    layout->addWidget(createHorizontalLine());

    layout->addWidget(bdbg);
    for (auto threshold : deviceThresholds[bdbg].thresholds) {
        layout->addWidget(threshold);
    }
    layout->addWidget(createHorizontalLine());

#endif

}

void SettingsPage::hideUnusedUI()
{
    ui->workMode->setVisible(false);
    ui->line_10->setVisible(false);

#ifdef BRDM
    ui->MKDERThreshold      ->hide();
    ui->MKDoseThreshold     ->hide();
    ui->BDDERThreshold      ->hide();
#endif

#ifdef DZURA
    ui->SVDERThreshold->hide();
    ui->SRDERThreshold->hide();
    ui->SRChemThresholdNHO->hide();
    ui->SRChemThresholdAs->hide();
    ui->SRChemThresholdS->hide();
    ui->SRChemThresholdP->hide();
    ui->SRChemThresholdCH->hide();
#endif
}

void SettingsPage::hideHeadquartersUI()
{
#ifdef HQ_VERSION

    // ui->FrameProg->hide();
    ui->workMode->hide();
    ui->adminField->hide();
    ui->dataSaveDistance->hide();
    ui->dataSavePeriod->hide();
    ui->audioEnable->hide();
    ui->line_9->hide();
    ui->line_10->hide();
    ui->line_11->hide();
    ui->line_12->hide();
    ui->line_13->hide();
    ui->frameDeviceList->hide();

#endif
}

QFrame* SettingsPage::createHorizontalLine()
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setLineWidth(1);
    line->setMidLineWidth(0);
    return line;
}



void SettingsPage::saveSettings()
{
    Settings& settings = Settings::instance();

    settings.setValue("interface/fontSize",         ui->fontSizeMod->getValue());
    settings.setValue("interface/fontDelta",        ui->fontDeltaSize->getValue());
    settings.setValue("interface/appTheme",         ui->appTheme->currentIndex());

#ifdef BRDM
    settings.setValue("device/threshold/svder",     ui->SVDERThreshold->getValue());
    settings.setValue("device/threshold/srder",     ui->SRDERThreshold->getValue());
    settings.setValue("device/threshold/srhno",     ui->SRChemThresholdNHO->getValue());
    settings.setValue("device/threshold/sras",      ui->SRChemThresholdAs->getValue());
    settings.setValue("device/threshold/srs",       ui->SRChemThresholdS->getValue());
    settings.setValue("device/threshold/srp",       ui->SRChemThresholdP->getValue());
    settings.setValue("device/threshold/srch",      ui->SRChemThresholdCH->getValue());
#endif

#ifdef DZURA
    settings.setValue("device/threshold/mkder",     ui->MKDERThreshold->getValue());
    settings.setValue("device/threshold/mkdose",    ui->MKDoseThreshold->getValue());
    settings.setValue("device/threshold/bdder",     ui->BDDERThreshold->getValue());
#endif

    settings.setValue("programm/mode",              ui->workMode->getValue());
    settings.setValue("programm/audio",             ui->audioEnable->getValue());

    settings.setValue("programm/autosave/distance", ui->dataSaveDistance->getValue());
    settings.setValue("programm/autosave/time",     ui->dataSavePeriod->getValue());

    if(!mapFilePath.isEmpty())
        settings.setValue("programm/mapFilePath",       mapFilePath);

    settings.saveSettings();
    emit settingsSaved();

    NotificationManager::showInfo("Налаштування збережені");

}

void SettingsPage::updateSettings()
{
    Settings& settings = Settings::instance();

    ui->fontSizeMod         ->setValue(settings.getValue("interface/fontSize"));
    ui->fontDeltaSize       ->setValue(settings.getValue("interface/fontDelta"));
    ui->appTheme            ->setCurrentIndex(settings.getValue("interface/appTheme").toInt());

#ifdef BRDM
    // Временное решение, для того что бы пофиксить установку нулевых порогов (выключено) после загрузки, без этого сигнал о изменении на 0 не эмитируется при загрузке
    ui->SVDERThreshold      ->loadValue(settings.getValue("device/threshold/svder"));
    ui->SRDERThreshold      ->loadValue(settings.getValue("device/threshold/srder"));
    ui->SRChemThresholdNHO  ->loadValue(settings.getValue("device/threshold/srhno"));
    ui->SRChemThresholdAs   ->loadValue(settings.getValue("device/threshold/sras"));
    ui->SRChemThresholdS    ->loadValue(settings.getValue("device/threshold/srs"));
    ui->SRChemThresholdP    ->loadValue(settings.getValue("device/threshold/srp"));
    ui->SRChemThresholdCH   ->loadValue(settings.getValue("device/threshold/srch"));
#endif

#ifdef DZURA
    ui->MKDERThreshold      ->loadValue(settings.getValue("device/threshold/mkder"));
    ui->MKDoseThreshold     ->loadValue(settings.getValue("device/threshold/mkdose"));
    ui->BDDERThreshold      ->loadValue(settings.getValue("device/threshold/bdder"));
#endif

    ui->workMode            ->setValue(settings.getValue("programm/mode"));
    ui->audioEnable         ->setValue(settings.getValue("programm/audio"));

    ui->dataSaveDistance    ->loadValue(settings.getValue("programm/autosave/distance"));
    ui->dataSavePeriod      ->loadValue(settings.getValue("programm/autosave/time"));
}

void SettingsPage::onAirmarTestDataGenerated(const airmarTest &testData)
{

}

void SettingsPage::onGpsTestDataGenerated(const gpsTest &testData)
{

}

void SettingsPage::onLcdTestDataGenerated(const lcdTest &testData)
{

}

void SettingsPage::onBdbgTestDataGenerated(const bdbgTest &testData)
{

}

void SettingsPage::onSvngTestDataGenerated(const svngTest &testData)
{

}

void SettingsPage::onSrhbTestDataGenerated(const srhrTest &testData)
{

}

void SettingsPage::onMksuTestDataGenerated(const mksuTest &testData)
{

}

void SettingsPage::onDeviceRemovedFromPoll(TypeDev device)
{

}

void SettingsPage::onUpdateDiagnosticInfoDzura(const lcdTest &lcdTestData, const mksuTest &mksuTestData, const bdbgTest &bdbgTestData, const airmarTest &airmarTestData, const gpsTest &gpsTestData)
{
    // Update diagnostic data on setting page
    onAirmarTestDataGenerated(airmarTestData);
    onGpsTestDataGenerated(gpsTestData);
    onLcdTestDataGenerated(lcdTestData);
    onBdbgTestDataGenerated(bdbgTestData);
    onMksuTestDataGenerated(mksuTestData);
}

void SettingsPage::onUpdateDiagnosticInfoBrdm(const lcdTest &lcdTestData, const svngTest &svngTestData, const srhrTest &srhbTestData, const airmarTest &airmarTestData)
{
    // Update diagnostic data on setting page
    onAirmarTestDataGenerated(airmarTestData);
    onLcdTestDataGenerated(lcdTestData);
    onSvngTestDataGenerated(svngTestData);
    onSrhbTestDataGenerated(srhbTestData);

}

void SettingsPage::onCommanderKeyInserted(bool status, QString message)
{

}


