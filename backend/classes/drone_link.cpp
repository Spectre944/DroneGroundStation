#include "drone_link.h"

/**
    @brief Постоянные формата пакета (протокол, п. 2.2, 2.5).
*/
namespace {
    constexpr int HeaderSize   = 7;     ///< маркер 4 + ID 1 + Length 2
    constexpr int TailSize     = 6;     ///< CRC 2 + конец 4
    constexpr int MaxLength    = 2100;  ///< Length больше — пакет считается мусором (п. 2.5, правило 7)
    constexpr int MaxBurst     = 2048;  ///< наибольшая пачка команд (п. 2.4, правило 5)
    constexpr int DuplicateMs  = 1000;  ///< копия — тот же пакет не позже 1 с

    const QByteArray StartMarker = QByteArray::fromHex("AA55AA55");
    const QByteArray EndMarker   = QByteArray::fromHex("0D0A0D0A");
}

// ============================================================================
//  Создание
// ============================================================================

DroneLink::DroneLink(QObject *parent)
    : QObject(parent)
{
    // Порт и таймер — дочерние объекты: при moveToThread переезжают вместе с DroneLink.
    // Открывать порт и запускать таймер — только из потока, где живёт DroneLink.
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead,     this, &DroneLink::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &DroneLink::onPortError);

    m_linkTimer = new QTimer(this);
    m_linkTimer->setInterval(1000);
    connect(m_linkTimer, &QTimer::timeout, this, &DroneLink::onLinkTimer);
}

// ============================================================================
//  Порт
// ============================================================================

bool DroneLink::open(const QString &name)
{
    if (m_port->isOpen())
        close();

    m_port->setPortName(name);
    m_port->setBaudRate(QSerialPort::Baud115200);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit portError(QStringLiteral("%1: %2").arg(name, m_port->errorString()));
        return false;
    }

    // Всё, что было до открытия, к этому сеансу не относится
    m_port->clear();
    m_rxBuffer.clear();
    m_lastPacket.clear();
    m_lastPacketTime = QDateTime();
    m_lastRxTime = QDateTime();
    m_pending = false;
    m_stats = RxStats();

    m_linkTimer->start();
    onLinkTimer();                      // интерфейс сразу видит «порт открыт, ждём»
    return true;
}

void DroneLink::close()
{
    m_linkTimer->stop();
    if (m_port->isOpen())
        m_port->close();
    m_rxBuffer.clear();
    m_pending = false;
    onLinkTimer();                      // «порт закрыт»
}

/**
    @brief Ошибка порта. Серьёзная (порт пропал, нет доступа) — порт закрывается.
*/
void DroneLink::onPortError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError)
        return;

    emit portError(QStringLiteral("%1: %2").arg(m_port->portName(), m_port->errorString()));

    // ResourceError — порт пропал (вынули USB-адаптер): дальше работать нельзя
    if (error == QSerialPort::ResourceError || error == QSerialPort::PermissionError
        || error == QSerialPort::DeviceNotFoundError)
        close();
}

// ============================================================================
//  Отправка
// ============================================================================

bool DroneLink::canSend() const
{
    if (!m_port->isOpen())
        return false;
    if (!m_pending)
        return true;
    // Ответа нет: повтор — не раньше 20 с (протокол, п. 2.4, правило 3)
    return m_lastSendTime.msecsTo(QDateTime::currentDateTimeUtc()) >= CoreConst::RetryMinS * 1000;
}

DroneLink::SendResult DroneLink::send(quint8 id, const QByteArray &payload)
{
    if (!m_port->isOpen())
        return SendResult::PortClosed;
    if (!canSend())
        return SendResult::Busy;

    const QByteArray packet = buildPacket(id, payload);
    if (packet.size() > MaxBurst)
        return SendResult::TooLong;

    // Несколько копий подряд, без пауз — одна пачка. Сколько поместится в 2048 байт.
    const int copies = qBound(1, qMin(m_commandCopies, int(MaxBurst / packet.size())), 3);
    QByteArray burst;
    for (int i = 0; i < copies; ++i)
        burst.append(packet);

    if (m_port->write(burst) != burst.size())
        return SendResult::WriteError;

    m_pending = true;
    m_lastSendTime = QDateTime::currentDateTimeUtc();
    emit rawData(true, burst, m_lastSendTime);
    return SendResult::Ok;
}

void DroneLink::answerReceived()
{
    m_pending = false;
}

// ============================================================================
//  Настройки
// ============================================================================

void DroneLink::setLinkTimeout(int seconds)
{
    m_linkTimeoutS = qMax(1, seconds);
}

void DroneLink::setQuietExpected(bool quiet)
{
    m_quietExpected = quiet;
}

void DroneLink::setCommandCopies(int copies)
{
    m_commandCopies = qBound(1, copies, 3);
}

// ============================================================================
//  Приём
// ============================================================================

void DroneLink::onReadyRead()
{
    const QByteArray bytes = m_port->readAll();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    emit rawData(false, bytes, now);
    processBytes(bytes, now);
}

