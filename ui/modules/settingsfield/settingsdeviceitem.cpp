#include "settingsdeviceitem.h"
#include "qstyle.h"
#include "ui_settingsdeviceitem.h"

SettingsDeviceItem::SettingsDeviceItem(QWidget *parent, const DeviceData &data)
    : QWidget(parent)
    , ui(new Ui::SettingsDeviceItem)
    , _data(new DeviceData(data))
{
    ui->setupUi(this);

    setupUI();
    fillData();
    connections();

}

SettingsDeviceItem::~SettingsDeviceItem()
{
    delete _data;
    delete ui;
}

void SettingsDeviceItem::setupUI()
{
    ui->labelDeviceName->setProperty("textSize", "md");
    ui->labelDevicePort->setProperty("textSize", "sm");

    ui->labelDeviceStatus->setProperty("textSize", "sm");
    ui->labelLastUpdate->setProperty("textSize", "sm");
    ui->labelErrorCount->setProperty("textSize", "sm");

    ui->labelLastUpdate->setProperty("textRole", "secondary");
    ui->labelDevicePort->setProperty("textRole", "secondary");

    lastUpdateTimer = new QTimer(this);
    lastUpdateTimer->setInterval(UPDATE_INTERVAL_MS);
    lastUpdateTimer->start();

    ui->pushButtonThreshold->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/sliders2-vertical.svg"));
}

void SettingsDeviceItem::fillData()
{
    m_activeIcon = QPixmap(":/icons/png/old/okCheck.png");
    m_inactiveIcon = QPixmap(":/icons/png/old/error.png");
    m_errorIcon = QPixmap(":/icons/png/old/warning.png");
}

void SettingsDeviceItem::connections()
{
    connect(lastUpdateTimer, &QTimer::timeout, this, [this](){
        _data->lastUpdate += UPDATE_STEP_SECONDS;
        updateUI();
    });

    connect(ui->pushButtonSettings, &QPushButton::clicked, this, [this](){
        emit additionalSettingsRequested(_data);
    });

    connect(ui->pushButtonThreshold, &QPushButton::clicked, this, [this](){
        emit thredholdWindowRequested();
    });
}

void SettingsDeviceItem::setDeviceData(const DeviceData& data)
{
    *_data = data;
    updateUI();
}


void SettingsDeviceItem::updateErrorUI()
{

    ui->labelErrorCount->setText(QString("Помилок: %1").arg(_data->errorCount));

    if (_data->errorCount == 0) {
        ui->labelErrorCount->setProperty("textColor", "default");
    } else if (_data->errorCount < 3) {
        ui->labelErrorCount->setProperty("textColor", "warning");
    } else {
        ui->labelErrorCount->setProperty("textColor", "error");
    }

    ui->labelErrorCount->style()->unpolish(ui->labelErrorCount);
    ui->labelErrorCount->style()->polish(ui->labelErrorCount);
}

void SettingsDeviceItem::updateStatusUI()
{
    // Emoji for text status (alternate display) ✅ ❌ ⚠️
    if (_data->status == DeviceStatus::Active){
        ui->labelDeviceStatus->setProperty("textColor", "accept");
        ui->labelDeviceStatus->setText(QString("%1").arg("Активний"));
        ui->labelIsActive->setPixmap(m_activeIcon);
    } else if (_data->status == DeviceStatus::Inactive){
        ui->labelDeviceStatus->setProperty("textColor", "error");
        ui->labelDeviceStatus->setText(QString("%1").arg("Помилка"));
        ui->labelIsActive->setPixmap(m_errorIcon);
    } else if(_data->status == DeviceStatus::Disconnected){
        ui->labelDeviceStatus->setProperty("textColor", "error");
        ui->labelDeviceStatus->setText(QString("%1").arg("Від'єднаний"));
        ui->labelIsActive->setPixmap(m_inactiveIcon);
    }
    else {
        ui->labelDeviceStatus->setProperty("textColor", "error");
        ui->labelDeviceStatus->setText(QString("%1").arg("Помилка"));
        ui->labelIsActive->setPixmap(m_inactiveIcon);
    }

    int seconds = _data->lastUpdate;
    QString timeText;

    if (seconds < 60) {
        timeText = QString("%1 сек. назад").arg(seconds);
    } else {
        int minutes = seconds / 60;
        timeText = QString("%1 хв. назад").arg(minutes);
    }

    ui->labelLastUpdate->setText(timeText);

    ui->labelDeviceStatus->style()->unpolish(ui->labelDeviceStatus);
    ui->labelDeviceStatus->style()->polish(ui->labelDeviceStatus);
}

void SettingsDeviceItem::updateInfoUI()
{
    ui->labelDeviceName->setText(_data->name);
    ui->labelDevicePort->setText(QString("Порт: %1").arg(_data->port));

}

void SettingsDeviceItem::updateUI()
{
    updateInfoUI();
    updateStatusUI();
    updateErrorUI();
}

void SettingsDeviceItem::setDeviceName(const QString &name)
{
    _data->name = name;
    updateInfoUI();
}

void SettingsDeviceItem::setDevicePort(const QString &port)
{
    _data->port = port;
    updateInfoUI();
}

void SettingsDeviceItem::setDeviceStatus(const DeviceStatus &newStatus, bool force)
{
    if (force || statusPriority(newStatus) >= statusPriority(_data->status)) {
        _data->status = newStatus;
        updateStatusUI();
    }
}

void SettingsDeviceItem::setLastUpdateTime(int seconds)
{
    _data->lastUpdate = seconds;
    updateStatusUI();
}

void SettingsDeviceItem::setErrorCount(int count)
{
    if (count < 0) {
        qWarning() << "Error count cannot be negative, setting to 0";
        count = 0;
    }

    _data->errorCount = count;
    updateErrorUI();
}

void SettingsDeviceItem::setThresholdBtnActive(bool state)
{
    ui->pushButtonThreshold->setEnabled(state);
}

void SettingsDeviceItem::setSerialNumber(const QString &serial)
{
    if(serial.isEmpty())
        return;

    _data->serial = serial;
}

void SettingsDeviceItem::increseErrorCount()
{
    _data->errorCount++;
    updateErrorUI();
}

int SettingsDeviceItem::statusPriority(DeviceStatus status)
{
    switch (status) {
        case DeviceStatus::Active:       return 0;
        case DeviceStatus::Warning:      return 1;
        case DeviceStatus::Inactive:     return 2;
        case DeviceStatus::Disconnected: return 3;
    }
    return -1;
}

