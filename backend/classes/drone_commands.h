#ifndef DRONE_COMMANDS_H
#define DRONE_COMMANDS_H

/**
    @file  drone_commands.h
    @brief Команды наземной станции ID01–ID14 (Ф2): проверка, сборка данных,
           отправка через DroneLink, ожидание ответа логгера.

    Итог каждой команды — сигнал commandFinished: выполнено (ID01),
    ошибка пакета (ID02), запрещено (ID03), нет ответа или не отправлена (с причиной).

    @note  Форматы и правила — «Протокол обмена для наземного ПО», п. 4, 5.
    @note  Команда сама не повторяется: ответ мог потеряться, а команда — выполниться.
           Повтор — решение оператора.
*/

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QTimer>

#include "core_types.h"

class DroneLink;

class DroneCommands : public QObject
{
    Q_OBJECT

public:
    /**
        @param link Через него уходят команды; живёт в том же потоке ядра.
    */
    explicit DroneCommands(DroneLink *link, QObject *parent = nullptr);

    /**
        @brief Проверить команду, собрать данные, отправить, ждать ответа.
        @note  Итог — сигнал commandFinished. Не прошла проверку — сразу Rejected с причиной.
    */
    void send(const CommandRequest &request);

    /**
        @brief Пришёл ответ ID01, ID02 или ID03 (от MessageParser::answerReceived).
    */
    void onAnswer(quint8 id);

    void setAllowed(const QList<quint8> &ids);  ///< разрешённые сейчас команды — от LoggerState
    void setRadioSilence(bool on);              ///< идёт радиомолчание: разрешена только ID14
    void setReadOnly(bool on);                  ///< открыт прошлый полёт — команды не отправляются
    void setAnswerTimeoutMs(int ms);            ///< сколько ждать ответа; по умолчанию 20 с

    bool isWaiting() const { return m_waiting; }    ///< ждём ответ на команду

    /**
        @brief  Проверка значений команды по протоколу.
        @return RejectReason::None — значения допустимы; иначе причина.
        @note   Для ID04 — только число вершин и диапазон координат;
                полная проверка области — ReconArea (шаг 7).
    */
    static RejectReason validate(const CommandRequest &request);

    /**
        @brief  Данные (Payload) команды по формату протокола, младший байт первым.
        @return Пусто — команда «без данных» (DroneLink сам подставит 0x01).
    */
    static QByteArray buildPayload(const CommandRequest &request);

signals:
    void commandSent(const CommandRequest &request);        ///< команда ушла в порт → LoggerState, база
    void commandFinished(const CommandOutcome &outcome);    ///< итог команды → DroneCore, LoggerState, база
    void event(const CoreEvent &e);                         ///< ответ, который ни к чему не относится

private:
    void reject(const CommandRequest &request, RejectReason reason);    ///< итог «не отправлена»
    void finish(CommandResult result);          ///< итог ожидаемой команды: ответ или «нет ответа»
    void onTimeout();

    DroneLink     *m_link = nullptr;
    QTimer        *m_timer = nullptr;   ///< ожидание ответа, однократный

    bool           m_waiting = false;   ///< команда отправлена, ответа нет
    CommandOutcome m_current;           ///< ожидающая команда

    QList<quint8>  m_allowed;           ///< разрешённые команды
    bool           m_allowedKnown = false;  ///< список получен; до этого разрешены все
    bool           m_radioSilence = false;
    bool           m_readOnly = false;
};

#endif // DRONE_COMMANDS_H
