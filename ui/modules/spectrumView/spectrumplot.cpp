#include "spectrumplot.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>

#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor

namespace {

constexpr int kDefaultChannels = 1024;

/// «Красивый» шаг сетки: 1, 2, 5 × 10^k, не меньше rawStep.
double niceStep(double rawStep)
{
    if (rawStep <= 0)
        return 1;
    const double mag = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double f = rawStep / mag;
    return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * mag;
}

QString shortCount(double v)
{
    if (v >= 1e6) return QLocale().toString(v / 1e6, 'g', 3) + QStringLiteral("M");
    if (v >= 1e4) return QLocale().toString(v / 1e3, 'g', 3) + QStringLiteral("k");
    return QLocale().toString(qRound64(v));
}

QColor mix(const QColor &a, const QColor &b, double k)
{
    return QColor::fromRgbF(a.redF() * (1 - k) + b.redF() * k,
                            a.greenF() * (1 - k) + b.greenF() * k,
                            a.blueF() * (1 - k) + b.blueF() * k);
}

} // namespace


SpectrumPlot::SpectrumPlot(QWidget *parent)
    : QWidget(parent)
    , m_line(StatusColor::spectrum())
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::ClickFocus);
    setToolTip(tr("ЛКМ і протягнути вправо — збільшити, вліво — скинути\n"
                  "Клік — маркер каналу · ПКМ — зсув · Колесо — масштаб · Подвійний клік — скинути"));
    autoY();
}

/////////////////////////////////// Данные ///////////////////////////////////

void SpectrumPlot::setData(const QVector<quint16> &channels)
{
    const bool sizeChanged = channels.size() != m_data.size();
    m_data = channels;
    if (sizeChanged)
        resetZoom();
    else if (!m_yZoomed)
        autoY();
    update();
}

void SpectrumPlot::setOverlay(const QVector<quint16> &channels)
{
    m_overlay = channels;
    if (!m_yZoomed)
        autoY();
    update();
}

void SpectrumPlot::clearOverlay()
{
    setOverlay({});
}

void SpectrumPlot::clear()
{
    m_data.clear();
    m_overlay.clear();
    m_info.clear();
    setMarker(-1);
    resetZoom();
}

void SpectrumPlot::setLogScale(bool on)
{
    if (m_log == on)
        return;
    m_log = on;
    m_yZoomed = false;  // рамка Y в старой шкале смысла не имеет
    autoY();
    emit logScaleChanged(on);
    update();
}

void SpectrumPlot::setXRange(double from, double to)
{
    if (to < from)
        std::swap(from, to);
    m_x0 = from;
    m_x1 = qMax(from + 4.0, to);    // не уже 4 каналов
    clampX();
    m_yZoomed = false;
    autoY();
    emitRange();
    update();
}

void SpectrumPlot::resetZoom()
{
    m_x0 = -0.5;
    m_x1 = channelCount() - 0.5;
    m_yZoomed = false;
    autoY();
    emitRange();
    update();
}

void SpectrumPlot::setInfoLines(const QStringList &lines)
{
    m_info = lines;
    update();
}

void SpectrumPlot::setLineColor(const QColor &c)
{
    m_line = c;
    update();
}

void SpectrumPlot::setMarker(int channel)
{
    if (channel >= channelCount())
        channel = -1;
    if (m_marker == channel)
        return;
    m_marker = channel;
    emit markerChanged(channel);
    update();
}

/////////////////////////////////// Пересчёт координат ///////////////////////////////////

int SpectrumPlot::channelCount() const
{
    return m_data.isEmpty() ? (m_overlay.isEmpty() ? kDefaultChannels : int(m_overlay.size()))
                            : int(m_data.size());
}

QRectF SpectrumPlot::plotRect() const
{
    const QFontMetrics fm(font());
    const double left = fm.horizontalAdvance(QStringLiteral("00000")) + 10;
    const double bottom = fm.height() + 10;
    return QRectF(left, 8, width() - left - 12, height() - 8 - bottom);
}

double SpectrumPlot::tr_(double v) const   { return m_log ? std::log10(qMax(0.0, v) + 1.0) : v; }
double SpectrumPlot::untr(double t) const  { return m_log ? std::pow(10.0, t) - 1.0 : t; }

double SpectrumPlot::xToPx(double ch) const
{
    const QRectF r = plotRect();
    return r.left() + (ch - m_x0) / (m_x1 - m_x0) * r.width();
}

double SpectrumPlot::pxToX(double px) const
{
    const QRectF r = plotRect();
    return m_x0 + (px - r.left()) / r.width() * (m_x1 - m_x0);
}

