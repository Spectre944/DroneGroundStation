#include "reconpage.h"
#include "ui_reconpage.h"

ReconPage::ReconPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ReconPage)
{
    ui->setupUi(this);

    setupUI();
    fillData();
    connections();
    hideUnusedUI();

}

ReconPage::~ReconPage()
{
    delete ui;
}

void ReconPage::collectReconDataFromPage()
{
    startReconParams reconData;
    int reconDurationSeconds = ui->spinBoxReconDuration->value() * 3600;

    reconData.recon_mode = getReconModeFromComboBox(ui->comboBoxReconType);
    reconData.duration_reconn = reconDurationSeconds;
    reconData.req_dev_mode = getDeviceModeFromComboBox(ui->comboBoxWorkMode);
    reconData.coord_package = ui->textEditCoordinates->getCoordinateString();
    reconData.isCoordinatesValid = ui->textEditCoordinates->isCoordinatesValid();
    reconData.coordinatesCount = ui->textEditCoordinates->getCoordinatesCount();
    reconData.release_zone = ui->spinBoxReleaseRadius->value();
    reconData.danger_distance = ui->spinBoxDangerDistance->value();
    reconData.attack_status = getAtackFromComboBox(ui->comboBoxAttackStatus);

    emit reconDataGenerated(reconData);
}

PROGRAMM_SETTINGS::ReconMode ReconPage::getReconModeFromComboBox(QComboBox *comboBoxReconMode)
{
    if(!comboBoxReconMode)
        return PROGRAMM_SETTINGS::ReconMode::WAIT;

    return static_cast<PROGRAMM_SETTINGS::ReconMode>(comboBoxReconMode->currentIndex() + 1);
}

reqDevMode ReconPage::getDeviceModeFromComboBox(QComboBox *comboBoxDevMode)
{
    if(!comboBoxDevMode)
        return reqDevMode::Auto;

    return static_cast<reqDevMode>(comboBoxDevMode->currentIndex());
}

attackStatus ReconPage::getAtackFromComboBox(QComboBox *comboBoxAtackStatus)
{
    if(!comboBoxAtackStatus)
        return attackStatus::Default;

    return static_cast<attackStatus>(comboBoxAtackStatus->currentIndex());
}

void ReconPage::fillData()
{
    QJsonObject dangerTypes {
                            {"RAD", "Радіаційна"},
                            {"CHEM", "Хімічна"},
                            {"BIO", "Біологічна"}
    };

    /*
    ui->comboBoxDangerType->clear();
    for (auto it = dangerTypes.begin(); it != dangerTypes.end(); ++it) {
        ui->comboBoxDangerType->addItem(QString("%1 : %2").arg(it.key(), it.value().toString()), it.key());
    }
    */
    /*
    QStandardItemModel *model = new QStandardItemModel();

    ui->comboBoxDangerType->clear();
    for (auto it = dangerTypes.begin(); it != dangerTypes.end(); ++it) {
        QStandardItem* item = new QStandardItem(QString("%1 : %2").arg(it.key(), it.value().toString()));

        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setData(Qt::Unchecked, Qt::CheckStateRole);

        model->appendRow(item);
    }
    ui->comboBoxDangerType->setModel(model);
    */

    //--------------------------------------------------------------------//
    //                                                                    //
    //--------------------------------------------------------------------//

    QJsonObject workMode {
                         {"AUTO", "Автоматичний режим"},
                         {"MANUAL", "Ручний режим"}
    };

    ui->comboBoxWorkMode->clear();
    for (auto it = workMode.begin(); it != workMode.end(); ++it) {
        ui->comboBoxWorkMode->addItem(QString("%1 : %2").arg(it.key(), it.value().toString()), it.key());
    }

    //--------------------------------------------------------------------//
    //                                                                    //
    //--------------------------------------------------------------------//

    QJsonObject reconType {
                          {"AREA", "Розвідка району"},
                          {"ROUTE", "Розвідка маршруту"}
    };

    ui->comboBoxReconType->clear();
    for (auto it = reconType.begin(); it != reconType.end(); ++it) {
        ui->comboBoxReconType->addItem(QString("%1 : %2").arg(it.key(), it.value().toString()), it.key());
    }

    //--------------------------------------------------------------------//
    //                                                                    //
    //--------------------------------------------------------------------//

    /*
    QJsonObject airResistance {
        {"U", "Нестабільна"},
        {"N", "Нейтральна"},
        {"S", "Стабільна"}
    };

    for (auto it = airResistance.begin(); it != airResistance.end(); ++it) {
        ui->comboBoxAirResistance->addItem(QString("%1 : %2").arg(it.key(), it.value().toString()), it.key());
    }
    */
    ui->comboBoxAirResistance->clear();
    ui->comboBoxAirResistance->addItem(QString("%1 : %2").arg("U", "Нестабільна"), "U");
    ui->comboBoxAirResistance->addItem(QString("%1 : %2").arg("N", "Нейтральна"), "N");
    ui->comboBoxAirResistance->addItem(QString("%1 : %2").arg("S", "Стабільна"), "S");

    //--------------------------------------------------------------------//
    //                                                                    //
    //--------------------------------------------------------------------//

    QJsonObject atackStatus {
                            {"VISIBLE", "Атака спостерігалась"},
                            {"NONVISIBLE", "Атака не спостерігалась"}
    };

    ui->comboBoxAttackStatus->clear();
    for (auto it = atackStatus.begin(); it != atackStatus.end(); ++it) {
        ui->comboBoxAttackStatus->addItem(QString("%1 : %2").arg(it.key(), it.value().toString()), it.key());
    }


}

void ReconPage::setupUI()
{

    ui->textEditCoordinates->setWindowDesription("Вершини району розвідки");
    ui->checkBoxTypeRad->setFocus();

    setTabOrder({
        ui->scrollArea,
        ui->checkBoxTypeRad,
        ui->checkBoxTypeChem,
        ui->checkBoxTypeBio,
        ui->comboBoxReconType,
        ui->comboBoxWorkMode,
        ui->spinBoxReconDuration,
        ui->textEditCoordinates->getWidget(TextEditCoordinates::WidgetType::ComboBox),
        ui->textEditCoordinates->getWidget(TextEditCoordinates::WidgetType::Button),
        ui->textEditCoordinates->getWidget(TextEditCoordinates::WidgetType::TextEdit),
        ui->comboBoxAirResistance,
        ui->pushButtonAirResistanceHelp,
        ui->spinBoxReleaseRadius,
        ui->pushButtonDangerRadiusHelp,
        ui->spinBoxDangerDistance,
        ui->pushButtonDangerRadiusHelp_2,
        ui->comboBoxAttackStatus,
        ui->pushButtonStart,
        // ui->pushButtonShow,
        // ui->pushButtonSend,
        // ui->pushButtonClear
    });
}

void ReconPage::connections()
{

    connect(ui->comboBoxReconType, &QComboBox::currentIndexChanged, this, [this](int index) {
        switch (index) {
        case 0:     ui->textEditCoordinates->setCoordinatesMode(TextEditCoordinates::CoordinatesMode::Area);   break;
        case 1:     ui->textEditCoordinates->setCoordinatesMode(TextEditCoordinates::CoordinatesMode::Route);  break;
        default:    ui->textEditCoordinates->setCoordinatesMode(TextEditCoordinates::CoordinatesMode::Other);  break;

        }
    });

    connect(ui->pushButtonStart, &QPushButton::clicked, this, &ReconPage::collectReconDataFromPage);
}

void ReconPage::hideUnusedUI()
{
    ui->checkBoxTypeBio->hide();
}
