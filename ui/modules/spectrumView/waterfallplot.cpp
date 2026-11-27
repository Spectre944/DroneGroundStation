#include "waterfallplot.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace {

constexpr int kLeft   = 46;     ///< подписи порций
constexpr int kRight  = 10;
constexpr int kTop    = 6;
constexpr int kBottom = 22;     ///< подписи каналов

QColor mix(const QColor &a, const QColor &b, double k)
{
    return QColor::fromRgbF(a.redF() * (1 - k) + b.redF() * k,
                            a.greenF() * (1 - k) + b.greenF() * k,
                            a.blueF() * (1 - k) + b.blueF() * k);
}

double niceStep(double raw)
{
    if (raw <= 0)
        return 1;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / mag;
    return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * mag;
}

} // namespace

WaterfallPlot::WaterfallPlot(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void WaterfallPlot::setPortions(const QList<SpectrumPortion> &portions)
{
    m_portions = portions;
    rebuild();
}

void WaterfallPlot::setChannelRange(int from, int to)
{
    from = qBound(0, from, CoreConst::SpectrumChannels - 1);
    to = qBound(from, to, CoreConst::SpectrumChannels - 1);
    if (from == m_from && to == m_to)
        return;
    m_from = from;
    m_to = to;
    rebuild();
}

void WaterfallPlot::setLogScale(bool on)
{
    m_log = on;
    rebuild();
}

void WaterfallPlot::clear()
{
    m_portions.clear();
    rebuild();
}

/**
    @brief Палитра «водопада»: фон почти чёрно-синий, пики — жёлтые.
*/
QRgb WaterfallPlot::colorAt(double k)
{
    struct Stop { double at; QColor c; };
    static const Stop stops[] = {
        {0.00, QColor(0x0B, 0x12, 0x2B)},
        {0.25, QColor(0x2B, 0x3A, 0x9C)},
        {0.50, QColor(0x8E, 0x3F, 0xC4)},
        {0.75, QColor(0xF0, 0x7A, 0x3A)},
        {1.00, QColor(0xFF, 0xE8, 0x6B)},
    };
    k = qBound(0.0, k, 1.0);
    for (int i = 1; i < 5; ++i)
        if (k <= stops[i].at) {
            const double t = (k - stops[i - 1].at) / (stops[i].at - stops[i - 1].at);
            return mix(stops[i - 1].c, stops[i].c, t).rgb();
        }
    return stops[4].c.rgb();
}

void WaterfallPlot::rebuild()
{
    const int w = m_to - m_from + 1;
    const int h = m_portions.size();
    m_max = 0;
    if (h == 0 || w <= 0) {
        m_image = QImage();
        update();
        return;
    }
    for (const SpectrumPortion &p : std::as_const(m_portions))
        for (int c = m_from; c <= m_to && c < p.channels.size(); ++c)
            m_max = qMax(m_max, p.channels[c]);

    m_image = QImage(w, h, QImage::Format_RGB32);
    const double top = m_log ? std::log1p(double(qMax<quint16>(1, m_max))) : double(qMax<quint16>(1, m_max));
    for (int r = 0; r < h; ++r) {
        const QVector<quint16> &ch = m_portions[r].channels;
        QRgb *line = reinterpret_cast<QRgb *>(m_image.scanLine(r));
        for (int c = 0; c < w; ++c) {
            const int idx = m_from + c;
            const double v = idx < ch.size() ? ch[idx] : 0;
            line[c] = colorAt((m_log ? std::log1p(v) : v) / top);
        }
    }
    update();
}

QRectF WaterfallPlot::plotRect() const
{
    return QRectF(kLeft, kTop, width() - kLeft - kRight, height() - kTop - kBottom);
}

void WaterfallPlot::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    update();
}

void WaterfallPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QColor base = palette().color(QPalette::Base);
    const QColor text = palette().color(QPalette::Text);
    const QColor label = mix(base, text, 0.65);
    p.fillRect(rect(), base);

    const QRectF r = plotRect();
    if (m_image.isNull()) {
        p.setPen(mix(base, text, 0.5));
        p.drawText(rect(), Qt::AlignCenter, tr("Порцій немає"));
        return;
    }

    // картинка без сглаживания: каждая порция — чёткая полоса
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(r, m_image);

    const QFontMetrics fm(font());
    p.setPen(label);

    // слева — номера порций
    const int n = m_portions.size();
    const double rowH = r.height() / n;
    const int every = qMax(1, int(std::ceil((fm.height() + 4) / rowH)));
    for (int i = 0; i < n; i += every) {
        const QString s = QString::number(i + 1);
        const double y = r.top() + (i + 0.5) * rowH;
        p.drawText(QPointF(r.left() - 6 - fm.horizontalAdvance(s), y + fm.ascent() / 2.0 - 1), s);
    }
    p.save();
    p.translate(10, r.center().y());
    p.rotate(-90);
    const QString yTitle = tr("порція");
    p.drawText(QPointF(-fm.horizontalAdvance(yTitle) / 2.0, 0), yTitle);
    p.restore();

    // снизу — каналы
    const int w = m_to - m_from + 1;
    const double cs = niceStep(w / qMax(2.0, r.width() / 70.0));
    for (double c = std::ceil(m_from / cs) * cs; c <= m_to; c += cs) {
        const double x = r.left() + (c - m_from + 0.5) / w * r.width();
        p.drawLine(QPointF(x, r.bottom()), QPointF(x, r.bottom() + 3));
        const QString s = QString::number(int(c));
        p.drawText(QPointF(x - fm.horizontalAdvance(s) / 2.0, r.bottom() + 4 + fm.ascent()), s);
    }

    // наведение
    if (!r.contains(m_mouse))
        return;
    const int row = qBound(0, int((m_mouse.y() - r.top()) / rowH), n - 1);
    const int ch = qBound(m_from, m_from + int((m_mouse.x() - r.left()) / r.width() * w), m_to);
    const SpectrumPortion &pp = m_portions[row];
    const int v = ch < pp.channels.size() ? pp.channels[ch] : 0;
    QString t = tr("Порція %1 · канал %2 · %3 імп.").arg(row + 1).arg(ch).arg(QLocale().toString(v));
    if (pp.rxTime.isValid())
        t += QStringLiteral(" · ") + pp.rxTime.toLocalTime().toString("HH:mm:ss");

    p.setPen(QPen(QColor(255, 255, 255, 160), 1, Qt::DashLine));
    p.drawLine(QPointF(m_mouse.x(), r.top()), QPointF(m_mouse.x(), r.bottom()));
    p.drawRect(QRectF(r.left(), r.top() + row * rowH, r.width(), rowH));

    const QSizeF sz(fm.horizontalAdvance(t) + 12, fm.height() + 6);
    QRectF box(QPointF(m_mouse) + QPointF(12, -sz.height() - 6), sz);
    if (box.right() > width() - 2) box.moveRight(m_mouse.x() - 12);
    if (box.top() < 2)             box.moveTop(m_mouse.y() + 12);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 24, 36, 230));
    p.setRenderHint(QPainter::Antialiasing);
    p.drawRoundedRect(box, 4, 4);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter, t);
}

void WaterfallPlot::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->pos();
    update();
}

void WaterfallPlot::leaveEvent(QEvent *)
{
    m_mouse = QPoint(-1, -1);
    update();
}
