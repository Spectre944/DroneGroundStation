#ifndef DRONE_LINK_H
#define DRONE_LINK_H

/**
    @file  drone_link.h
    @brief Связь с логгером по радиолинии (Ф1): COM-порт, сборка пакетов,
           CRC16, отсев копий, отправка команд по правилам логгера.

    Пакет: AA 55 AA 55 | ID | Length (uint16 LE) | Payload | CRC16 (LE) | 0D 0A 0D 0A,
    всего 13 + Length байт. Пакет «без данных» — Length = 1, Payload = 0x01.

    @note  Что значат данные, DroneLink не знает: он отдаёт готовые Packet,
           разбирает их MessageParser.
    @note  Основа — приём радиолинии и CRC16 из имитатора (onRadiolineDataReceived,
           calculateCrc16), без окон.
    @note  Протокол обмена для наземного ПО, п. 2.
*/

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QSerialPort>
#include <QTimer>

#include "core_types.h"

class DroneLink : public QObject
{
    Q_OBJECT

public:
    /**
        @brief Счётчики приёма — для оценки качества радиолинии.
    */
    struct RxStats {
        int packets = 0;        ///< принято новых пакетов
        int duplicates = 0;     ///< отброшено копий
        int crcErrors = 0;      ///< отброшено: CRC не совпал
        int endErrors = 0;      ///< отброшено: нет 0D 0A 0D 0A на ожидаемом месте
        int lengthErrors = 0;   ///< отброшено: Length = 0 или больше 2100
    };

    /**
        @brief Итог отправки.
    */
    enum class SendResult {
        Ok,             ///< отправлено
        PortClosed,     ///< порт закрыт
        Busy,           ///< ждём ответ на предыдущую команду, 20 с ещё не прошло
        TooLong,        ///< пакет длиннее 2048 байт
        WriteError      ///< порт не принял данные
    };

    explicit DroneLink(QObject *parent = nullptr);

    // ---------------------------------------------------------------- порт

    /**
        @brief  Открыть COM-порт: 115200, 8N1, без управления потоком.
        @param  name Имя порта, например «COM5».
        @return true — открыт; false — ошибка (текст — сигнал portError).
    */
    bool open(const QString &name);

    /**
        @brief Закрыть порт. Буфер приёма и ожидание ответа сбрасываются.
    */
    void close();

    bool isOpen() const { return m_port->isOpen(); }

    // ---------------------------------------------------------------- отправка

    /**
        @brief  Собрать пакет и отправить, если можно (canSend).
        @param  id      ID команды.
        @param  payload Данные; пустой — пакет «без данных» (Payload = 0x01).
        @note   Пакет уходит m_commandCopies раз подряд одной пачкой:
                логгер выполнит его один раз, а доставка надёжнее.
    */
    virtual SendResult send(quint8 id, const QByteArray &payload);

    /**
        @brief Можно ли сейчас отправлять: порт открыт и ответ на предыдущую
               команду получен (или с её отправки прошло 20 с — можно повторить).
    */
    virtual bool canSend() const;

    /**
        @brief Ответ на команду получен (ID01, ID02 или ID03) — снять ожидание.
        @note  Вызывает DroneCommands.
    */
    virtual void answerReceived();

    // ---------------------------------------------------------------- настройки

    void setLinkTimeout(int seconds);           ///< без пакетов дольше — «связь потеряна»
    void setQuietExpected(bool quiet);          ///< тишина ожидаема (радиомолчание…): «молчание» вместо «потеряна»
    void setCommandCopies(int copies);          ///< копий команды в пачке, 1–3

    // ---------------------------------------------------------------- приём

    /**
        @brief Обработать принятые байты: собрать пакеты, проверить, отсеять копии.
        @param bytes  Новые байты (сколько пришло — хоть часть пакета, хоть несколько).
        @param rxTime Время приёма.
        @note  Вызывается при приёме из порта. Отделён от порта, чтобы
               проверять сборку пакетов без железа (tests/link_test).
    */
    void processBytes(const QByteArray &bytes, const QDateTime &rxTime);

    RxStats rxStats() const { return m_stats; }

    // ---------------------------------------------------------------- пакет

    /**
        @brief  CRC16 CCITT-FALSE: полином 0x1021, начальное значение 0xFFFF, без отражения.
        @note   Контроль: «123456789» → 0x29B1.
    */
    static quint16 crc16(const QByteArray &data);

    /**
        @brief  Собрать пакет целиком: маркер, ID, Length, Payload, CRC16, конец.
        @param  payload Пустой — пакет «без данных» (Payload = 0x01).
    */
    static QByteArray buildPacket(quint8 id, const QByteArray &payload);

signals:
    void packetReceived(const Packet &packet);              ///< новый пакет (не копия) → MessageParser
    void linkStatusChanged(const LinkStatus &status);       ///< раз в секунду и при смене состояния
    void rawData(bool sent, const QByteArray &bytes, const QDateTime &time);    ///< все байты — сырой журнал обмена
    void portError(const QString &text);                    ///< ошибка порта

private:
    void onReadyRead();
    void onPortError(QSerialPort::SerialPortError error);
    void onLinkTimer();
    LinkStatus currentStatus() const;

    QSerialPort *m_port = nullptr;      ///< создаётся в конструкторе, переезжает в поток ядра вместе с DroneLink
    QTimer      *m_linkTimer = nullptr; ///< раз в секунду — linkStatusChanged

    QByteArray   m_rxBuffer;            ///< байты, ещё не собранные в пакет
    QByteArray   m_lastPacket;          ///< последний принятый пакет целиком — для отсева копий
    QDateTime    m_lastPacketTime;      ///< время последнего пакета или его копии
    QDateTime    m_lastRxTime;          ///< время последнего нового пакета — для «N с назад»

    bool         m_pending = false;     ///< команда отправлена, ответа нет
    QDateTime    m_lastSendTime;        ///< когда отправлена

    int          m_linkTimeoutS = 90;   ///< порог «связь потеряна», с
    bool         m_quietExpected = false;
    int          m_commandCopies = 2;

    RxStats      m_stats;
};

#endif // DRONE_LINK_H
