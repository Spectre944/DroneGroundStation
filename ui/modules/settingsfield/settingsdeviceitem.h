#ifndef SETTINGSDEVICEITEM_H
#define SETTINGSDEVICEITEM_H

#include <QWidget>
#include <QDateTime>
#include <QTimer>
#include <QStyle>

#include <backend/data_types/data_types.h>
#include <backend/utilities/utilities.h>

namespace Ui {
class SettingsDeviceItem;
}

class SettingsDeviceItem : public QWidget
{
    Q_OBJECT
public:

    enum class DeviceStatus {
        Active,
        Inactive,
        Warning,
        Disconnected
    };

    struct DeviceData {
        QString name;
        TypeDev devType;
        QString serial;
        QString port;
        DeviceStatus status;
        int lastUpdate;
        int errorCount;

        DeviceData() :status(DeviceStatus::Active), lastUpdate(-1), errorCount(0) {}

        DeviceData(const QString& deviceName, const TypeDev& deviceType, const QString& deviceSerial, const QString& devicePort, const DeviceStatus& deviceStatus)
            : name(deviceName), devType(deviceType),serial(deviceSerial), port(devicePort), status(deviceStatus), lastUpdate(-1), errorCount(0) {}
    };

    explicit SettingsDeviceItem(QWidget *parent = nullptr, const DeviceData &data = DeviceData());
    ~SettingsDeviceItem();

    void setDeviceData(const DeviceData& data);

    void setDeviceName(const QString &name);
    void setDevicePort(const QString &port);
    void setDeviceStatus(const DeviceStatus &status, bool force = false);
    void setLastUpdateTime(int seconds);
    void setErrorCount(int count);
    void setThresholdBtnActive(bool);
    void setSerialNumber(const QString& serial);
    void increseErrorCount();
    int statusPriority(DeviceStatus status);

    /**
        @brief Пришёл пакет от прибора: «Останнє оновлення» с нуля.
        @note  Если прибор был «Від'єднаний» (не проверялся), он становится «Активний» —
               раз данные идут, прибор работает. Результат проверки «Помилка» не меняется.
    */
    void markAlive();

    DeviceStatus status() const { return _data->status; }
    const DeviceData &data() const { return *_data; }
    DeviceData *dataPtr() { return _data; }

    /// Текст «N сек. назад» / «N хв. назад» / «немає даних» (lastUpdate < 0)
    static QString lastUpdateText(int seconds);

signals:
    void additionalSettingsRequested(DeviceData* data);
    void thredholdWindowRequested();


private:
    static constexpr int UPDATE_INTERVAL_MS         = 1000;   ///< шаг счётчика «Останнє оновлення»
    static constexpr int UPDATE_STEP_SECONDS        = UPDATE_INTERVAL_MS/1000;

    void setupUI();
    void fillData();
    void connections();

    void updateUI();
    void updateInfoUI();
    void updateErrorUI();
    void updateStatusUI();

    Ui::SettingsDeviceItem *ui;
    DeviceData *_data;
    QTimer *lastUpdateTimer;

    QPixmap m_activeIcon;
    QPixmap m_inactiveIcon;
    QPixmap m_errorIcon;
};

#endif // SETTINGSDEVICEITEM_H