double SpectrumPlot::tToPx(double t) const
{
    const QRectF r = plotRect();
    return r.bottom() - (t - m_t0) / (m_t1 - m_t0) * r.height();
}

double SpectrumPlot::pxToT(double py) const
{
    const QRectF r = plotRect();
    return m_t0 + (r.bottom() - py) / r.height() * (m_t1 - m_t0);
}

quint16 SpectrumPlot::valueAt(int ch) const
{
    return ch >= 0 && ch < m_data.size() ? m_data.at(ch) : 0;
}

void SpectrumPlot::autoY()
{
    const int a = qMax(0, int(std::ceil(m_x0)));
    const int b = qMin(channelCount() - 1, int(std::floor(m_x1)));
    quint16 mx = 0;
    for (int i = a; i <= b; ++i) {
        if (i < m_data.size())    mx = qMax(mx, m_data.at(i));
        if (i < m_overlay.size()) mx = qMax(mx, m_overlay.at(i));
    }
    m_t0 = 0.0;
    m_t1 = qMax(tr_(qMax<double>(mx, m_log ? 9 : 10)) * 1.08, 1e-6);
}

void SpectrumPlot::clampX()
{
    const double lo = -0.5, hi = channelCount() - 0.5;
    const double w = qMin(m_x1 - m_x0, hi - lo);
    if (m_x0 < lo) { m_x0 = lo; m_x1 = lo + w; }
    if (m_x1 > hi) { m_x1 = hi; m_x0 = hi - w; }
}

void SpectrumPlot::emitRange()
{
    emit xRangeChanged(m_x0, m_x1);
}

/////////////////////////////////// Отрисовка ///////////////////////////////////

void SpectrumPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Base));
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = plotRect();
    if (r.width() < 20 || r.height() < 20)
        return;

    drawGrid(p, r);

    p.save();
    p.setClipRect(r);
    if (!m_overlay.isEmpty())
        drawSeries(p, r, m_overlay, mix(palette().color(QPalette::Text), palette().color(QPalette::Base), 0.45),
                   false, true);
    if (!m_data.isEmpty())
        drawSeries(p, r, m_data, m_line, true, false);

    const QColor cross = QColor(StatusColor::alarm().red(), StatusColor::alarm().green(),
                                StatusColor::alarm().blue(), 190);

    // Маркер канала
    if (m_marker >= 0 && m_marker + 0.5 >= m_x0 && m_marker - 0.5 <= m_x1) {
        const double x = xToPx(m_marker);
        p.setPen(QPen(cross, 1.5));
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
    }

    // Рамка зума
    if (m_drag == Drag::Zoom) {
        const QRectF zr = QRectF(m_press, m_mouse).normalized();
        p.setPen(QPen(cross, 1.5));
        p.setBrush(QColor(cross.red(), cross.green(), cross.blue(), 25));
        p.drawRoundedRect(zr, 3, 3);
    }

    // Перекрестье
    const bool showCross = m_hover && m_drag != Drag::Pan && r.contains(m_mouse);
    if (showCross) {
        p.setPen(QPen(cross, 1));
        p.drawLine(QPointF(m_mouse.x(), r.top()), QPointF(m_mouse.x(), r.bottom()));
        p.drawLine(QPointF(r.left(), m_mouse.y()), QPointF(r.right(), m_mouse.y()));
    }
    p.restore();

    // Подписи перекрестья и маркера — поверх, вне клипа
    if (showCross) {
        const int ch = qBound(0, qRound(pxToX(m_mouse.x())), channelCount() - 1);
        drawBadge(p, QPointF(m_mouse.x(), r.bottom() + 2), QString::number(ch), cross, Qt::AlignBottom);
        drawBadge(p, QPointF(r.left() - 2, m_mouse.y()), shortCount(untr(pxToT(m_mouse.y()))), cross, Qt::AlignLeft);
        if (!m_data.isEmpty())
            drawBadge(p, QPointF(m_mouse.x() + 8, m_mouse.y() - 8),
                      tr("к. %1 · %2").arg(ch).arg(QLocale().toString(valueAt(ch))),
                      mix(palette().color(QPalette::Base), palette().color(QPalette::Text), 0.85), Qt::AlignRight);
    }
    if (m_marker >= 0 && m_marker + 0.5 >= m_x0 && m_marker - 0.5 <= m_x1)
        drawBadge(p, QPointF(xToPx(m_marker), r.top() + 2),
                  tr("к. %1 · %2").arg(m_marker).arg(QLocale().toString(valueAt(m_marker))), cross, Qt::AlignTop);

    drawInfo(p, r);
}

