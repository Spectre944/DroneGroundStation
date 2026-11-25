#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QFrame>
#include <QMap>
#include <functional>

#include <ui/modules/settingsfield/settingsfield.h>
#include <ui/modules/settingsfield/settingsdeviceitem.h>
#include <backend/settings/settings.h>
#include <backend/data_types/data_types.h>
#include <ui/modules/notifier/notifier.h>
#include "backend/classes/core_types.h"
#include "ui/pages/recon/reconparams.h"

class PreflightChecklist;
class DeviceDetailsWindow;

namespace Ui {
class SettingsPage;
}

/**
    @brief Страница «Налаштування».
    @note  Разделы:
             Інтерфейс     — размер текста, тема;
             Прилади       — передполётная проверка, приборы (SettingsDeviceItem) с окном
                             прибора (для радиомодема — выбор порта, Р2-1), пороги ПАЕД у БДБГ-15;
             Розвідка      — типовые параметры ID04 для страницы «Розвідка»;
             Теплова карта — клетка / радиус / степень сетки ядра (DoseGrid);
             Програма      — путь к картам.
           Настройки ядра (CoreSettings) собираются в coreSettings() из файла настроек
           и отдаются сигналом coreSettingsChanged при «Зберегти».
*/
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
    static constexpr char KEY_MAP_FILE[]       = "programm/mapFilePath";

    // Настройки ядра (CoreSettings). Нет ключа — значение по умолчанию CoreSettings
    static constexpr char KEY_PORT[]           = "core/portName";
    static constexpr char KEY_LINK_TIMEOUT[]   = "core/linkTimeoutS";
    static constexpr char KEY_MAX_AREA[]       = "core/maxAreaKm";
    static constexpr char KEY_WARN[]           = "core/warnLevel";
    static constexpr char KEY_ALARM[]          = "core/alarmLevel";
    static constexpr char KEY_GRID_CELL[]      = "core/gridCellM";
    static constexpr char KEY_GRID_RADIUS[]    = "core/gridRadiusM";
    static constexpr char KEY_GRID_POWER[]     = "core/gridPower";
    static constexpr char KEY_CORR_ON[]        = "core/correctionOn";
    static constexpr char KEY_CORR_MU[]        = "core/correctionMu";      ///< correctionCoeffs[0], 1/м
    static constexpr char KEY_CORR_MIN[]       = "core/correctionMinM";
    static constexpr char KEY_CORR_MAX[]       = "core/correctionMaxM";
    static constexpr char KEY_CSV_SEP[]        = "core/csvDecimalSep";
    static constexpr char KEY_CSV_LANG[]       = "core/language";
    static constexpr char KEY_DB_PATH[]        = "core/dbPath";

    /**
        @brief Радиус интерполяции по умолчанию, м.
        @note  В CoreSettings — 50 м, но точки по радиоканалу приходят примерно через 100 м
               (пакет раз в ~20 с): с радиусом 50 м между ними остаются клетки «немає даних».
               Радиус должен быть не меньше расстояния между точками, лучше в 1,5 раза больше.
    */
    static constexpr int DEFAULT_GRID_RADIUS_M = 150;

    /// Настройки ядра из файла настроек (при запуске и после «Зберегти»)
    CoreSettings coreSettings() const;

    /// Типовые параметры ID04 (раздел «Розвідка»)
    ReconParams reconDefaults() const { return ReconParams::load(); }

    /// Последний выбранный порт радиомодема (пусто — ещё не выбирали)
    QString savedPort() const;

    /// Источник списка COM-портов для окна радиомодема (DroneCore::availablePorts)
    void setPortProvider(std::function<QStringList()> provider);

    // --- Прилади ---
    // Добавить строку прибора в блок "Прилади". Возвращает виджет, через который
    // потом обновляется статус (setDeviceStatus, setLastUpdateTime, increseErrorCount ...).
    SettingsDeviceItem *addDevice(const SettingsDeviceItem::DeviceData &data);
    SettingsDeviceItem *deviceItem(TypeDev type) const { return m_devices.value(type, nullptr); }

    PreflightChecklist *preflightWindow() const { return m_preflight; }

public slots:
    // --- от DroneCore (подключаются в MainWindow) ---
    void onLinkStatus(const LinkStatus &status);            ///< строка «Радіомодем» + окно радиомодема
    void onLoggerStatus(const LoggerStatus &status);        ///< строки приборов логгера + окно проверки
    void onRoutePoint(const RoutePoint &point);             ///< GPS на связи; стартовая точка — в окно проверки
    void onMeasurement(const Measurement &m);               ///< GPS, LIDAR, БДБГ, акумулятор на связи
    void onSpectrum(const Spectrum &spectrum);              ///< БДБГ на связи (порция спектра)
    void onAllowedCommands(const QList<quint8> &ids);       ///< доступность кнопок окна проверки

    void openPreflightWindow();

