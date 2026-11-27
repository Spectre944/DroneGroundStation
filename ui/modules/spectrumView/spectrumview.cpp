#include "spectrumview.h"
#include "spectrumplot.h"
#include "waterfallplot.h"

#include <QCheckBox>
#include <QComboBox>
#include <QStackedWidget>
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

    // ---- спектры полёта (Р2-14): выбор, сравнение, водопад ----
    auto *pickRow = new QHBoxLayout();
    m_pick = new QComboBox(this);
    m_pick->setMinimumContentsLength(28);
    m_pick->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_cmp = new QComboBox(this);
    m_cmp->setMinimumContentsLength(20);
    m_cmp->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_wfButton = new QPushButton(tr("Водоспад"), this);
    m_wfButton->setCheckable(true);
    m_wfButton->setToolTip(tr("Порції набору за часом (зверху вниз) × канали"));
    pickRow->addWidget(new QLabel(tr("Спектр:"), this));
    pickRow->addWidget(m_pick, 2);
    pickRow->addSpacing(8);
    pickRow->addWidget(new QLabel(tr("Порівняти з:"), this));
    pickRow->addWidget(m_cmp, 1);
    pickRow->addSpacing(8);
    pickRow->addWidget(m_wfButton);
    lay->addLayout(pickRow);

    m_liveHint = new QLabel(this);
    m_liveHint->setWordWrap(true);
    m_liveHint->hide();
    lay->addWidget(m_liveHint);

    // ---- график / водопад ----
    m_plot = new SpectrumPlot(this);
    m_waterfall = new WaterfallPlot(this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_plot);
    m_stack->addWidget(m_waterfall);
    lay->addWidget(m_stack, 1);

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
    connect(m_log, &QCheckBox::toggled, m_waterfall, &WaterfallPlot::setLogScale);
    connect(m_wfButton, &QPushButton::toggled, this, [this](bool on) {
        m_stack->setCurrentWidget(on ? static_cast<QWidget *>(m_waterfall) : m_plot);
    });
    connect(m_pick, &QComboBox::activated, this, [this](int i) { pick(m_pick->itemData(i).toInt()); });
    connect(m_cmp, &QComboBox::activated, this, [this](int i) {
        const int k = m_cmp->itemData(i).toInt();
        if (k == -1 && m_hasLive)
            setOverlay(m_live);
        else if (k >= 0 && k < m_list.size())
            setOverlay(m_list[k]);
        else
            clearOverlay();
    });
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

    m_waterfall->setLogScale(m_log->isChecked());
    fillLists();
    updateHeader();
}


/////////////////////////////////// Данные ///////////////////////////////////

/**
    @brief Живой набор (spectrumUpdated — одна новая порция, spectrumFinished — все порции).
    @note  Порции копятся в m_live — для «водопада» идущего набора. Законченный спектр
           попадает в список спектров текущего полёта. Окно показывает живой набор,
           только если выбран «Поточний» и не открыт прошлый полёт.
*/
void SpectrumView::setSpectrum(const Spectrum &s)
{
    if (m_hasLive && sameSpectrum(m_live.info, s.info) && !s.info.finished) {
        m_live.info = s.info;
        m_live.channels = s.channels;
        m_live.portionList += s.portionList;
    } else {
        m_live = s;                     // новый спектр или законченный (все порции)
    }
    m_hasLive = true;

    if (s.info.finished) {
        bool found = false;
        for (Spectrum &x : m_liveList)
            if (sameSpectrum(x.info, s.info)) {
                x = m_live;
                found = true;
            }
        if (!found)
            m_liveList << m_live;
        if (!m_archive) {
            m_list = m_liveList;
            fillLists();
        }
    } else if (!m_archive && m_pick->count() == 0) {
        fillLists();
    }

    if (!m_archive && m_showing < 0)
        display(m_live);
    updateLiveHint();
}

void SpectrumView::clearLive()
{
    m_liveList.clear();
    m_live = Spectrum();
    m_hasLive = false;
    if (!m_archive) {
        m_list.clear();
        m_showing = -1;
        clear();
        fillLists();
    }
    updateLiveHint();
}

/**
    @brief Прошлый полёт: список — его спектры, показан последний.
*/
void SpectrumView::showFlightSpectra(const QVector<Spectrum> &list, const QString &flightName)
{
    m_archive = true;
    m_flightName = flightName;
    m_list = list;
    clearOverlay();
    fillLists();
    if (m_list.isEmpty()) {
        m_showing = -2;                 // ничего не выбрано
        clear();
    } else {
        pick(m_list.size() - 1);
    }
    updateLiveHint();
}

void SpectrumView::showLive()
{
    m_archive = false;
    m_flightName.clear();
    m_list = m_liveList;
    clearOverlay();
    fillLists();
    pick(-1);
    updateLiveHint();
}

