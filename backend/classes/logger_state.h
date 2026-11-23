#ifndef LOGGER_STATE_H
#define LOGGER_STATE_H

/**
    @file  logger_state.h
    @brief Состояние логгера (Ф4): режим, приборы, спектр, радиомолчание,
           разрешённые команды.

    Логгер свой режим не сообщает — LoggerState вычисляет его по сообщениям
    логгера и ответам на команды (протокол, п. 3.5).

    @note  Разрешённые команды — по матрице «команда × режим» (протокол, п. 5).
    @note  «Режим не совпадает» — логгер ответил ID03 на команду, разрешённую
           в нашем режиме: логгер в другом режиме, чем мы думаем
           (например, сообщение об аварии потерялось в эфире).
*/

#include <QObject>
#include <QList>
#include <QVariantMap>

#include "core_types.h"

class LoggerState : public QObject
{
    Q_OBJECT

public:
    explicit LoggerState(QObject *parent = nullptr);

    /**
        @brief Сообщение логгера (от MessageParser::loggerMessage).
        @param id     ID сообщения.
        @param params Параметры: voltage (ID04, ID05), device (ID25).
    */
    void onMessage(quint8 id, const QVariantMap &params);

    /**
        @brief Итог команды (от DroneCommands::commandFinished).
    */
    void onCommandFinished(const CommandOutcome &outcome);

    /**
        @brief Измерение (ID22) — для напряжения аккумулятора.
    */
    void onMeasurement(const Measurement &m);


    /**
        @brief Набор спектра окончен по решению SpectrumCollector (канал 55705 или время).
        @note  Логгер об этом не сообщает: снимается «идёт набор», команды — как без набора.
               spectrumFinished не выдаётся — SpectrumCollector уже закрыл набор.
    */
    void onSpectrumStopped();

    /**
        @brief Сброс: режим «неизвестен», приборы «не проверялись».
        @note  При открытии порта и новом полёте.
    */
    void reset();

    const LoggerStatus &status() const { return m_status; }
    QList<quint8> allowedCommands() const { return m_allowed; }

    /**
        @brief  Разрешённые команды для состояния — по матрице протокола.
        @param  afterSdError Последняя проверка закончилась ошибкой SD (ID16): разрешены ID02, ID03.
    */
    static QList<quint8> allowedFor(const LoggerStatus &s, bool afterSdError);

signals:
    void statusChanged(const LoggerStatus &status);         ///< любое изменение состояния → DroneCore, база
    void allowedCommandsChanged(const QList<quint8> &ids);  ///< изменился список разрешённых → DroneCommands, интерфейс
    void modeChanged(LoggerMode mode);                      ///< изменился режим → MessageParser
    void radioSilenceChanged(bool on);                      ///< радиомолчание вкл / выкл → DroneCommands, DroneLink
    void enteredError(quint8 msgId);                        ///< переход в аварию (ID24, ID25, ID26, ID28) → тревоги
    void modeMismatch(const CommandRequest &request);       ///< ID03 на разрешённую команду → тревоги
    void spectrumFinished(SpectrumStopReason reason);       ///< набор спектра окончен → SpectrumCollector

private:
    void setDevice(LoggerDevice d, DeviceCheck check, quint8 msgId);
    void setMode(LoggerMode mode);
    void enterError(quint8 msgId);
    void finishSpectrum(SpectrumStopReason reason);         ///< набор шёл — закончить; подготовка — отменить
    void publish();                                         ///< выдать сигналы о том, что изменилось

    LoggerStatus  m_status;
    QList<quint8> m_allowed;            ///< разрешённые команды — последние выданные
    bool          m_afterSdError = false;   ///< проверка закончилась ID16: разрешены ID02, ID03

    LoggerMode    m_lastMode = LoggerMode::Unknown;     ///< для сигнала modeChanged
    bool          m_lastSilence = false;                ///< для сигнала radioSilenceChanged
};

#endif // LOGGER_STATE_H