void SpectrumPlot::drawGrid(QPainter &p, const QRectF &r)
{
    const QColor text = palette().color(QPalette::Text);
    const QColor base = palette().color(QPalette::Base);
    const QColor grid = mix(base, text, 0.10);
    const QColor axis = mix(base, text, 0.35);
    const QColor label = mix(base, text, 0.65);
    const QFontMetrics fm(font());

    p.setPen(QPen(axis, 1));
    p.drawLine(r.bottomLeft(), r.bottomRight());
    p.drawLine(r.bottomLeft(), r.topLeft());

    // ---- X: каналы ----
    const double xStep = niceStep((m_x1 - m_x0) / qMax(2.0, r.width() / 80.0));
    for (double x = std::ceil(m_x0 / xStep) * xStep; x <= m_x1; x += xStep) {
        const double px = xToPx(x);
        p.setPen(QPen(grid, 1));
        p.drawLine(QPointF(px, r.top()), QPointF(px, r.bottom()));
        p.setPen(label);
        const QString s = QString::number(qRound(x));
        p.drawText(QPointF(px - fm.horizontalAdvance(s) / 2.0, r.bottom() + fm.ascent() + 4), s);
    }

    // ---- Y ----
    auto yLabel = [&](double t, const QString &s) {
        const double py = tToPx(t);
        if (py < r.top() - 1 || py > r.bottom() + 1)
            return;
        p.setPen(QPen(grid, 1));
        p.drawLine(QPointF(r.left(), py), QPointF(r.right(), py));
        p.setPen(label);
        p.drawText(QPointF(r.left() - 6 - fm.horizontalAdvance(s), py + fm.ascent() / 2.0 - 1), s);
    };

    if (m_log) {
        for (int k = 0; k <= 6; ++k) {
            const double v = std::pow(10.0, k);
            yLabel(tr_(v), shortCount(v));
        }
    } else {
        const double yStep = niceStep((m_t1 - m_t0) / qMax(2.0, r.height() / 40.0));
        for (double t = std::ceil(m_t0 / yStep) * yStep; t <= m_t1; t += yStep)
            yLabel(t, shortCount(t));
    }
}

