#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QFrame>
#include <QMap>

#include <ui/modules/settingsfield/settingsfield.h>
#include <ui/modules/settingsfield/settingsdeviceitem.h>
#include <backend/settings/settings.h>
#include <backend/data_types/data_types.h>
#include <ui/modules/notifier/notifier.h>
#include "backend/classes/core_types.h"

class PreflightChecklist;

namespace Ui {
class SettingsPage;
}

// Страница настроек: три блока
//   Інтерфейс  - размер текста, дельта текста, тема
//   Прилади    - статус подключённых приборов (SettingsDeviceItem), добавляются через addDevice()
//   Програма   - автосохранение по времени / по дистанции, путь к файлу карт
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage();

    void saveSettings();
    void updateSettings();

    // Ключи в файле настроек
    static constexpr char KEY_FONT_SIZE[]      = "interface/fontSize";
    static constexpr char KEY_FONT_DELTA[]     = "interface/fontDelta";
    static constexpr char KEY_APP_THEME[]      = "interface/appTheme";
    static constexpr char KEY_SAVE_DISTANCE[]  = "programm/autosave/distance";
    static constexpr char KEY_SAVE_TIME[]      = "programm/autosave/time";
    static constexpr char KEY_MAP_FILE[]       = "programm/mapFilePath";

    // --- Прилади ---
    // Добавить строку прибора в блок "Прилади". Возвращает виджет, через который
    // потом обновляется статус (setDeviceStatus, setLastUpdateTime, increseErrorCount ...).
    SettingsDeviceItem *addDevice(const SettingsDeviceItem::DeviceData &data);
    SettingsDeviceItem *deviceItem(TypeDev type) const { return m_devices.value(type, nullptr); }

    PreflightChecklist *preflightWindow() const { return m_preflight; }

public slots:
    // --- от DroneCore (подключаются в MainWindow) ---
    void onLinkStatus(const LinkStatus &status);            ///< строка «Радіомодем»
    void onLoggerStatus(const LoggerStatus &status);        ///< строки приборов логгера + окно проверки
    void onRoutePoint(const RoutePoint &point);             ///< стартовая точка — в окно проверки
    void onAllowedCommands(const QList<quint8> &ids);       ///< доступность кнопок окна проверки

    void openPreflightWindow();

signals:
    void commandRequested(const CommandRequest &request);   ///< ID01 / ID02 / ID03 из окна проверки → DroneCore::sendCommand

    void settingsSaved();
    void styleChangeRequested(int themeId);          // -1 = поменялся только шрифт

    void settingsSaveDistanceChanged(const QVariant &meters);
    void settingsSavePeriodChanged(const QVariant &minutes);
    void mapFileSelected(const QString &path);

private:
    Ui::SettingsPage *ui;

    void connections();
    void setupInterfaceFields();
    void setupDeviceFields();
    void setupProgramFields();
    void loadSerialNumbersFromFile();
    void selectMapFile();
    void updateMapFieldText();

    static QString deviceKey(TypeDev type);          // ключ прибора в settings/devices.json
    QFrame *createHorizontalLine();

    QMap<TypeDev, SettingsDeviceItem *> m_devices;
    QString m_mapFilePath;

    SettingsFieldButton *m_preflightField = nullptr;
    PreflightChecklist  *m_preflight      = nullptr;
};

#endif // SETTINGSPAGE_H
