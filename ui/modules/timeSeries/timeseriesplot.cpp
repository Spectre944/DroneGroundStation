#include "timeseriesplot.h"

#include <QDateTime>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <limits>
#include <cmath>

namespace {

constexpr int kLeft   = 58;     ///< поле под подписи Y
constexpr int kRight  = 14;
constexpr int kTop    = 4;
constexpr int kGap    = 12;     ///< между панелями
constexpr double kMinWindowMs = 10000;   ///< сильнее 10 с не увеличиваем

QColor mix(const QColor &a, const QColor &b, double k)
{
    return QColor::fromRgbF(a.redF() * (1 - k) + b.redF() * k,
                            a.greenF() * (1 - k) + b.greenF() * k,
                            a.blueF() * (1 - k) + b.blueF() * k);
}

/// «Красивый» шаг 1, 2, 5 × 10^k
double niceStep(double raw)
{
    if (raw <= 0)
        return 1;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / mag;
    return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * mag;
}

/// Шаг сетки по времени, мс: секунды, минуты, часы
double timeStep(double rawMs)
{
    static const double steps[] = {1e3, 2e3, 5e3, 10e3, 15e3, 30e3,
                                   60e3, 120e3, 300e3, 600e3, 900e3, 1800e3,
                                   3600e3, 7200e3, 3 * 3600e3, 6 * 3600e3, 12 * 3600e3, 24 * 3600e3};
    for (double s : steps)
        if (s >= rawMs)
            return s;
    return 24 * 3600e3;
}

QString number(double v, int decimals)
{
    return QLocale().toString(v, 'f', decimals);
}

/// Подпись оси Y: без лишних нулей
QString axisNumber(double v, double step)
{
    const int dec = step >= 1 ? 0 : qBound(0, int(std::ceil(-std::log10(step))), 4);
    return QLocale().toString(v, 'f', dec);
}

} // namespace


TimeSeriesPlot::TimeSeriesPlot(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setToolTip(tr("Колесо — масштаб за часом · ЛКМ і протягнути — зсув · Подвійний клік — весь політ"));
    m_empty = tr("Немає даних");
}

/////////////////////////////////// Данные ///////////////////////////////////

int TimeSeriesPlot::addPanel(const QString &title, const QString &unit, int decimals)
{
    Panel p;
    p.title = title;
    p.unit = unit;
    p.decimals = decimals;
    m_panels.append(p);
    update();
    return m_panels.size() - 1;
}

int TimeSeriesPlot::addSeries(int panel, const QString &name, const QColor &color)
{
    Series s;
    s.name = name;
    s.color = color;
    m_panels[panel].series.append(s);
    return m_panels[panel].series.size() - 1;
}

void TimeSeriesPlot::setSeriesData(int panel, int series, const QVector<QPointF> &points)
{
    m_panels[panel].series[series].points = points;
    if (m_follow)
        m_x0 = m_x1 = 0;
    update();
}

/**
    @note Точка добавляется в конец (время растёт). Если пользователь не увеличивал
          график, окно расширяется вместе с полётом.
*/
void TimeSeriesPlot::appendPoint(int panel, int series, const QPointF &point)
{
    m_panels[panel].series[series].points.append(point);
    if (m_follow)
        m_x0 = m_x1 = 0;
    update();
}

void TimeSeriesPlot::setLevels(int panel, const QVector<Level> &levels)
{
    m_panels[panel].levels = levels;
    update();
}

void TimeSeriesPlot::setPanelVisible(int panel, bool visible)
{
    m_panels[panel].visible = visible;
    update();
}

void TimeSeriesPlot::setPanelLog(int panel, bool log)
{
    m_panels[panel].log = log;
    update();
}

void TimeSeriesPlot::clearData()
{
    for (Panel &p : m_panels)
        for (Series &s : p.series)
            s.points.clear();
    resetZoom();
}

void TimeSeriesPlot::resetZoom()
{
    m_x0 = m_x1 = 0;
    m_follow = true;
    update();
}

bool TimeSeriesPlot::hasData() const
{
    for (const Panel &p : m_panels)
        for (const Series &s : p.series)
            if (!s.points.isEmpty())
                return true;
    return false;
}

