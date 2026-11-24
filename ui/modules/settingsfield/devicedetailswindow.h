#ifndef DEVICEDETAILSWINDOW_H
#define DEVICEDETAILSWINDOW_H

/**
    @file  devicedetailswindow.h
    @brief Окно прибора (по кнопке настроек в строке прибора на странице «Налаштування»).
    @note  Идея — из окна «Діагностика» РХБ (DiagnosticDeviceAction): сведения о приборе
           и действия над ним. Для радиомодема добавлен блок «Підключення» (Р2-1):
           выбор COM-порта (первый пункт — «Тест (заглушка)», порт "STUB"), «Оновити»,
           «Підключити» / «Відключити» и состояние связи от ядра (LinkStatus).
           Окно само к ядру не обращается: список портов — через setPortProvider(),
           подключение — сигналами connectRequested / disconnectRequested.
*/

#include <QPointer>
#include <QWidget>
#include <functional>

#include "settingsdeviceitem.h"
#include "backend/classes/core_types.h"

class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QTimer;

class DeviceDetailsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceDetailsWindow(QWidget *parent = nullptr);

    /// Порт заглушки ядра — первый пункт списка
    static constexpr char STUB_PORT[] = "STUB";

    /// Источник списка COM-портов (DroneCore::availablePorts — синхронный вызов)
    void setPortProvider(std::function<QStringList()> provider) { m_portProvider = std::move(provider); }

    /// Порт, выбранный в прошлый раз (из настроек) — выделяется при открытии
    void setSavedPort(const QString &port) { m_savedPort = port; }

public slots:
    /// Показать окно для прибора из строки item (окно следит за его данными)
    void openFor(SettingsDeviceItem *item);

    /// Состояние связи от ядра: блокировки кнопок, текст состояния
    void onLinkStatus(const LinkStatus &status);

    /// Перечитать список портов (кнопка «Оновити»)
    void refreshPorts();

signals:
    void connectRequested(const QString &port);     ///< → DroneCore::openPort
    void disconnectRequested();                     ///< → DroneCore::closePort

private:
    void buildUi();
    void refresh();                                 ///< сведения о приборе — раз в секунду
    void updateConnectionUi();
    QString selectedPort() const;

    QPointer<SettingsDeviceItem> m_item;
    std::function<QStringList()> m_portProvider;
    QString    m_savedPort;
    LinkStatus m_link;

    QLabel *m_title = nullptr;
    QLabel *m_statusIcon = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_source = nullptr;
    QLabel *m_lastUpdate = nullptr;
    QLabel *m_errors = nullptr;
    QLabel *m_serial = nullptr;

    QGroupBox   *m_connBox = nullptr;
    QComboBox   *m_ports = nullptr;
    QPushButton *m_refresh = nullptr;
    QPushButton *m_connect = nullptr;
    QPushButton *m_disconnect = nullptr;
    QLabel      *m_linkText = nullptr;

    QPushButton *m_resetErrors = nullptr;
    QTimer      *m_timer = nullptr;
};

#endif // DEVICEDETAILSWINDOW_H
