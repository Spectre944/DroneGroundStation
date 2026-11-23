#include "spectrum_collector.h"

SpectrumCollector::SpectrumCollector(QObject *parent)
    : QObject(parent)
{
    // Таймер — дочерний: переезжает в поток ядра вместе с SpectrumCollector
    m_timer = new QTimer(this);
    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, &SpectrumCollector::onTimer);
}

// ============================================================================
//  Начало, порции, конец
// ============================================================================

void SpectrumCollector::onStart(const SpectrumInfo &info)
{
    if (m_collecting)
        close(SpectrumStopReason::NewSpectrum);     // прежний не закрыт — закрыть
    begin(info);
    emit spectrumUpdated(m_current);                // график: набор начат, порций пока нет
}

void SpectrumCollector::onPortion(const SpectrumPortion &portion)
{
    if (!m_collecting) {
        // Порция без ID27: ID27 потерялся в эфире. Спектр без номера, начало — время порции.
        SpectrumInfo info;
        info.startTime = portion.rxTime;
        begin(info);
    }

    // Сумма по каналам с насыщением 65535 — как в логгере
    bool full = false;
    const int n = qMin(int(portion.channels.size()), CoreConst::SpectrumChannels);
    for (int ch = 0; ch < n; ++ch) {
        const quint32 sum = quint32(m_current.channels[ch]) + portion.channels[ch];
        m_current.channels[ch] = quint16(qMin<quint32>(sum, CoreConst::SpectrumMaxCount));
        if (m_current.channels[ch] >= CoreConst::SpectrumStopCount)
            full = true;
    }

    m_current.portionList.append(portion);
    m_current.info.portions = m_current.portionList.size();
    m_current.info.lastPaed = portion.paed;
    m_current.info.lastStat = portion.stat;

    // В spectrumUpdated — только последняя порция, иначе каждый сигнал копирует весь набор
    Spectrum update = m_current;
    update.portionList = { portion };
    emit spectrumUpdated(update);

    // Канал дошёл до 55705 — логгер останавливает набор сам; сообщения об этом нет
    if (full) {
        close(SpectrumStopReason::ChannelFull);
        emit stoppedByItself(SpectrumStopReason::ChannelFull);
    }
}

void SpectrumCollector::finish(SpectrumStopReason reason)
{
    if (!m_collecting || reason == SpectrumStopReason::NewSpectrum)
        return;
    close(reason);
}

void SpectrumCollector::setDuration(quint16 seconds)
{
    m_nextDurationS = seconds;
}

void SpectrumCollector::reset()
{
    m_timer->stop();
    m_collecting = false;
    m_current = Spectrum();
}

// ============================================================================
//  Внутреннее
// ============================================================================

void SpectrumCollector::begin(const SpectrumInfo &info)
{
    m_current = Spectrum();
    m_current.info = info;
    m_current.info.setDurationS = m_nextDurationS;
    m_current.info.finished = false;
    m_current.info.stopReason = SpectrumStopReason::None;
    m_collecting = true;
    m_since.start();
    if (m_nextDurationS > 0)
        m_timer->start();
}

void SpectrumCollector::close(SpectrumStopReason reason)
{
    m_timer->stop();
    m_collecting = false;
    m_current.info.finished = true;
    m_current.info.stopReason = reason;
    emit spectrumFinished(m_current);               // все порции — для «водопада» и базы
}

/**
    @brief Заданное время набора + запас истекли — набор окончен.
    @note  Нужно в режиме «по оператору»: там после спектра ID22 сам не придёт.
*/
void SpectrumCollector::onTimer()
{
    if (!m_collecting || m_current.info.setDurationS == 0)
        return;
    if (m_since.elapsed() >= qint64(m_current.info.setDurationS) * 1000 + m_marginMs) {
        close(SpectrumStopReason::TimeElapsed);
        emit stoppedByItself(SpectrumStopReason::TimeElapsed);
    }
}
