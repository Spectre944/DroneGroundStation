#include "drone_commands.h"
#include "drone_link.h"
#include "recon_area.h"

#include <cmath>
#include <cstring>

/**
    @brief Постоянные команд (протокол, п. 4).
*/
namespace {
    constexpr int AnswerTimeoutMs = 20000;  ///< ожидание ответа: самый долгий цикл логгера ~19 с
    constexpr quint8 FirstId = 1;
    constexpr quint8 LastId  = 14;
    constexpr quint8 IdExitSilence = 14;    ///< ID14 — единственная команда в радиомолчании

    /// Добавить uint16 младшим байтом вперёд
    void appendU16(QByteArray &b, quint32 v)
    {
        b.append(char(v & 0xFF));
        b.append(char((v >> 8) & 0xFF));
    }

    /// Добавить float младшим байтом вперёд (IEEE 754, как в STM32)
    void appendFloat(QByteArray &b, double value)
    {
        const float f = float(value);
        quint32 bits;
        std::memcpy(&bits, &f, sizeof(bits));
        for (int i = 0; i < 4; ++i)
            b.append(char((bits >> (8 * i)) & 0xFF));
    }
}

DroneCommands::DroneCommands(DroneLink *link, QObject *parent)
    : QObject(parent), m_link(link)
{
    // Таймер — дочерний: переезжает в поток ядра вместе с DroneCommands
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(AnswerTimeoutMs);
    connect(m_timer, &QTimer::timeout, this, &DroneCommands::onTimeout);
}

// ============================================================================
//  Отправка
// ============================================================================

void DroneCommands::send(const CommandRequest &request)
{
    // Проверки — по порядку; первая не прошедшая даёт причину отказа
    if (m_readOnly) {
        reject(request, RejectReason::ReadOnly);
        return;
    }
    if (request.id < FirstId || request.id > LastId) {
        reject(request, RejectReason::BadId);
        return;
    }
    if (m_waiting) {                        // следующая команда — только после ответа на предыдущую
        reject(request, RejectReason::Busy);
        return;
    }
    if (m_allowedKnown && !m_allowed.contains(request.id)) {
        reject(request, RejectReason::NotAllowed);
        return;
    }
    if (m_radioSilence && request.id != IdExitSilence) {
        // Логгер выполнил бы, но ответа не прислал бы — итог неизвестен
        reject(request, RejectReason::NotAllowed);
        return;
    }
    const RejectReason bad = validate(request);
    if (bad != RejectReason::None) {
        reject(request, bad);
        return;
    }
	
	// ID04: полная проверка области — выпуклость, пересечения, размер и т. д.
    if (request.id == 0x04 && !ReconArea::check(request.vertices, false, m_maxAreaKm).ok) {
        reject(request, RejectReason::AreaNotValid);
        return;
    }

    // Отправка
    switch (m_link->send(request.id, buildPayload(request))) {
    case DroneLink::SendResult::Ok:
        break;
    case DroneLink::SendResult::Busy:
        reject(request, RejectReason::Busy);
        return;
    case DroneLink::SendResult::TooLong:
        reject(request, RejectReason::BadValue);
        return;
    case DroneLink::SendResult::PortClosed:
    case DroneLink::SendResult::WriteError:
        reject(request, RejectReason::PortClosed);
        return;
    }

    // Ушла — ждём ответа
    m_current = CommandOutcome();
    m_current.request = request;
    m_current.sentTime = QDateTime::currentDateTimeUtc();
    m_current.attempts = 1;
    m_waiting = true;
    m_timer->start();

    emit commandSent(request);
}

void DroneCommands::reject(const CommandRequest &request, RejectReason reason)
{
    CommandOutcome out;
    out.request = request;
    out.result = CommandResult::Rejected;
    out.reason = reason;
    out.sentTime = out.finishTime = QDateTime::currentDateTimeUtc();
    out.attempts = 0;                       // не отправлялась
    emit commandFinished(out);
}

// ============================================================================
//  Ответ
// ============================================================================

void DroneCommands::onAnswer(quint8 id)
{
    if (!m_waiting) {
        // Ответ ни к чему не относится: опоздал после «нет ответа» или лишний
        CoreEvent e;
        e.code = EventCode::UnexpectedAnswer;
        e.time = QDateTime::currentDateTimeUtc();
        e.params = {{QStringLiteral("msgId"), id}};
        emit event(e);
        return;
    }

    switch (id) {
    case 0x01: finish(CommandResult::Done);      break;
    case 0x02: finish(CommandResult::BadPacket); break;
    case 0x03: finish(CommandResult::Forbidden); break;
    default:   break;                       // другие ID сюда не приходят
    }
}

