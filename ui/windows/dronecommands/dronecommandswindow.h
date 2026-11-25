#ifndef DRONECOMMANDSWINDOW_H
#define DRONECOMMANDSWINDOW_H

/**
    @file  dronecommandswindow.h
    @brief Окно «Команди дрону» — ID05–ID14 в полёте (Р2-4).
    @note  Отдельное окно (открывается из меню «Дії → Команди дрону…»), чтобы потом
           решить, куда его встроить: виджет можно положить и на страницу.
           Окно само к ядру не обращается:
             - отправка — сигнал sendRequested(CommandRequest); MainWindow вызывает
               DroneCore::sendCommand и возвращает номер через onCommandSent(seq);
             - доступность — onAllowedCommands / onLinkStatus / setReadOnly;
             - итог — onCommandFinished (по номеру запроса), тексты — CoreTexts (таблица 6.1).
           Кнопка серая, если ID нет среди разрешённых, ждём ответ на любую команду
           (commandPending) или открыт прошлый полёт. Подтверждение перед ID08, ID13, ID09 = 3.
*/

#include <QHash>
#include <QWidget>

#include "backend/classes/core_types.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

class DroneCommandsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DroneCommandsWindow(QWidget *parent = nullptr);

public slots:
    void onAllowedCommands(const QList<quint8> &ids);
    void onLinkStatus(const LinkStatus &status);
    void onLoggerStatus(const LoggerStatus &status);
    void onCommandSent(quint32 seq);                    ///< номер от DroneCore::sendCommand
    void onCommandFinished(const CommandOutcome &outcome);
    void setReadOnly(bool readOnly);                    ///< открыт прошлый полёт

signals:
    void sendRequested(const CommandRequest &request);

private:
    /// Строка окна: одна команда — подпись, значение (если есть), кнопка, итог
    struct Row {
        quint8       id = 0;
        QWidget     *value = nullptr;      ///< QSpinBox / QComboBox или nullptr
        QPushButton *button = nullptr;
        QLabel      *result = nullptr;
    };

    void buildUi();
    QWidget *addGroup(const QString &title, const QList<quint8> &ids);
    void addRow(class QGridLayout *grid, int row, quint8 id, const QString &title,
                QWidget *value, const QString &buttonText);
    void send(quint8 id);
    bool confirm(quint8 id, int value);
    int  valueOf(const Row &r) const;
    void updateState();
    void showResult(Row &r, const CommandOutcome &o);

    QHash<quint8, Row> m_rows;
    QList<quint8> m_allowed;
    LinkStatus    m_link;
    LoggerStatus  m_logger;
    bool          m_readOnly = false;

    quint8        m_pendingId = 0;      ///< наша команда, на которую ждём ответ
    quint32       m_pendingSeq = 0;
    bool          m_sending = false;    ///< sendRequested отправлен, номер ещё не пришёл
    QVector<CommandOutcome> m_early;    ///< итоги, пришедшие раньше номера (заглушка)

    QLabel *m_state = nullptr;          ///< режим логгера / почему кнопки серые
};

#endif // DRONECOMMANDSWINDOW_H