/**
    @brief Сборка пакетов из потока байтов (протокол, п. 2.5).
    @note  Разбираются все целые пакеты в буфере; неполный остаётся до следующих байт.
*/
void DroneLink::processBytes(const QByteArray &bytes, const QDateTime &rxTime)
{
    m_rxBuffer.append(bytes);
    const LinkState before = currentStatus().state;

    while (true) {
        // 1. Маркер начала. Всё до него (в том числе 00 00 после копий) — выбросить.
        const int start = m_rxBuffer.indexOf(StartMarker);
        if (start < 0) {
            // Маркера нет: оставить последние 3 байта — в них может быть начало маркера
            if (m_rxBuffer.size() > StartMarker.size() - 1)
                m_rxBuffer.remove(0, m_rxBuffer.size() - (StartMarker.size() - 1));
            break;
        }
        if (start > 0)
            m_rxBuffer.remove(0, start);

        // 2. Заголовок целиком? Иначе — ждать байты.
        if (m_rxBuffer.size() < HeaderSize)
            break;

        const quint8  id     = quint8(m_rxBuffer[4]);
        const quint16 length = quint16(quint8(m_rxBuffer[5]) | (quint8(m_rxBuffer[6]) << 8));

        // 3. Недопустимая длина — это не пакет: сдвиг на 1 байт, искать маркер дальше
        if (length == 0 || length > MaxLength) {
            ++m_stats.lengthErrors;
            m_rxBuffer.remove(0, 1);
            continue;
        }

        // 4. Пакет целиком? Иначе — ждать байты.
        const int fullSize = HeaderSize + length + TailSize;
        if (m_rxBuffer.size() < fullSize)
            break;

        // 5. Конец — строго на своём месте (0D 0A 0D 0A бывает и внутри Payload)
        if (m_rxBuffer.mid(HeaderSize + length + 2, EndMarker.size()) != EndMarker) {
            ++m_stats.endErrors;
            m_rxBuffer.remove(0, 1);
            continue;
        }

        // 6. CRC16 по ID + Length + Payload; в пакете — младший байт первым
        const quint16 crcCalc = crc16(m_rxBuffer.mid(4, 3 + length));
        const quint16 crcRecv = quint16(quint8(m_rxBuffer[HeaderSize + length])
                                      | (quint8(m_rxBuffer[HeaderSize + length + 1]) << 8));
        if (crcCalc != crcRecv) {
            ++m_stats.crcErrors;
            m_rxBuffer.remove(0, 1);
            continue;
        }

        // Пакет правильный — вырезать из буфера
        const QByteArray raw = m_rxBuffer.left(fullSize);
        m_rxBuffer.remove(0, fullSize);

        // 7. Копия: тот же пакет не позже 1 с после предыдущего (или его копии)
        const bool duplicate = !m_lastPacket.isEmpty() && raw == m_lastPacket
                               && m_lastPacketTime.msecsTo(rxTime) <= DuplicateMs;
        m_lastPacket = raw;
        m_lastPacketTime = rxTime;          // следующая копия считается от этой
        if (duplicate) {
            ++m_stats.duplicates;
            continue;
        }

        // 8. Новый пакет — дальше
        ++m_stats.packets;
        m_lastRxTime = rxTime;

        Packet packet;
        packet.id = id;
        packet.payload = raw.mid(HeaderSize, length);
        packet.rxTime = rxTime;
        emit packetReceived(packet);
    }

    // Связь появилась или восстановилась — сообщить сразу, не ждать таймера
    if (currentStatus().state != before)
        onLinkTimer();
}

// ============================================================================
//  Состояние связи
// ============================================================================

LinkStatus DroneLink::currentStatus() const
{
    LinkStatus s;
    s.portName = m_port->portName();
    s.commandPending = m_pending;

    if (!m_port->isOpen()) {
        s.state = LinkState::Closed;
        return s;
    }
    if (!m_lastRxTime.isValid()) {
        s.state = LinkState::Waiting;       // пакетов ещё не было
        return s;
    }

    s.secondsSinceLastPacket = int(m_lastRxTime.secsTo(QDateTime::currentDateTimeUtc()));
    if (s.secondsSinceLastPacket < m_linkTimeoutS)
        s.state = LinkState::Ok;
    else
        s.state = m_quietExpected ? LinkState::Silent : LinkState::Lost;
    return s;
}

void DroneLink::onLinkTimer()
{
    emit linkStatusChanged(currentStatus());
}

// ============================================================================
//  Пакет
// ============================================================================

quint16 DroneLink::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (const char byte : data) {
        crc ^= quint16(quint8(byte)) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? quint16((crc << 1) ^ 0x1021) : quint16(crc << 1);
    }
    return crc;
}

QByteArray DroneLink::buildPacket(quint8 id, const QByteArray &payload)
{
    // Пакет «без данных» — Payload из одного байта 0x01
    const QByteArray data = payload.isEmpty() ? QByteArray(1, char(0x01)) : payload;
    const quint16 length = quint16(data.size());

    // ID + Length + Payload — по ним считается CRC
    QByteArray body;
    body.append(char(id));
    body.append(char(length & 0xFF));           // Length, младший байт первым
    body.append(char(length >> 8));
    body.append(data);

    const quint16 crc = crc16(body);

    QByteArray packet;
    packet.reserve(HeaderSize + data.size() + TailSize);
    packet.append(StartMarker);
    packet.append(body);
    packet.append(char(crc & 0xFF));            // CRC, младший байт первым
    packet.append(char(crc >> 8));
    packet.append(EndMarker);
    return packet;
}
