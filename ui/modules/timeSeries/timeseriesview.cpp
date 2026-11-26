#include "timeseriesview.h"
#include "timeseriesplot.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "backend/settings/settings.h"
#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor

namespace {
const QString KEY_COMBINED = QStringLiteral("graphs/combined");
// Пороги полосы аккумулятора по умолчанию (временно — порогов напряжения в ядре и настройках нет)
constexpr double kBattOk  = 11.4;
constexpr double kBattLow = 11.0;
}

TimeSeriesView::TimeSeriesView(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    // ---- заголовок ----
    auto *head = new QHBoxLayout();
    auto *title = new QLabel(tr("Графіки польоту"), this);
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() * 1.15);
    title->setFont(tf);
    m_source = new QLabel(this);
    head->addWidget(title);
    head->addSpacing(8);
    head->addWidget(m_source, 1);
    // вид: всё на одном графике или панелями
    m_together = new QPushButton(tr("Разом"), this);
    m_apart = new QPushButton(tr("Окремо"), this);
    auto *mode = new QButtonGroup(this);
    for (QPushButton *b : {m_together, m_apart}) {
        b->setCheckable(true);
        mode->addButton(b);
        head->addWidget(b);
    }
    head->addSpacing(8);
    m_reset = new QPushButton(tr("Весь політ"), this);
    head->addWidget(m_reset);
    lay->addLayout(head);

    // ---- какие графики показывать ----
    auto *opts = new QHBoxLayout();
    m_paed = new QCheckBox(tr("ПАЕД"), this);
    m_height = new QCheckBox(tr("Висота"), this);
    m_batt = new QCheckBox(tr("Акумулятор"), this);
    m_log = new QCheckBox(tr("ПАЕД — логарифмічна шкала"), this);
    for (QCheckBox *c : {m_paed, m_height, m_batt})
        c->setChecked(true);
    opts->addWidget(m_paed);
    opts->addWidget(m_height);
    opts->addWidget(m_batt);
    opts->addStretch(1);
    opts->addWidget(m_log);
    lay->addLayout(opts);

    // ---- график ----
    m_plot = new TimeSeriesPlot(this);
    m_plot->setMinimumHeight(380);
    m_plot->setEmptyText(tr("Вимірювань ще немає"));

    m_pPaed   = m_plot->addPanel(tr("ПАЕД, мкЗв/год"), tr("мкЗв/год"), 3);
    m_sPaed   = m_plot->addSeries(m_pPaed, tr("ПАЕД"), StatusColor::info());
    m_sPaed1m = m_plot->addSeries(m_pPaed, tr("на 1 м"), StatusColor::spectrum());
    m_pHeight = m_plot->addPanel(tr("Висота LIDAR, м"), tr("м"), 1);
    // высота — «земляной» цвет: не путается с зелёным полосы аккумулятора и синим ПАЕД
    m_sHeight = m_plot->addSeries(m_pHeight, tr("Висота"), QColor(0x9A, 0x84, 0x5E));
    m_pBatt   = m_plot->addPanel(tr("Акумулятор, В"), tr("В"), 2);
    m_sBatt   = m_plot->addSeries(m_pBatt, tr("АКБ"), StatusColor::warning());
    lay->addWidget(m_plot, 1);

    // роли в виде «Разом»: ПАЕД — главная, высота — фон, аккумулятор — цветная полоса
    m_plot->setPanelRole(m_pPaed, TimeSeriesPlot::Role::Primary);
    m_plot->setPanelRole(m_pHeight, TimeSeriesPlot::Role::Background);
    m_plot->setPanelRole(m_pBatt, TimeSeriesPlot::Role::Band);
    setBatteryLevels(kBattOk, kBattLow);

    const bool together = Settings::instance().getValue(KEY_COMBINED, true).toBool();
    (together ? m_together : m_apart)->setChecked(true);
    m_plot->setCombined(together);
    connect(m_together, &QPushButton::toggled, this, [this](bool on) {
        m_plot->setCombined(on);
        Settings::instance().setValue(KEY_COMBINED, on);
        Settings::instance().saveSettings();
    });

    connect(m_paed,   &QCheckBox::toggled, this, [this](bool on) { m_plot->setPanelVisible(m_pPaed, on); });
    connect(m_height, &QCheckBox::toggled, this, [this](bool on) { m_plot->setPanelVisible(m_pHeight, on); });
    connect(m_batt,   &QCheckBox::toggled, this, [this](bool on) { m_plot->setPanelVisible(m_pBatt, on); });
    connect(m_log,    &QCheckBox::toggled, this, [this](bool on) { m_plot->setPanelLog(m_pPaed, on); });
    connect(m_reset,  &QPushButton::clicked, m_plot, &TimeSeriesPlot::resetZoom);

    updateTitle();
}

