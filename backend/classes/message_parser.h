#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

/**
    @file  message_parser.h
    @brief Разбор сообщений логгера (Ф5): пакет → данные.

    Получает от DroneLink готовый пакет (ID и Payload), проверяет длину данных,
    раскладывает байты по полям и выдаёт результат сигналами:
    точку маршрута, измерение, начало и порцию спектра, ответ на команду, событие.

    @note  Форматы — «Протокол обмена для наземного ПО», п. 6.
           Числа в Payload — младший байт первым (как хранит STM32).
    @note  Флаги достоверности (Ф9) парсер не ставит — это дальше по цепочке.
*/

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QVariantMap>

#include "core_types.h"

class MessageParser : public QObject
{
    Q_OBJECT

public:
    explicit MessageParser(QObject *parent = nullptr);

    /**
        @brief Разобрать пакет: проверить длину, по ID разложить данные, выдать сигнал.
        @note  Неверная длина или недопустимое значение — событие, данные не выдаются.
               Неизвестный ID — событие-предупреждение, работа не останавливается.
    */
    void parse(const Packet &packet);

    /**
        @brief Текущий режим логгера — записывается в каждую точку маршрута.
        @note  Сообщает LoggerState (шаг 8). До этого — Unknown.
    */
    void setLoggerMode(LoggerMode mode) { m_mode = mode; }

    /**
        @brief  Положенная длина данных сообщения.
        @return Длина в байтах; -1 — ID неизвестен.
    */
    static int expectedLength(quint8 id);

signals:
    void answerReceived(quint8 id);                             ///< ID01, ID02, ID03 → DroneCommands
    void loggerMessage(quint8 id, const QVariantMap &params);   ///< сообщение, влияющее на состояние логгера → LoggerState
    void routePoint(const RoutePoint &point);                   ///< ID19, ID20, ID22
    void measurement(const Measurement &m);                     ///< ID22 (без ПАЕД на 1 м)
    void spectrumStarted(const SpectrumInfo &info);             ///< ID27
    void spectrumPortion(const SpectrumPortion &portion);       ///< ID23
    void event(const CoreEvent &e);                             ///< событие для журнала

private:
    bool parseCoord(const Packet &p);           ///< ID19, ID20
    bool parseMeasurement(const Packet &p);     ///< ID22
    bool parsePortion(const Packet &p);         ///< ID23
    bool parseSpectrumStart(const Packet &p);   ///< ID27

    void sendEvent(EventCode code, const Packet &p, const QVariantMap &params = {}, bool debug = false);
    void sendBadValue(const Packet &p, const QString &field);

    /**
        @brief  Время логгера (ЧЧ:ММ:СС) + дата по часам компьютера.
        @param  rxTime Время приёма: из трёх дат (вчера, сегодня, завтра) берётся та,
                при которой время пакета ближе всего к нему — поправка около полуночи.
        @return Пустое (isValid() == false), если часы, минуты или секунды вне диапазона.
    */
    static QDateTime logTime(quint8 h, quint8 m, quint8 s, const QDateTime &rxTime);

    static float   readFloat(const QByteArray &data, int offset);  ///< float, младший байт первым
    static quint16 readU16(const QByteArray &data, int offset);    ///< uint16, младший байт первым
    static quint8  readU8(const QByteArray &data, int offset) { return quint8(data[offset]); }

    LoggerMode m_mode = LoggerMode::Unknown;    ///< текущий режим логгера — от LoggerState
};

#endif // MESSAGE_PARSER_H