signals:
    void commandRequested(const CommandRequest &request);   ///< ID01 / ID02 / ID03 из окна проверки → DroneCore::sendCommand

    void portOpenRequested(const QString &port);            ///< → DroneCore::openPort ("STUB" — заглушка)
    void portCloseRequested();                              ///< → DroneCore::closePort

    void settingsSaved();
    void coreSettingsChanged(const CoreSettings &settings); ///< после «Зберегти» → DroneCore::applySettings
    void reconDefaultsChanged(const ReconParams &params);   ///< после «Зберегти» → страница «Розвідка»
    void styleChangeRequested(int themeId);          // -1 = поменялся только шрифт

    void mapFileSelected(const QString &path);

private:
    Ui::SettingsPage *ui;

    void connections();
    void setupInterfaceFields();
    void setupDeviceFields();
    void setupReconFields();
    void setupGridFields();
    void setupLinkFields();          ///< Р2-5: связь, размер области
    void setupCorrectionFields();    ///< Р2-5: пересчёт на 1 м
    void setupExportFields();        ///< Р2-5: CSV
    void selectDbFile();             ///< Р2-5: файл базы полётов
    void updateDbFieldText();
    void setupProgramFields();
    void loadSerialNumbersFromFile();
    void selectMapFile();
    void updateMapFieldText();

    /// Раздел страницы (заголовок + рамка), вставляется перед разделом «Програма»
    QVBoxLayout *addSection(const QString &title);

    /// Пришёл пакет от прибора — «Останнє оновлення» с нуля
    void touchDevice(TypeDev type);

    /// Показать / скрыть поля порогов под строкой прибора (кнопка «пороги» в строке)
    void setupDeviceThresholds(SettingsDeviceItem *device, const QList<QWidget *> &fields);
    void toggleDeviceThresholds(SettingsDeviceItem *device);

    static QString deviceKey(TypeDev type);          // ключ прибора в settings/devices.json
    QFrame *createHorizontalLine();

    QMap<TypeDev, SettingsDeviceItem *> m_devices;
    QMap<SettingsDeviceItem *, QList<QWidget *>> m_thresholds;   ///< поля порогов под строкой прибора
    QString m_mapFilePath;
    LoggerStatus m_prevLogger;                       ///< для «пришло новое сообщение прибора»

    SettingsFieldButton *m_preflightField = nullptr;
    PreflightChecklist  *m_preflight      = nullptr;
    DeviceDetailsWindow *m_details        = nullptr;

    // Прилади: пороги ПАЕД (БДБГ-15)
    SettingsFieldDoubleSpinBox *m_warnLevel  = nullptr;
    SettingsFieldDoubleSpinBox *m_alarmLevel = nullptr;

    // Розвідка: типовые параметры ID04
    SettingsFieldCombobox *m_reconMode = nullptr;
    SettingsFieldSpinBox  *m_interval  = nullptr;
    SettingsFieldSpinBox  *m_distance  = nullptr;
    SettingsFieldCombobox *m_dup       = nullptr;
    SettingsFieldSpinBox  *m_spectrum  = nullptr;

    // Зв'язок і область
    SettingsFieldSpinBox       *m_linkTimeout = nullptr;
    SettingsFieldDoubleSpinBox *m_maxArea     = nullptr;

    // Перерахунок на 1 м
    SettingsFieldCombobox      *m_corrOn  = nullptr;
    SettingsFieldDoubleSpinBox *m_corrMu  = nullptr;
    SettingsFieldSpinBox       *m_corrMin = nullptr;
    SettingsFieldSpinBox       *m_corrMax = nullptr;

    // Експорт CSV
    SettingsFieldCombobox *m_csvSep  = nullptr;
    SettingsFieldCombobox *m_csvLang = nullptr;

    // База польотів
    SettingsFieldButton *m_dbField = nullptr;
    QString              m_dbPath;          ///< пусто — flights.db рядом с программой

    // Теплова карта
    SettingsFieldSpinBox       *m_gridCell   = nullptr;
    SettingsFieldSpinBox       *m_gridRadius = nullptr;
    SettingsFieldDoubleSpinBox *m_gridPower  = nullptr;
};

#endif // SETTINGSPAGE_H