void DroneCommands::onTimeout()
{
    if (m_waiting)
        finish(CommandResult::NoAnswer);
}

void DroneCommands::finish(CommandResult result)
{
    m_timer->stop();
    m_waiting = false;

    // Ответ получен — DroneLink снова может отправлять сразу.
    // «Нет ответа» — ожидание в DroneLink не снимается: повтор не раньше 20 с от отправки.
    if (result != CommandResult::NoAnswer)
        m_link->answerReceived();

    m_current.result = result;
    m_current.finishTime = QDateTime::currentDateTimeUtc();
    emit commandFinished(m_current);
}

// ============================================================================
//  Состояние
// ============================================================================

void DroneCommands::setAllowed(const QList<quint8> &ids)
{
    m_allowed = ids;
    m_allowedKnown = true;
}

void DroneCommands::setRadioSilence(bool on)
{
    m_radioSilence = on;
}

void DroneCommands::setReadOnly(bool on)
{
    m_readOnly = on;
}

void DroneCommands::setAnswerTimeoutMs(int ms)
{
    m_timer->setInterval(qMax(1, ms));
}


void DroneCommands::setMaxAreaKm(double km)
{
    m_maxAreaKm = km;
}

// ============================================================================
//  Проверка значений и сборка данных
// ============================================================================

RejectReason DroneCommands::validate(const CommandRequest &r)
{
    switch (r.id) {
    case 0x04: {                                    // параметры разведки
        if (r.vertices.size() < CoreConst::AreaMinVertices || r.vertices.size() > CoreConst::AreaMaxVertices)
            return RejectReason::AreaNotValid;
        for (const GeoPoint &p : r.vertices) {
            if (!std::isfinite(p.lat) || !std::isfinite(p.lon)
                || p.lat < -90.0 || p.lat > 90.0 || p.lon < -180.0 || p.lon > 180.0)
                return RejectReason::AreaNotValid;
        }
        if (r.reconMode != ReconMode::Operator && r.reconMode != ReconMode::Time
            && r.reconMode != ReconMode::Distance)  // 3 (спектр) в ID04 не допускается
            return RejectReason::BadReconMode;
        if (r.intervalS == 0)
            return RejectReason::BadInterval;
        if (r.distanceM == 0)
            return RejectReason::BadDistance;
        if (r.dupFactor < 2 || r.dupFactor > 5)
            return RejectReason::BadDupFactor;
        return RejectReason::None;                  // время спектра — любое
    }
    case 0x05:                                      // коэффициент дублирования
        return (r.value >= 2 && r.value <= 5) ? RejectReason::None : RejectReason::BadDupFactor;
    case 0x06:                                      // интервал, с
        return (r.value >= 1 && r.value <= 0xFFFF) ? RejectReason::None : RejectReason::BadInterval;
    case 0x07:                                      // расстояние, м
        return (r.value >= 1 && r.value <= 0xFFFF) ? RejectReason::None : RejectReason::BadDistance;
    case 0x09:                                      // режим разведки 0–3
        return (r.value >= 0 && r.value <= 3) ? RejectReason::None : RejectReason::BadReconMode;
    case 0x0B:                                      // время спектра, с; 0 — до ID12
        return (r.value >= 0 && r.value <= 0xFFFF) ? RejectReason::None : RejectReason::BadValue;
    default:                                        // остальные — без данных
        return RejectReason::None;
    }
}

QByteArray DroneCommands::buildPayload(const CommandRequest &r)
{
    QByteArray b;
    switch (r.id) {
    case 0x04: {
        // N | вершины (широта, долгота — float) | режим | интервал | расстояние | коэффициент | время спектра
        b.append(char(r.vertices.size()));
        for (const GeoPoint &p : r.vertices) {
            appendFloat(b, p.lat);
            appendFloat(b, p.lon);
        }
        b.append(char(quint8(r.reconMode)));
        appendU16(b, r.intervalS);
        appendU16(b, r.distanceM);
        b.append(char(r.dupFactor));
        appendU16(b, r.spectrumS);
        break;
    }
    case 0x05: case 0x09:                           // 1 байт
        b.append(char(quint8(r.value)));
        break;
    case 0x06: case 0x07: case 0x0B:                // 2 байта
        appendU16(b, quint32(r.value));
        break;
    default:                                        // без данных
        break;
    }
    return b;
}