void SpectrumPlot::drawSeries(QPainter &p, const QRectF &r, const QVector<quint16> &d,
                              const QColor &c, bool fill, bool dashed)
{
    const int a = qMax(0, int(std::floor(m_x0)));
    const int b = qMin(int(d.size()) - 1, int(std::ceil(m_x1)));
    if (b < a)
        return;

    const double pxPerCh = r.width() / (m_x1 - m_x0);
    const bool steps = pxPerCh >= 3.0;      // при сильном увеличении — ступеньки (каналы — это бины)

    QPainterPath path;
    for (int i = a; i <= b; ++i) {
        const double y = tToPx(tr_(d.at(i)));
        if (steps) {
            const double xl = xToPx(i - 0.5), xr = xToPx(i + 0.5);
            if (i == a) path.moveTo(xl, y); else path.lineTo(xl, y);
            path.lineTo(xr, y);
        } else {
            const double x = xToPx(i);
            if (i == a) path.moveTo(x, y); else path.lineTo(x, y);
        }
    }

    if (fill) {
        QPainterPath area = path;
        area.lineTo(path.currentPosition().x(), r.bottom());
        area.lineTo(path.elementAt(0).x, r.bottom());
        area.closeSubpath();
        QColor fc = c;
        fc.setAlpha(45);
        p.fillPath(area, fc);
    }

    QPen pen(c, dashed ? 1.2 : 1.6);
    if (dashed)
        pen.setStyle(Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

void SpectrumPlot::drawBadge(QPainter &p, const QPointF &anchor, const QString &text,
                             const QColor &bg, Qt::Alignment where)
{
    const QFontMetrics fm(font());
    const QSizeF sz(fm.horizontalAdvance(text) + 10, fm.height() + 4);
    QRectF br(QPointF(0, 0), sz);
    if (where & Qt::AlignBottom)     br.moveCenter(QPointF(anchor.x(), anchor.y() + sz.height() / 2));
    else if (where & Qt::AlignTop)   br.moveCenter(QPointF(anchor.x(), anchor.y() + sz.height() / 2));
    else if (where & Qt::AlignLeft)  br.moveCenter(QPointF(anchor.x() - sz.width() / 2, anchor.y()));
    else                             br.moveTopLeft(anchor - QPointF(0, sz.height()));

    // не вылезать за виджет
    if (br.right() > width() - 1)  br.moveRight(width() - 1);
    if (br.left() < 1)             br.moveLeft(1);
    if (br.top() < 1)              br.moveTop(1);
    if (br.bottom() > height() - 1) br.moveBottom(height() - 1);

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(br, 4, 4);
    p.setPen(bg.lightnessF() > 0.6 ? QColor(20, 20, 20) : QColor(255, 255, 255));
    p.drawText(br, Qt::AlignCenter, text);
}

void SpectrumPlot::drawInfo(QPainter &p, const QRectF &r)
{
    if (m_info.isEmpty())
        return;
    const QFontMetrics fm(font());
    int w = 0;
    for (const QString &s : m_info)
        w = qMax(w, fm.horizontalAdvance(s));
    const QRectF box(r.right() - w - 20, r.top() + 6, w + 14, m_info.size() * fm.height() + 10);

    QColor bg = palette().color(QPalette::Base);
    bg.setAlpha(215);
    p.setPen(QPen(mix(palette().color(QPalette::Base), palette().color(QPalette::Text), 0.2), 1));
    p.setBrush(bg);
    p.drawRoundedRect(box, 6, 6);
    p.setPen(palette().color(QPalette::Text));
    for (int i = 0; i < m_info.size(); ++i)
        p.drawText(QPointF(box.left() + 7, box.top() + 5 + fm.ascent() + i * fm.height()), m_info.at(i));
}

/////////////////////////////////// Мышь ///////////////////////////////////

void SpectrumPlot::mousePressEvent(QMouseEvent *e)
{
    m_press = m_mouse = e->position();
    if (e->button() == Qt::LeftButton) {
        m_drag = Drag::Zoom;
    } else if (e->button() == Qt::RightButton) {
        m_drag = Drag::Pan;
        m_px0 = m_x0; m_px1 = m_x1; m_pt0 = m_t0; m_pt1 = m_t1;
        setCursor(Qt::ClosedHandCursor);
    }
    update();
}

void SpectrumPlot::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->position();
    m_hover = true;

    if (m_drag == Drag::Pan) {
        const QRectF r = plotRect();
        const double dxCh = (m_press.x() - m_mouse.x()) / r.width() * (m_px1 - m_px0);
        m_x0 = m_px0 + dxCh;
        m_x1 = m_px1 + dxCh;
        clampX();
        if (m_yZoomed) {
            const double dt = (m_mouse.y() - m_press.y()) / r.height() * (m_pt1 - m_pt0);
            m_t0 = qMax(0.0, m_pt0 + dt);
            m_t1 = m_t0 + (m_pt1 - m_pt0);
        } else {
            autoY();
        }
        emitRange();
    }
    update();
}

void SpectrumPlot::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_drag == Drag::Zoom && e->button() == Qt::LeftButton) {
        const double dx = m_mouse.x() - m_press.x();
        if (dx > 4) {                       // вправо — увеличить рамку
            const QRectF zr = QRectF(m_press, m_mouse).normalized().intersected(plotRect());
            const double a = pxToX(zr.left()), b = pxToX(zr.right());
            const double tTop = pxToT(zr.top()), tBot = pxToT(zr.bottom());
            m_x0 = a;
            m_x1 = qMax(a + 4.0, b);
            clampX();
            if (zr.height() > 6) {
                m_t0 = qMax(0.0, tBot);
                m_t1 = qMax(m_t0 + 1e-6, tTop);
                m_yZoomed = true;
            } else {
                autoY();
            }
            emitRange();
        } else if (dx < -4) {               // влево — сброс
            resetZoom();
        } else {                            // щелчок — маркер
            const int ch = qBound(0, qRound(pxToX(e->position().x())), channelCount() - 1);
            setMarker(ch == m_marker ? -1 : ch);
        }
    }
    if (m_drag == Drag::Pan)
        unsetCursor();
    m_drag = Drag::None;
    update();
}

void SpectrumPlot::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        resetZoom();
}

void SpectrumPlot::wheelEvent(QWheelEvent *e)
{
    const double k = e->angleDelta().y() > 0 ? 0.8 : 1.25;
    const double c = pxToX(e->position().x());
    m_x0 = c - (c - m_x0) * k;
    m_x1 = c + (m_x1 - c) * k;
    if (m_x1 - m_x0 < 4) { m_x0 = c - 2; m_x1 = c + 2; }
    if (m_x1 - m_x0 > channelCount()) { m_x0 = -0.5; m_x1 = channelCount() - 0.5; }
    clampX();
    m_yZoomed = false;
    autoY();
    emitRange();
    update();
    e->accept();
}

void SpectrumPlot::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(e);
}
