#ifndef DRONECOMMANDSWINDOW_H
#define DRONECOMMANDSWINDOW_H

/**
    @file  dronecommandswindow.h
    @brief Окно «Команди дрону» — ID05–ID14 в полёте (Р2-4).
    @note  Отдельное окно (открывается из меню «Дії → Команди дрону…»), чтобы потом
           решить, куда его встроить: виджет можно положить и на страницу.
           Сверху — то, что включается и выключается (переключатели ToggleSwitch):
             «Набір спектра»   — вкл: ID09 = 3, выкл: ID12; положение — LoggerStatus::spectrumStage;
             «Радіомовчання»   — вкл: ID13,     выкл: ID14; положение — LoggerStatus::radioSilence.
           Ниже — команды со значением (ID05, ID06, ID07, ID09 0–2, ID11) и разовые (ID10, ID08).
           Окно само к ядру не обращается:
             - отправка — сигнал sendRequested(CommandRequest); MainWindow вызывает
               DroneCore::sendCommand и возвращает номер через onCommandSent(seq);
             - доступность — onAllowedCommands / onLinkStatus / setReadOnly;
             - итог — onCommandFinished (по номеру запроса), тексты — CoreTexts (таблица 6.1).
           Кнопка / переключатель серые, если нужного ID нет среди разрешённых, ждём ответ
           на любую команду (commandPending) или открыт прошлый полёт.
           Подтверждение перед ID08, ID13, ID09 = 3.
*/

#include <QHash>
#include <QWidget>

#include "backend/classes/core_types.h"

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class ToggleSwitch;

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
    /// Строка с кнопкой: одна команда — подпись, значение (если есть), кнопка, итог
    struct Row {
        quint8       id = 0;
        QWidget     *value = nullptr;      ///< QSpinBox / QComboBox или nullptr
        QPushButton *button = nullptr;
        QLabel      *result = nullptr;
    };
    /// Строка с переключателем: две команды — включить и выключить
    struct Toggle {
        ToggleSwitch *sw = nullptr;
        QLabel       *result = nullptr;
        quint8        onId = 0;
        int           onValue = 0;
        quint8        offId = 0;
    };

    void buildUi();
    void addButtonRow(QGridLayout *grid, int row, quint8 id, const QString &title,
                      QWidget *value, const QString &buttonText);
    void addToggleRow(QGridLayout *grid, int row, Toggle &t, const QString &title, const QString &ids);

    void send(quint8 id, int value, QLabel *result, ToggleSwitch *sw = nullptr);
    bool confirm(quint8 id, int value);
    int  valueOf(const Row &r) const;
    void updateState();
    void showResult(QLabel *result, const CommandOutcome &o);
    QString blockReason(quint8 id, bool busy) const;

    QHash<quint8, Row> m_rows;
    Toggle        m_spectrum;
    Toggle        m_silence;

    QList<quint8> m_allowed;
    LinkStatus    m_link;
    LoggerStatus  m_logger;
    bool          m_readOnly = false;

    // Наша команда, на которую ждём ответ
    quint8        m_pendingId = 0;
    quint32       m_pendingSeq = 0;
    QLabel       *m_pendingResult = nullptr;
    ToggleSwitch *m_pendingSwitch = nullptr;
    bool          m_sending = false;    ///< sendRequested отправлен, номер ещё не пришёл
    QVector<CommandOutcome> m_early;    ///< итоги, пришедшие раньше номера (заглушка)

    QLabel *m_state = nullptr;          ///< режим логгера / почему кнопки серые
};

#endif // DRONECOMMANDSWINDOW_H
