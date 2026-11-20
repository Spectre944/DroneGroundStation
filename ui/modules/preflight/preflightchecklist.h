#ifndef PREFLIGHTCHECKLIST_H
#define PREFLIGHTCHECKLIST_H

#include <QWidget>
#include <QList>

#include "backend/classes/core_types.h"

class QLabel;
class QPushButton;
class QFrame;

/**
    @brief Окно передпольотної перевірки (як «Діагностика» в РХБ).
    @note  Приборы в порядке проверки логгера: GPS → LIDAR → БДБГ → батарея → SD.
           На первой ошибке логгер останавливается и ждёт повтора ID01.
           При ошибке SD — выбор «Продовжити без SD» (ID02) / «Не продовжувати» (ID03).
           Подключение (через SettingsPage):
             DroneCore::loggerStatusChanged    → onLoggerStatus
             DroneCore::routePoint             → onRoutePoint (стартовая точка в режиме Test)
             DroneCore::allowedCommandsChanged → onAllowedCommands
             commandRequested                  → DroneCore::sendCommand
*/
class PreflightChecklist : public QWidget
{
    Q_OBJECT

public:
    explicit PreflightChecklist(QWidget *parent = nullptr);

public slots:
    void onLoggerStatus(const LoggerStatus &status);
    void onRoutePoint(const RoutePoint &point);
    void onAllowedCommands(const QList<quint8> &ids);

signals:
    void commandRequested(const CommandRequest &request);

protected:
    void changeEvent(QEvent *e) override;

private:
    struct Card {
        QFrame *frame  = nullptr;
        QLabel *icon   = nullptr;
        QLabel *name   = nullptr;
        QLabel *state  = nullptr;
        QLabel *detail = nullptr;
    };
    enum class CardState { NotChecked, Running, Ok, Warn, Fail };

    Card makeCard(const QString &name);
    void setCard(LoggerDevice dev, CardState st, const QString &stateText, const QString &detail);
    void refresh();
    QString failText(LoggerDevice dev, quint8 msgId) const;
    void send(quint8 id);

    Card         m_cards[int(LoggerDevice::Count)];
    QLabel      *m_counter   = nullptr;
    QFrame      *m_sdActions = nullptr;
    QPushButton *m_btnNoSd   = nullptr;
    QPushButton *m_btnStop   = nullptr;
    QPushButton *m_btnCheck  = nullptr;

    LoggerStatus m_status;
    RoutePoint   m_start;
    bool         m_hasStart = false;
    QList<quint8> m_allowed;
    bool         m_allowedKnown = false;    ///< пока ядро не прислало список — кнопки по режиму
};

#endif // PREFLIGHTCHECKLIST_H