/**
    @brief Щелчок по значку спектра на карте: показать этот спектр.
    @note  Идущий набор — «Поточний»; иначе — по номеру в базе или по номеру и времени начала.
*/
void SpectrumView::openSpectrum(qint64 id, quint16 number, const QDateTime &startTime)
{
    SpectrumInfo want;
    want.id = id;
    want.number = number;
    want.startTime = startTime;
    for (int i = 0; i < m_list.size(); ++i)
        if (sameSpectrum(m_list[i].info, want)) {
            pick(i);
            return;
        }
    if (!m_archive && m_hasLive && sameSpectrum(m_live.info, want))
        pick(-1);
}

bool SpectrumView::sameSpectrum(const SpectrumInfo &a, const SpectrumInfo &b)
{
    if (a.id > 0 && b.id > 0)
        return a.id == b.id;
    return a.number == b.number && a.startTime == b.startTime;
}

QString SpectrumView::itemText(const Spectrum &s) const
{
    const SpectrumInfo &i = s.info;
    QString t = tr("№%1").arg(i.number);
    if (i.startTime.isValid())
        t += QStringLiteral(" · ") + i.startTime.toLocalTime().toString("HH:mm:ss");
    t += QStringLiteral(" · ") + tr("%1 порц.").arg(i.portions);
    t += QStringLiteral(" · ") + tr("%1 мкЗв/год").arg(QLocale().toString(i.lastPaed, 'f', 2));
    if (i.source == DataSource::Sd)
        t += QStringLiteral(" · SD");
    return t;
}

void SpectrumView::fillLists()
{
    const QSignalBlocker b1(m_pick), b2(m_cmp);
    m_pick->clear();
    m_cmp->clear();
    if (!m_archive)
        m_pick->addItem(tr("Поточний (живий набір)"), -1);
    for (int i = 0; i < m_list.size(); ++i)
        m_pick->addItem(itemText(m_list[i]), i);
    if (m_pick->count() == 0)
        m_pick->addItem(tr("Спектрів у польоті немає"), -2);

    m_cmp->addItem(tr("Без порівняння"), -2);
    for (int i = 0; i < m_list.size(); ++i)
        m_cmp->addItem(itemText(m_list[i]), i);
    if (!m_archive && m_hasLive && !m_live.info.finished)
        m_cmp->addItem(tr("Поточний набір"), -1);

    const int at = m_pick->findData(m_showing);
    if (at >= 0)
        m_pick->setCurrentIndex(at);
}

void SpectrumView::pick(int index)
{
    m_showing = index;
    const int at = m_pick->findData(index);
    if (at >= 0) {
        const QSignalBlocker b(m_pick);
        m_pick->setCurrentIndex(at);
    }
    if (index == -1) {
        if (m_hasLive)
            display(m_live);
        else
            clear();
    } else if (index >= 0 && index < m_list.size()) {
        display(m_list[index]);
    }
    if (m_has)
        emit spectrumShown(m_spectrum.info.id, m_spectrum.info.number);
    updateLiveHint();
}

/**
    @brief Подсказка над графиком: показан не идущий набор, а он есть.
*/
void SpectrumView::updateLiveHint()
{
    const bool collecting = m_hasLive && !m_live.info.finished;
    const bool away = m_archive || m_showing != -1;
    if (collecting && away) {
        m_liveHint->setText(tr("<span style='color:#8E6CEF'>Йде набір спектра №%1</span> — "
                               "щоб дивитися його, оберіть «Поточний»%2")
                                .arg(m_live.info.number)
                                .arg(m_archive ? tr(" після повернення до поточного польоту") : QString()));
        m_liveHint->show();
    } else {
        m_liveHint->hide();
    }
}

void SpectrumView::display(const Spectrum &s)
{
    const bool isNew = !m_has || s.info.number != m_spectrum.info.number
                       || s.info.startTime != m_spectrum.info.startTime;
    if (isNew)
        m_seenAt = QDateTime::currentDateTimeUtc();

    m_spectrum = s;
    m_has = true;
    m_plot->setData(s.channels);
    m_waterfall->setPortions(s.portionList);

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
    m_waterfall->clear();
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
    if (m_archive)
        m_title->setText(m_title->text() + QStringLiteral("  <span style='color:#8E6CEF; font-weight:normal'>%1</span>")
                                               .arg(tr("архівний · політ «%1»").arg(m_flightName.toHtmlEscaped())));

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
    m_waterfall->setChannelRange(m_from->value(), m_to->value());
}

void SpectrumView::onRangeEdited()
{
    int a = m_from->value(), b = m_to->value();
    if (b - a < 4)
        b = qMin(CoreConst::SpectrumChannels - 1, a + 4);
    m_plot->setXRange(a - 0.5, b + 0.5);
}