void TimeSeriesPlot::dataRange(double &t0, double &t1) const
{
    t0 = std::numeric_limits<double>::max();
    t1 = std::numeric_limits<double>::lowest();
    for (const Panel &p : m_panels)
        for (const Series &s : p.series)
            if (!s.points.isEmpty()) {
                t0 = qMin(t0, s.points.first().x());
                t1 = qMax(t1, s.points.last().x());
            }
    if (t1 - t0 < kMinWindowMs) {               // одна точка или очень короткий полёт
        const double c = (t0 + t1) / 2;
        t0 = c - kMinWindowMs / 2;
        t1 = c + kMinWindowMs / 2;
    }
}

/////////////////////////////////// Геометрия ///////////////////////////////////

QVector<int> TimeSeriesPlot::visiblePanels() const
{
    QVector<int> out;
    for (int i = 0; i < m_panels.size(); ++i)
        if (m_panels[i].visible)
            out << i;
    return out;
}

QRectF TimeSeriesPlot::panelRect(int order, int count) const
{
    const QFontMetrics fm(font());
    const double axisH = fm.height() + 8;
    const double total = height() - kTop - axisH - kGap * (count - 1);
    const double h = total / qMax(1, count);
    return QRectF(kLeft, kTop + order * (h + kGap), width() - kLeft - kRight, h);
}

/**
    @brief Шкала Y — по точкам в видимом окне времени, с запасом 8 %.
    @note  Пороги входят в шкалу, только если данные до них «дотягивают» (не больше чем в 3 раза
           меньше порога) — иначе низкий фон сплющился бы под линией тревоги.
*/
TimeSeriesPlot::Range TimeSeriesPlot::yRange(const Panel &p) const
{
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (const Series &s : p.series)
        for (const QPointF &pt : s.points) {
            if (pt.x() < m_x0 || pt.x() > m_x1)
                continue;
            if (p.log && pt.y() <= 0)
                continue;
            lo = qMin(lo, pt.y());
            hi = qMax(hi, pt.y());
        }
    if (lo > hi)
        return p.log ? Range{-2, 1} : Range{0, 1};

    for (const Level &l : p.levels)
        if (l.value > 0 && hi * 3 >= l.value)
            hi = qMax(hi, l.value);

    if (p.log) {
        Range r{std::floor(std::log10(lo) * 4) / 4, std::ceil(std::log10(hi) * 4) / 4};
        if (r.hi - r.lo < 0.5)
            r.hi = r.lo + 0.5;
        return r;
    }
    if (hi - lo < 1e-9) {
        const double d = qMax(std::abs(hi) * 0.1, 0.1);
        lo -= d;
        hi += d;
    }
    const double pad = (hi - lo) * 0.08;
    Range r{lo - pad, hi + pad};
    if (lo >= 0 && r.lo < 0)
        r.lo = 0;                       // ПАЕД, высота, напряжение — не отрицательные
    return r;
}

double TimeSeriesPlot::xToPx(double t, const QRectF &r) const
{
    return r.left() + (t - m_x0) / (m_x1 - m_x0) * r.width();
}

double TimeSeriesPlot::pxToX(double px, const QRectF &r) const
{
    return m_x0 + (px - r.left()) / r.width() * (m_x1 - m_x0);
}

double TimeSeriesPlot::yToPx(double v, const Range &y, const Panel &p, const QRectF &r) const
{
    const double t = p.log ? std::log10(qMax(v, 1e-12)) : v;
    return r.bottom() - (t - y.lo) / (y.hi - y.lo) * r.height();
}

void TimeSeriesPlot::clampX()
{
    double t0, t1;
    dataRange(t0, t1);
    const double full = t1 - t0;
    double w = qBound(kMinWindowMs, m_x1 - m_x0, full);
    if (m_x0 < t0) m_x0 = t0;
    if (m_x0 + w > t1) m_x0 = t1 - w;
    m_x1 = m_x0 + w;
    m_follow = w >= full - 1;
}

/////////////////////////////////// Рисование ///////////////////////////////////

void TimeSeriesPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().color(QPalette::Base));

    const QVector<int> order = visiblePanels();
    if (!hasData() || order.isEmpty()) {
        p.setPen(mix(palette().color(QPalette::Base), palette().color(QPalette::Text), 0.5));
        p.drawText(rect(), Qt::AlignCenter, order.isEmpty() ? tr("Оберіть графіки") : m_empty);
        return;
    }

    if (m_x1 <= m_x0)
        dataRange(m_x0, m_x1);

    for (int i = 0; i < order.size(); ++i)
        drawPanel(p, m_panels[order[i]], panelRect(i, order.size()), i == order.size() - 1);
    drawTimeAxis(p, panelRect(order.size() - 1, order.size()));
    drawHover(p, order);
}

void TimeSeriesPlot::drawPanel(QPainter &p, const Panel &panel, const QRectF &r, bool)
{
    const QColor text = palette().color(QPalette::Text);
    const QColor base = palette().color(QPalette::Base);
    const QColor grid = mix(base, text, 0.10);
    const QColor axis = mix(base, text, 0.35);
    const QColor label = mix(base, text, 0.65);
    const QFontMetrics fm(font());
    const Range y = yRange(panel);

    // ---- сетка: вертикальные линии времени ----
    const double step = timeStep((m_x1 - m_x0) / qMax(2.0, r.width() / 90.0));
    p.setPen(QPen(grid, 1));
    for (double t = std::ceil(m_x0 / step) * step; t <= m_x1; t += step) {
        const double px = xToPx(t, r);
        p.drawLine(QPointF(px, r.top()), QPointF(px, r.bottom()));
    }

    // ---- сетка Y и подписи ----
    auto yLine = [&](double v, const QString &s) {
        const double py = yToPx(v, y, panel, r);
        if (py < r.top() - 1 || py > r.bottom() + 1)
            return;
        p.setPen(QPen(grid, 1));
        p.drawLine(QPointF(r.left(), py), QPointF(r.right(), py));
        p.setPen(label);
        p.drawText(QPointF(r.left() - 6 - fm.horizontalAdvance(s), py + fm.ascent() / 2.0 - 1), s);
    };
    if (panel.log) {
        // меньше трёх декад — ещё 2 и 5 внутри декады, иначе подписей мало
        const bool fine = y.hi - y.lo <= 2.5;
        for (int k = int(std::floor(y.lo)); k <= int(std::ceil(y.hi)); ++k)
            for (double m : {1.0, 2.0, 5.0}) {
                if (m > 1 && !fine)
                    continue;
                const double v = m * std::pow(10.0, k);
                yLine(v, QLocale().toString(v, 'g', 3));
            }
    } else {
        const double ys = niceStep((y.hi - y.lo) / qMax(2.0, r.height() / 32.0));
        for (double v = std::ceil(y.lo / ys) * ys; v <= y.hi + ys * 1e-6; v += ys)
            yLine(v, axisNumber(v, ys));
    }

    p.setPen(QPen(axis, 1));
    p.drawLine(r.bottomLeft(), r.bottomRight());
    p.drawLine(r.bottomLeft(), r.topLeft());

    // ---- пороги ----
    for (const Level &l : panel.levels) {
        const double py = yToPx(l.value, y, panel, r);
        if (py < r.top() || py > r.bottom())
            continue;
        QPen pen(l.color, 1.2, Qt::DashLine);
        p.setPen(pen);
        p.drawLine(QPointF(r.left(), py), QPointF(r.right(), py));
        p.setPen(l.color);
        // подпись над линией, а у верхнего края панели — под ней
        const double ty = py - fm.descent() - 3 < r.top() + fm.ascent() ? py + fm.ascent() + 2 : py - 3;
        p.drawText(QPointF(r.right() - fm.horizontalAdvance(l.name) - 4, ty), l.name);
    }

    // ---- линии ----
    p.save();
    p.setClipRect(r.adjusted(0, -2, 0, 2));
    for (const Series &s : panel.series) {
        if (s.points.isEmpty())
            continue;
        QPainterPath path;
        bool open = false;
        double prevT = 0;
        int shown = 0;
        for (const QPointF &pt : s.points) {
            if (panel.log && pt.y() <= 0) {
                open = false;
                continue;
            }
            const QPointF px(xToPx(pt.x(), r), yToPx(pt.y(), y, panel, r));
            if (!open || pt.x() - prevT > m_gapMs)
                path.moveTo(px);
            else
                path.lineTo(px);
            open = true;
            prevT = pt.x();
            if (pt.x() >= m_x0 && pt.x() <= m_x1)
                ++shown;
        }
        p.setPen(QPen(s.color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        // точки — если их немного в окне (видно, где измерения)
        if (shown > 0 && r.width() / shown >= 6) {
            p.setPen(Qt::NoPen);
            p.setBrush(s.color);
            for (const QPointF &pt : s.points) {
                if (pt.x() < m_x0 || pt.x() > m_x1 || (panel.log && pt.y() <= 0))
                    continue;
                p.drawEllipse(QPointF(xToPx(pt.x(), r), yToPx(pt.y(), y, panel, r)), 2.4, 2.4);
            }
        }
    }
    p.restore();

    // ---- заголовок панели и легенда ----
    QFont bf = font();
    bf.setBold(true);
    p.setFont(bf);
    const QFontMetrics bfm(bf);
    QString title = panel.title;
    double x = r.left() + 8;
    const double ty = r.top() + bfm.ascent() + 4;
    QColor bg = base;
    bg.setAlpha(200);
    // подложка, чтобы заголовок читался поверх линий
    double w = bfm.horizontalAdvance(title) + 8;
    int withData = 0;
    for (const Series &s : panel.series)
        withData += s.points.isEmpty() ? 0 : 1;
    const bool legend = withData > 1;           // легенда — когда в панели больше одной линии с данными
    if (legend)
        for (const Series &s : panel.series)
            if (!s.points.isEmpty())
                w += 22 + fm.horizontalAdvance(s.name);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(x - 4, r.top() + 2, w + 4, bfm.height() + 4), 4, 4);
    p.setPen(text);
    p.drawText(QPointF(x, ty), title);
    x += bfm.horizontalAdvance(title) + 12;
    p.setFont(font());
    if (legend) {
        for (const Series &s : panel.series) {
            if (s.points.isEmpty())
                continue;
            p.setPen(QPen(s.color, 2.4));
            p.drawLine(QPointF(x, ty - fm.ascent() / 2.5), QPointF(x + 14, ty - fm.ascent() / 2.5));
            p.setPen(label);
            p.drawText(QPointF(x + 18, ty), s.name);
            x += 22 + fm.horizontalAdvance(s.name);
        }
    }
}

void TimeSeriesPlot::drawTimeAxis(QPainter &p, const QRectF &r)
{
    const QColor label = mix(palette().color(QPalette::Base), palette().color(QPalette::Text), 0.65);
    const QFontMetrics fm(font());
    const double step = timeStep((m_x1 - m_x0) / qMax(2.0, r.width() / 90.0));
    const QString fmt = step < 60e3 ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm");
    p.setPen(label);
    for (double t = std::ceil(m_x0 / step) * step; t <= m_x1; t += step) {
        const double px = xToPx(t, r);
        const QString s = QDateTime::fromMSecsSinceEpoch(qint64(t)).toLocalTime().toString(fmt);
        p.drawText(QPointF(px - fm.horizontalAdvance(s) / 2.0, r.bottom() + fm.ascent() + 5), s);
    }
}

void TimeSeriesPlot::drawHover(QPainter &p, const QVector<int> &order)
{
    const QRectF first = panelRect(0, order.size());
    const QRectF last = panelRect(order.size() - 1, order.size());
    if (m_drag || m_mouse.x() < first.left() || m_mouse.x() > first.right()
        || m_mouse.y() < first.top() || m_mouse.y() > last.bottom())
        return;

    const double t = pxToX(m_mouse.x(), first);
    const QColor text = palette().color(QPalette::Text);
    const QColor base = palette().color(QPalette::Base);
    const QColor cross = mix(base, text, 0.55);

    p.setPen(QPen(cross, 1, Qt::DashLine));
    p.drawLine(QPointF(m_mouse.x(), first.top()), QPointF(m_mouse.x(), last.bottom()));

    // время — на оси
    const QString ts = QDateTime::fromMSecsSinceEpoch(qint64(t)).toLocalTime().toString("HH:mm:ss");
    const QFontMetrics fm(font());
    drawBadge(p, QPointF(m_mouse.x() - (fm.horizontalAdvance(ts) + 10) / 2.0, last.bottom() + fm.height() + 6),
              ts, mix(base, text, 0.8), false);

    // значения: ближайшая по времени точка каждой линии
    for (int i = 0; i < order.size(); ++i) {
        const Panel &panel = m_panels[order[i]];
        const QRectF r = panelRect(i, order.size());
        const Range y = yRange(panel);
        double prevBottom = -1e9;                       // значения линий одной панели — не внахлёст
        for (const Series &s : panel.series) {
            if (s.points.isEmpty())
                continue;
            auto it = std::lower_bound(s.points.cbegin(), s.points.cend(), t,
                                       [](const QPointF &a, double v) { return a.x() < v; });
            int k = int(it - s.points.cbegin());
            if (k >= s.points.size() || (k > 0 && t - s.points[k - 1].x() < s.points[k].x() - t))
                --k;
            const QPointF pt = s.points[k];
            if (std::abs(xToPx(pt.x(), r) - m_mouse.x()) > 40)
                continue;                               // далеко от курсора — точки нет
            const QPointF px(xToPx(pt.x(), r), yToPx(pt.y(), y, panel, r));
            p.setPen(QPen(base, 1.5));
            p.setBrush(s.color);
            p.drawEllipse(px, 4, 4);

            QString label = number(pt.y(), panel.decimals);
            if (!panel.unit.isEmpty())
                label += QLatin1Char(' ') + panel.unit;
            if (panel.series.size() > 1)
                label = s.name + QStringLiteral(": ") + label;
            // подпись — рядом с точкой, по ту сторону линии, где больше места; в пределах панели
            const bool left = m_mouse.x() > r.center().x();
            const double h = fm.height() + 4;
            double bottom = qBound(r.top() + h, px.y() + h / 2, r.bottom());
            if (bottom - h < prevBottom + 2)
                bottom = prevBottom + 2 + h;
            prevBottom = bottom;
            drawBadge(p, QPointF(m_mouse.x() + (left ? -10 : 10), bottom), label, s.color, left);
        }
    }
}

void TimeSeriesPlot::drawBadge(QPainter &p, const QPointF &anchor, const QString &text,
                               const QColor &bg, bool left)
{
    const QFontMetrics fm(font());
    const QSizeF sz(fm.horizontalAdvance(text) + 10, fm.height() + 4);
    QRectF br(QPointF(0, 0), sz);
    if (left)
        br.moveTopRight(anchor - QPointF(0, sz.height()));
    else
        br.moveTopLeft(anchor - QPointF(0, sz.height()));
    if (br.right() > width() - 1)   br.moveRight(width() - 1);
    if (br.left() < 1)              br.moveLeft(1);
    if (br.top() < 1)               br.moveTop(1);
    if (br.bottom() > height() - 1) br.moveBottom(height() - 1);

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(br, 4, 4);
    p.setPen(bg.lightnessF() > 0.6 ? QColor(20, 20, 20) : QColor(255, 255, 255));
    p.drawText(br, Qt::AlignCenter, text);
}

/////////////////////////////////// Мышь ///////////////////////////////////

void TimeSeriesPlot::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && hasData()) {
        m_drag = true;
        m_dragFrom = e->pos();
        m_dragX0 = m_x0;
        m_dragX1 = m_x1;
        setCursor(Qt::ClosedHandCursor);
    }
}

void TimeSeriesPlot::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->pos();
    if (m_drag) {
        const QRectF r = panelRect(0, 1);
        const double dt = (e->pos().x() - m_dragFrom.x()) / r.width() * (m_dragX1 - m_dragX0);
        m_x0 = m_dragX0 - dt;
        m_x1 = m_dragX1 - dt;
        clampX();
    }
    update();
}

void TimeSeriesPlot::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = false;
    unsetCursor();
    update();
}

void TimeSeriesPlot::mouseDoubleClickEvent(QMouseEvent *)
{
    resetZoom();
}

void TimeSeriesPlot::wheelEvent(QWheelEvent *e)
{
    if (!hasData())
        return;
    const QRectF r = panelRect(0, 1);
    const double at = pxToX(e->position().x(), r);
    const double k = e->angleDelta().y() > 0 ? 0.8 : 1.25;
    m_x0 = at - (at - m_x0) * k;
    m_x1 = at + (m_x1 - at) * k;
    clampX();
    update();
}

void TimeSeriesPlot::leaveEvent(QEvent *)
{
    m_mouse = QPoint(-1, -1);
    update();
}