/////////////////////////////////// Текущий полёт ///////////////////////////////////

void TimeSeriesView::appendLive(const Measurement &m)
{
    m_live.append(m);
    if (m_showingLive) {
        addToPlot(m);
        updateTitle();
    }
}

void TimeSeriesView::clearLive(const QString &flightName)
{
    m_live.clear();
    m_liveName = flightName;
    if (m_showingLive) {
        m_plot->clearData();
        updateTitle();
    }
}

void TimeSeriesView::showLive()
{
    m_showingLive = true;
    m_loading = false;
    m_archiveId = -1;
    fill(m_live);
    updateTitle();
}

/////////////////////////////////// Прошлый полёт ///////////////////////////////////

void TimeSeriesView::showArchive(qint64 sessionId, const QString &name)
{
    m_showingLive = false;
    m_archiveId = sessionId;
    m_archiveName = name;
    m_loading = true;
    m_plot->clearData();
    m_plot->setEmptyText(tr("Завантаження…"));
    updateTitle();
}

void TimeSeriesView::onTimeSeriesReady(qint64 sessionId, const QVector<Measurement> &series)
{
    if (m_showingLive || sessionId != m_archiveId)
        return;                         // ответ на старый запрос
    m_loading = false;
    fill(series);
    updateTitle();
}

void TimeSeriesView::setBatteryLevels(double ok, double low)
{
    m_plot->setBandLevels(m_pBatt, ok, low);
}

void TimeSeriesView::setThresholds(double warn, double alarm)
{
    m_plot->setLevels(m_pPaed, {
        {warn,  StatusColor::warning(), tr("попередження")},
        {alarm, StatusColor::alarm(),   tr("тривога")},
    });
}

/////////////////////////////////// Внутреннее ///////////////////////////////////

/**
    @brief Разложить измерения по линиям.
    @note  ПАЕД на 1 м — только там, где посчитана (hasPaed1m); высота 0 — LIDAR не измерил
           (точка пропускается); напряжение 0 — не передано.
*/
void TimeSeriesView::fill(const QVector<Measurement> &series)
{
    QVector<QPointF> paed, paed1m, height, batt;
    for (const Measurement &m : series) {
        if (!m.time.isValid())
            continue;
        const double t = double(m.time.toMSecsSinceEpoch());
        paed << QPointF(t, m.paed);
        if (m.hasPaed1m)
            paed1m << QPointF(t, m.paed1m);
        if (m.lidarCm > 0)
            height << QPointF(t, m.lidarCm / 100.0);
        if (m.battV > 0)
            batt << QPointF(t, m.battV);
    }
    m_plot->setSeriesData(m_pPaed, m_sPaed, paed);
    m_plot->setSeriesData(m_pPaed, m_sPaed1m, paed1m);
    m_plot->setSeriesData(m_pHeight, m_sHeight, height);
    m_plot->setSeriesData(m_pBatt, m_sBatt, batt);
    m_plot->setEmptyText(tr("Вимірювань немає"));
    m_plot->resetZoom();
}

void TimeSeriesView::addToPlot(const Measurement &m)
{
    if (!m.time.isValid())
        return;
    const double t = double(m.time.toMSecsSinceEpoch());
    m_plot->appendPoint(m_pPaed, m_sPaed, QPointF(t, m.paed));
    if (m.hasPaed1m)
        m_plot->appendPoint(m_pPaed, m_sPaed1m, QPointF(t, m.paed1m));
    if (m.lidarCm > 0)
        m_plot->appendPoint(m_pHeight, m_sHeight, QPointF(t, m.lidarCm / 100.0));
    if (m.battV > 0)
        m_plot->appendPoint(m_pBatt, m_sBatt, QPointF(t, m.battV));
}

void TimeSeriesView::updateTitle()
{
    const QString muted = StatusColor::muted().name();
    if (m_showingLive) {
        const QString name = m_liveName.isEmpty() ? QString() : QStringLiteral(" «%1»").arg(m_liveName.toHtmlEscaped());
        m_source->setText(QStringLiteral("<span style='color:%1'>%2%3 · вимірювань: %4</span>")
                              .arg(muted, tr("поточний політ"), name).arg(m_live.size()));
    } else {
        m_source->setText(QStringLiteral("<span style='color:%1'>%2</span> <b>«%3»</b>%4")
                              .arg(StatusColor::spectrum().name(), tr("архівний політ"),
                                   m_archiveName.toHtmlEscaped(),
                                   m_loading ? tr(" · завантаження…") : QString()));
    }
}
