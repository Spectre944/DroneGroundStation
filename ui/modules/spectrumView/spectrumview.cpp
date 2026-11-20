#include "spectrumview.h"
#include "spectrumplot.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace {

QString mmss(qint64 s)
{
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

} // namespace


SpectrumView::SpectrumView(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    // ---- заголовок ----
    auto *head = new QHBoxLayout();
    m_title = new QLabel(tr("Спектр"), this);
    QFont tf = m_title->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() * 1.15);
    m_title->setFont(tf);
    m_state = new QLabel(this);
    head->addWidget(m_title);
    head->addSpacing(8);
    head->addWidget(m_state);
    head->addStretch(1);

    m_log = new QCheckBox(tr("Логарифмічна шкала"), this);
    m_reset = new QPushButton(tr("Скинути масштаб"), this);
    m_clear = new QPushButton(tr("Очистити"), this);
    head->addWidget(m_log);
    head->addWidget(m_reset);
    head->addWidget(m_clear);
    lay->addLayout(head);

    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(4);
    m_progress->setRange(0, 1000);
    m_progress->hide();
    lay->addWidget(m_progress);

    // ---- график ----
    m_plot = new SpectrumPlot(this);
    lay->addWidget(m_plot, 1);

    // ---- диапазон каналов (вместо RangeSlider из QML) ----
    auto *range = new QHBoxLayout();
    m_from = new QSpinBox(this);
    m_to = new QSpinBox(this);
    m_from->setRange(0, CoreConst::SpectrumChannels - 1);
    m_to->setRange(0, CoreConst::SpectrumChannels - 1);
    m_to->setValue(CoreConst::SpectrumChannels - 1);
    range->addWidget(new QLabel(tr("Канали: від"), this));
    range->addWidget(m_from);
    range->addWidget(new QLabel(tr("до"), this));
    range->addWidget(m_to);
    range->addStretch(1);
    lay->addLayout(range);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);

    connect(m_log, &QCheckBox::toggled, m_plot, &SpectrumPlot::setLogScale);
    connect(m_plot, &SpectrumPlot::logScaleChanged, this, [this](bool on) {
        const QSignalBlocker b(m_log);
        m_log->setChecked(on);
    });
    connect(m_reset, &QPushButton::clicked, m_plot, &SpectrumPlot::resetZoom);
    connect(m_clear, &QPushButton::clicked, this, &SpectrumView::clear);
    connect(m_plot, &SpectrumPlot::xRangeChanged, this, &SpectrumView::onRangeChanged);
    connect(m_from, &QSpinBox::editingFinished, this, &SpectrumView::onRangeEdited);
    connect(m_to, &QSpinBox::editingFinished, this, &SpectrumView::onRangeEdited);
    connect(m_timer, &QTimer::timeout, this, &SpectrumView::tick);

    updateHeader();
}


/////////////////////////////////// Данные ///////////////////////////////////

void SpectrumView::setSpectrum(const Spectrum &s)
{
    const bool isNew = !m_has || s.info.number != m_spectrum.info.number
                       || s.info.startTime != m_spectrum.info.startTime;
    if (isNew)
        m_seenAt = QDateTime::currentDateTimeUtc();

    m_spectrum = s;
    m_has = true;
    m_plot->setData(s.channels);

    if (s.info.finished)
        m_timer->stop();
    else if (!m_timer->isActive())
        m_timer->start();

    updateHeader();
    updateInfo();
}

void SpectrumView::setOverlay(const Spectrum &s)
{
    m_overlayNumber = s.info.number;
    m_plot->setOverlay(s.channels);
    updateInfo();
}

void SpectrumView::clearOverlay()
{
    m_overlayNumber = 0;
    m_plot->clearOverlay();
    updateInfo();
}

void SpectrumView::clear()
{
    m_has = false;
    m_spectrum = Spectrum();
    m_overlayNumber = 0;
    m_timer->stop();
    m_plot->clear();
    updateHeader();
}


/////////////////////////////////// Отображение ///////////////////////////////////

void SpectrumView::updateHeader()
{
    if (!m_has) {
        m_title->setText(tr("Спектр"));
        m_state->setText(tr("немає даних"));
        m_progress->hide();
        return;
    }

    const SpectrumInfo &i = m_spectrum.info;
    m_title->setText(tr("Спектр №%1").arg(i.number));

    if (i.finished) {
        m_state->setText(tr("закінчено · %1").arg(stopReasonText(i.stopReason)));
        m_progress->hide();
        return;
    }

    const qint64 elapsed = m_seenAt.isValid() ? m_seenAt.secsTo(QDateTime::currentDateTimeUtc()) : 0;
    if (i.setDurationS > 0) {
        m_state->setText(tr("набір · %1 / %2").arg(mmss(elapsed), mmss(i.setDurationS)));
        m_progress->setValue(int(qMin<qint64>(1000, elapsed * 1000 / i.setDurationS)));
        m_progress->show();
    } else {
        m_state->setText(tr("набір · %1 · до команди «Зупинити»").arg(mmss(elapsed)));
        m_progress->hide();
    }
}

void SpectrumView::updateInfo()
{
    if (!m_has) {
        m_plot->setInfoLines({});
        return;
    }
    const SpectrumInfo &i = m_spectrum.info;
    const QLocale loc;

    quint64 total = 0;
    for (quint16 v : m_spectrum.channels)
        total += v;

    QStringList lines;
    lines << tr("Порцій: %1").arg(i.portions)
          << tr("Сума: %1 імп.").arg(loc.toString(total))
          << tr("ПАЕД: %1 мкЗв/год").arg(loc.toString(i.lastPaed, 'f', 2))
          << tr("Висота: %1 м").arg(loc.toString(i.lidarCm / 100.0, 'f', 1))
          << QStringLiteral("%1 %2").arg(i.lat, 0, 'f', 5).arg(i.lon, 0, 'f', 5);
    if (i.source == DataSource::Sd)
        lines << tr("Джерело: SD-карта");
    if (m_overlayNumber > 0)
        lines << tr("Пунктир: спектр №%1").arg(m_overlayNumber);
    m_plot->setInfoLines(lines);
}

QString SpectrumView::stopReasonText(SpectrumStopReason r) const
{
    switch (r) {
    case SpectrumStopReason::None:          return tr("—");
    case SpectrumStopReason::Measurement:   return tr("логер повернувся до вимірів");
    case SpectrumStopReason::CommandStop:   return tr("зупинено оператором");
    case SpectrumStopReason::CommandFinish: return tr("закінчено розвідку");
    case SpectrumStopReason::NewArea:       return tr("нова область");
    case SpectrumStopReason::NewSpectrum:   return tr("почався новий спектр");
    case SpectrumStopReason::LoggerError:   return tr("аварія");
    case SpectrumStopReason::TimeElapsed:   return tr("минув заданий час");
    case SpectrumStopReason::ChannelFull:   return tr("канал заповнено (55705)");
    }
    return QString();
}

void SpectrumView::tick()
{
    updateHeader();
}


/////////////////////////////////// Диапазон каналов ///////////////////////////////////

void SpectrumView::onRangeChanged(double from, double to)
{
    const QSignalBlocker b1(m_from), b2(m_to);
    m_from->setValue(qMax(0, int(std::ceil(from))));
    m_to->setValue(qMin(CoreConst::SpectrumChannels - 1, int(std::floor(to))));
}

void SpectrumView::onRangeEdited()
{
    int a = m_from->value(), b = m_to->value();
    if (b - a < 4)
        b = qMin(CoreConst::SpectrumChannels - 1, a + 4);
    m_plot->setXRange(a - 0.5, b + 0.5);
}
