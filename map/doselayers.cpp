#include "doselayers.h"
#include "mapwidget.h"          // formatMapPoint, m_core

#include <QApplication>
#include <QClipboard>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainterPath>
#include <QtMath>
#include <QDebug>
#include <algorithm>
#include <functional>
#include <limits>
#include <cmath>
#include <cstring>
#include <utility>

extern mapcore m_core;

// ======================================================================
// dosemap
// ======================================================================

namespace dosemap {

QPointF mercNorm(double lat, double lon)
{
    // та же формула, что mapcore::longlatt2map_xy, только без умножения на размер мира
    double s = std::sin(lat * M_PI / 180.0);
    s = qBound(-0.9999, s, 0.9999);
    return QPointF((lon + 180.0) / 360.0,
                   0.5 - std::log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI));
}

void mercInverse(const QPointF &n, double &lat, double &lon)
{
    lon = n.x() * 360.0 - 180.0;
    lat = std::atan(std::sinh(M_PI * (1.0 - 2.0 * n.y()))) * 180.0 / M_PI;
}

View currentView()
{
    View v;
    v.zoom = m_core.getzoom();
    v.world = 256.0 * double(1LL << v.zoom);

    // Смещение экрана = мировые пиксели центра - экранные пиксели центра.
    // Обе функции округляют одинаково, поэтому разница точная.
    const smappoint c = m_core.getview();
    u_long mx = 0, my = 0, sx = 0, sy = 0;
    m_core.longlatt2map_xy(mx, my, c);
    m_core.longlatt2screen_xy(sx, sy, c);
    v.offset = QPointF(double(mx - sx), double(my - sy));
    return v;
}

QString formatDose(float v)
{
    if (!std::isfinite(v))
        return QStringLiteral("—");
    const float a = std::fabs(v);
    const int dec = a < 0.1f ? 3 : a < 10.0f ? 2 : a < 100.0f ? 1 : 0;
    return QString::number(double(v), 'f', dec);
}

} // namespace dosemap

namespace {

// Цвета текста по уровню (тёмные оттенки StatusColor - читаются на белом)
QColor levelTextColor(DoseColorScale::Level l)
{
    switch (l) {
    case DoseColorScale::Alarm:   return QColor(0xB4, 0x23, 0x2A);
    case DoseColorScale::Warning: return QColor(0x8A, 0x5A, 0x00);
    default:                      return QColor(0x2B, 0x36, 0x40);
    }
}

const QColor kMuted(0x8A, 0x96, 0xA3);
const QColor kInfo(0x3D, 0x8B, 0xFD);

QFont mapFont(const QFont &base, qreal pt, bool bold = false)
{
    QFont f = base;
    f.setPointSizeF(pt);
    f.setBold(bold);
    return f;
}

// Карточка в стиле плашек карты: белый фон, цветная полоска слева
void drawCardBox(QPainter &p, const QPointF &anchor, const QString &title, const QColor &titleColor,
                 const QColor &accent, const QStringList &lines, const QString &warnLine,
                 const QRect &bounds)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QFont ft = mapFont(p.font(), 10, true);
    const QFont fl = mapFont(p.font(), 8.5);
    const QFontMetricsF mt(ft), ml(fl);

    qreal w = mt.horizontalAdvance(title);
    for (const QString &l : lines)
        w = qMax(w, ml.horizontalAdvance(l));
    if (!warnLine.isEmpty())
        w = qMax(w, ml.horizontalAdvance(warnLine));

    const qreal pad = 8, bar = 3;
    const qreal h = pad * 2 + mt.height() + 2 + ml.height() * (lines.size() + (warnLine.isEmpty() ? 0 : 1));
    QRectF r(0, 0, w + pad * 2 + bar, h);

    r.moveTopLeft(anchor + QPointF(14, 10));
    if (r.right() > bounds.right() - 4)
        r.moveRight(anchor.x() - 14);
    if (r.bottom() > bounds.bottom() - 4)
        r.moveBottom(anchor.y() - 10);
    if (r.left() < bounds.left() + 4)
        r.moveLeft(bounds.left() + 4);
    if (r.top() < bounds.top() + 4)
        r.moveTop(bounds.top() + 4);

    p.setPen(QPen(QColor(0, 0, 0, 45), 1));
    p.setBrush(QColor(255, 255, 255, 246));
    p.drawRoundedRect(r, 6, 6);

    QPainterPath clip;
    clip.addRoundedRect(r, 6, 6);
    p.setClipPath(clip);
    p.fillRect(QRectF(r.left(), r.top(), bar, r.height()), accent);
    p.setClipping(false);

    qreal x = r.left() + bar + pad;
    qreal y = r.top() + pad + mt.ascent();
    p.setFont(ft);
    p.setPen(titleColor);
    p.drawText(QPointF(x, y), title);

    y += mt.descent() + 2 + ml.ascent();
    p.setFont(fl);
    p.setPen(QColor(0x4A, 0x55, 0x60));
    for (const QString &l : lines) {
        p.drawText(QPointF(x, y), l);
        y += ml.height();
    }
    if (!warnLine.isEmpty()) {
        p.setPen(levelTextColor(DoseColorScale::Warning));
        p.drawText(QPointF(x, y), warnLine);
    }
    p.restore();
}

// Пилюля с текстом (подписи у точек, "щуп" сетки)
void drawPill(QPainter &p, const QRectF &r, const QString &text, const QColor &textColor,
              const QColor &border, bool bold)
{
    p.setPen(QPen(border, 1));
    p.setBrush(QColor(255, 255, 255, 236));
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
    QFont f = p.font();
    f.setBold(bold);
    p.setFont(f);
    p.setPen(textColor);
    p.drawText(r, Qt::AlignCenter, text);
}

QString flagsText(quint32 flags)
{
    QStringList s;
    if (flags & MfOutOfArea)    s << "поза областю";
    if (flags & MfLidarRange)   s << "висота поза діапазоном";
    if (flags & MfBdbgStat)     s << "стан БДБГ";
    if (flags & MfDeviceFault)  s << "відмова приладу";
    return s.join(", ");
}

} // namespace

// ======================================================================
// DoseColorScale
// ======================================================================

DoseColorScale::DoseColorScale()
{
    rebuild();
}

void DoseColorScale::setThresholds(float warn, float alarm)
{
    if (!(warn > 0.0f))
        warn = 0.3f;
    if (!(alarm > warn))
        alarm = warn * 3.0f;
    if (qFuzzyCompare(warn, m_warn) && qFuzzyCompare(alarm, m_alarm))
        return;
    m_warn = warn;
    m_alarm = alarm;
    rebuild();
}

double DoseColorScale::position(float v) const
{
    if (!(v > 0.0f))
        return 0.0;
    const double t = (std::log(double(v)) - m_logLo) * m_invLogRange;
    return qBound(0.0, t, 1.0);
}

int DoseColorScale::lutIndex(float v) const
{
    return qBound(0, int(position(v) * 255.0 + 0.5), 255);
}

DoseColorScale::Level DoseColorScale::level(float v) const
{
    if (v >= m_alarm) return Alarm;
    if (v >= m_warn)  return Warning;
    return Normal;
}

void DoseColorScale::rebuild()
{
    m_lo = double(m_warn) / 4.0;
    m_hi = double(m_alarm) * 10.0;
    m_logLo = std::log(m_lo);
    m_invLogRange = 1.0 / (std::log(m_hi) - m_logLo);

    // Опорные цвета привязаны к порогам: зелёный (фон) -> жёлтый (предупреждение)
    // -> красный (тревога) -> тёмно-сливовый (на порядок выше тревоги).
    struct Stop { double v; QColor c; };
    const Stop stops[] = {
        { m_lo,                                QColor(0x3D, 0xAA, 0x72) },
        { std::sqrt(m_lo * m_warn),            QColor(0xA4, 0xC6, 0x4A) },
        { double(m_warn),                      QColor(0xF2, 0xC2, 0x30) },
        { std::sqrt(double(m_warn) * m_alarm), QColor(0xEE, 0x8A, 0x2A) },
        { double(m_alarm),                     QColor(0xE5, 0x48, 0x4D) },
        { std::sqrt(double(m_alarm) * m_hi),   QColor(0xB0, 0x28, 0x5E) },
        { m_hi,                                QColor(0x6E, 0x1E, 0x6E) },
    };
    const int n = int(sizeof(stops) / sizeof(stops[0]));
    const double l0 = std::log(m_lo), l1 = std::log(m_hi);

    m_lut.resize(256);
    for (int i = 0; i < 256; ++i) {
        const double t = i / 255.0;
        int k = 0;
        while (k < n - 2 && (std::log(stops[k + 1].v) - l0) / (l1 - l0) < t)
            ++k;
        const double t0 = (std::log(stops[k].v) - l0) / (l1 - l0);
        const double t1 = (std::log(stops[k + 1].v) - l0) / (l1 - l0);
        const double f = qBound(0.0, (t - t0) / qMax(1e-9, t1 - t0), 1.0);
        const QColor &a = stops[k].c, &b = stops[k + 1].c;
        m_lut[i] = qRgb(int(a.red()   + (b.red()   - a.red())   * f),
                        int(a.green() + (b.green() - a.green()) * f),
                        int(a.blue()  + (b.blue()  - a.blue())  * f));
    }
    ++m_version;
}

// ======================================================================
// MeasurementLayer
// ======================================================================

MeasurementLayer::MeasurementLayer(const DoseColorScale *scale, QObject *parent)
    : MapLayer(parent), m_scale(scale)
{
}

MeasurementLayer::Point MeasurementLayer::makePoint(const Measurement &m)
{
    Point pt;
    pt.n = dosemap::mercNorm(m.lat, m.lon);
    pt.lat = m.lat;
    pt.lon = m.lon;
    pt.value = m.shownPaed();
    pt.paed = m.paed;
    pt.paed1m = m.paed1m;
    pt.has1m = m.hasPaed1m;
    pt.lidarCm = m.lidarCm;
    pt.flags = m.flags;
    pt.time = m.time;
    pt.id = m.id;
    return pt;
}

void MeasurementLayer::appendPoint(const Point &pt)
{
    m_points.append(pt);
    const int idx = m_points.size() - 1;
    if (pt.reliable() && (m_maxIndex < 0 || pt.value > m_points[m_maxIndex].value))
        m_maxIndex = idx;
    m_orderDirty = true;
}

void MeasurementLayer::addMeasurement(const Measurement &m)
{
    const Point pt = makePoint(m);
    appendPoint(pt);

    // Живой полёт: точку дорисовываем прямо в кэш, без перестройки
    if (m_cacheValid && m_spriteDpr > 0) {
        QPainter cp(&m_cache);
        paintPointIntoCache(cp, pt, 256.0 * double(1LL << m_cacheZoom));
    }
    emit updateRequested();
}

void MeasurementLayer::setMeasurements(const QVector<Measurement> &list)
{
    m_points.clear();
    m_points.reserve(list.size());
    m_maxIndex = -1;
    m_hover = m_selected = -1;
    for (const Measurement &m : list)
        appendPoint(makePoint(m));
    m_cacheValid = false;
    emit updateRequested();
}

void MeasurementLayer::clear()
{
    m_points.clear();
    m_ascending.clear();
    m_labelOrder.clear();
    m_maxIndex = m_hover = m_selected = -1;
    m_cacheValid = false;
    m_cache = QImage();
    emit updateRequested();
}

void MeasurementLayer::selectAndCenter(qint64 measurementId)
{
    for (int i = 0; i < m_points.size(); ++i) {
        if (m_points[i].id != measurementId)
            continue;
        m_selected = i;
        emit centerRequested(m_points[i].lat, m_points[i].lon);
        emit updateRequested();
        return;
    }
}

void MeasurementLayer::selectIndexAndCenter(int index)
{
    if (index < 0 || index >= m_points.size())
        return;
    m_selected = index;
    emit centerRequested(m_points[index].lat, m_points[index].lon);
    emit updateRequested();
}

// ------------------------ ОТРИСОВКА ------------------------ //

qreal MeasurementLayer::dotRadius(int zoom)
{
    // z16: 3.5 px, z14-15: 2.5 px, мельче - 2 px
    return zoom >= 16 ? 3.5 : zoom >= 14 ? 2.5 : 2.0;
}

void MeasurementLayer::ensureSprites(qreal dpr, qreal radius)
{
    if (m_spriteDpr == dpr && m_spriteR == radius && m_spriteScale == m_scale->version() &&
        !m_sprites.isEmpty())
        return;

    const qreal half = radius + 2.5;
    const int px = int(std::ceil(half * 2 * dpr));

    auto make = [&](const QColor &fill, bool hollow) {
        QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(Qt::transparent);
        QPainter sp(&img);
        sp.setRenderHint(QPainter::Antialiasing);
        const QPointF c(half, half);
        if (hollow) {
            sp.setBrush(Qt::NoBrush);
            sp.setPen(QPen(QColor(255, 255, 255, 200), 3));
            sp.drawEllipse(c, radius - 0.3, radius - 0.3);
            sp.setPen(QPen(fill, 1.4));
            sp.drawEllipse(c, radius - 0.3, radius - 0.3);
        } else {
            sp.setPen(QPen(QColor(255, 255, 255, 235), radius >= 3 ? 1.5 : 1.0));
            sp.setBrush(fill);
            sp.drawEllipse(c, radius, radius);
        }
        return img;
    };

    m_sprites.resize(64);
    const QVector<QRgb> &lut = m_scale->lut();
    for (int k = 0; k < 64; ++k)
        m_sprites[k] = make(QColor(lut[k * 4 + 2]), false);
    m_badSprite = make(kMuted, true);

    m_spriteDpr = dpr;
    m_spriteR = radius;
    m_spriteScale = m_scale->version();
    m_cacheValid = false;
}

const QImage &MeasurementLayer::spriteFor(const Point &pt) const
{
    if (!pt.reliable())
        return m_badSprite;
    return m_sprites[m_scale->lutIndex(pt.value) >> 2];
}

void MeasurementLayer::paintPointIntoCache(QPainter &cp, const Point &pt, double world) const
{
    const QPointF w = pt.n * world - QPointF(m_cacheWorld.topLeft());
    const qreal half = m_spriteR + 2.5;
    if (w.x() < -half || w.y() < -half ||
        w.x() > m_cacheWorld.width() + half || w.y() > m_cacheWorld.height() + half)
        return;
    cp.drawImage(QPointF(std::floor(w.x() - half + 0.5), std::floor(w.y() - half + 0.5)), spriteFor(pt));
}

void MeasurementLayer::ensureCache(const dosemap::View &v, const QSize &screen, qreal dpr)
{
    const QRect view(QPoint(int(std::floor(v.offset.x())), int(std::floor(v.offset.y()))), screen);
    if (m_cacheValid && m_cacheZoom == v.zoom && m_cacheDpr == dpr &&
        m_cacheScale == m_scale->version() && m_cacheWorld.contains(view))
        return;

    m_cacheWorld = view.adjusted(-CACHE_MARGIN, -CACHE_MARGIN, CACHE_MARGIN, CACHE_MARGIN);
    m_cache = QImage(m_cacheWorld.size() * dpr, QImage::Format_ARGB32_Premultiplied);
    m_cache.setDevicePixelRatio(dpr);
    m_cache.fill(Qt::transparent);

    // Прореживание: идём от больших значений к меньшим, в ячейке экранной сетки
    // оставляем первую (бОльшую) точку. Рисуем потом в обратном порядке - пики сверху.
    const qreal cell = qMax(2.0, m_spriteR * 1.5);
    const int gw = int(m_cacheWorld.width() / cell) + 1;
    const int gh = int(m_cacheWorld.height() / cell) + 1;
    QVector<quint8> occupied(gw * gh, 0);
    QVector<int> keep;
    keep.reserve(m_ascending.size());
    for (int i = m_ascending.size() - 1; i >= 0; --i) {
        const int idx = m_ascending[i];
        const QPointF w = m_points[idx].n * v.world - QPointF(m_cacheWorld.topLeft());
        const int gx = int(std::floor(w.x() / cell)), gy = int(std::floor(w.y() / cell));
        if (gx < 0 || gy < 0 || gx >= gw || gy >= gh)
            continue;
        quint8 &o = occupied[gy * gw + gx];
        if (o)
            continue;
        o = 1;
        keep.append(idx);
    }

    QPainter cp(&m_cache);
    for (int i = keep.size() - 1; i >= 0; --i)
        paintPointIntoCache(cp, m_points[keep[i]], v.world);
    cp.end();

    m_cacheZoom = v.zoom;
    m_cacheDpr = dpr;
    m_cacheScale = m_scale->version();
    m_cacheValid = true;
}

void MeasurementLayer::draw(QPainter &p)
{
    if (m_points.isEmpty())
        return;

    if (m_orderDirty) {
        // Отрисовка: недостоверные внизу, достоверные по возрастанию - пики сверху
        m_ascending.resize(m_points.size());
        for (int i = 0; i < m_points.size(); ++i)
            m_ascending[i] = i;
        std::sort(m_ascending.begin(), m_ascending.end(), [this](int a, int b) {
            const Point &pa = m_points[a], &pb = m_points[b];
            if (pa.reliable() != pb.reliable())
                return !pa.reliable();
            return pa.value < pb.value;
        });
        m_labelOrder.clear();
        for (int i = m_ascending.size() - 1; i >= 0; --i)
            if (m_points[m_ascending[i]].reliable())
                m_labelOrder.append(m_ascending[i]);
        m_orderDirty = false;
    }

    const dosemap::View v = dosemap::currentView();
    const QRect screen(0, 0, p.device()->width(), p.device()->height());
    const qreal dpr = p.device()->devicePixelRatioF();

    ensureSprites(dpr, dotRadius(v.zoom));
    ensureCache(v, screen.size(), dpr);
    p.drawImage(QPointF(m_cacheWorld.topLeft()) - v.offset, m_cache);

    p.setRenderHint(QPainter::Antialiasing);
    if (m_labels)
        drawLabels(p, v, screen);

    if (m_selected >= 0)
        drawMarker(p, v.toScreen(m_points[m_selected].n), m_points[m_selected], true);
    if (m_hover >= 0 && m_hover != m_selected)
        drawMarker(p, v.toScreen(m_points[m_hover].n), m_points[m_hover], false);

}

void MeasurementLayer::drawCardOverlay(QPainter &p)
{
    const int card = m_hover >= 0 ? m_hover : m_selected;
    if (!isVisible() || card < 0 || card >= m_points.size())
        return;
    const dosemap::View v = dosemap::currentView();
    const QRect screen(0, 0, p.device()->width(), p.device()->height());
    drawCard(p, v.toScreen(m_points[card].n), m_points[card], card, screen);
}

// Жадная раскладка подписей: от больших значений к меньшим, 4 места вокруг точки,
// пропуск при наложении. Чем крупнее зум - тем больше подписей помещается.
void MeasurementLayer::drawLabels(QPainter &p, const dosemap::View &v, const QRect &screen)
{
    const QFont font = mapFont(p.font(), 8);
    const QFontMetricsF fm(font);
    p.setFont(font);

    constexpr int BUCKET = 64;
    const int bc = screen.width() / BUCKET + 1;
    const int br = screen.height() / BUCKET + 1;
    QVector<QVector<QRectF>> buckets(bc * br);

    auto forCells = [&](const QRectF &r, const std::function<bool(QVector<QRectF> &)> &fn) {
        const int c0 = qBound(0, int(r.left()) / BUCKET, bc - 1);
        const int c1 = qBound(0, int(r.right()) / BUCKET, bc - 1);
        const int r0 = qBound(0, int(r.top()) / BUCKET, br - 1);
        const int r1 = qBound(0, int(r.bottom()) / BUCKET, br - 1);
        for (int y = r0; y <= r1; ++y)
            for (int x = c0; x <= c1; ++x)
                if (fn(buckets[y * bc + x]))
                    return true;
        return false;
    };
    auto collides = [&](const QRectF &r) {
        if (r.left() < 0 || r.top() < 0 || r.right() > screen.width() || r.bottom() > screen.height())
            return true;
        return forCells(r, [&](QVector<QRectF> &list) {
            for (const QRectF &o : list)
                if (o.intersects(r))
                    return true;
            return false;
        });
    };
    auto reserve = [&](const QRectF &r) {
        forCells(r, [&](QVector<QRectF> &list) { list.append(r); return false; });
    };

    // Карточка выбранной/наведённой точки важнее подписей
    const int card = m_hover >= 0 ? m_hover : m_selected;
    if (card >= 0) {
        const QPointF s = v.toScreen(m_points[card].n);
        reserve(QRectF(s.x() - 12, s.y() - 12, 24, 24));
    }
    for (const QPointF &n : std::as_const(m_avoid)) {
        const QPointF s = v.toScreen(n);
        reserve(QRectF(s.x() - 15, s.y() - 15, 72, 30));   // мишень + её подпись справа
    }

    const qreal th = fm.height() + 2;
    const qreal R = m_spriteR;
    const qreal gap = R + 3;
    QVector<QPointF> anchors;   // уже подписанные точки
    auto tooClose = [&](const QPointF &s) {
        for (const QPointF &a : std::as_const(anchors)) {
            const QPointF d = a - s;
            if (d.x() * d.x() + d.y() * d.y() < LABEL_SPACING * LABEL_SPACING)
                return true;
        }
        return false;
    };
    const QRectF visible = QRectF(screen).adjusted(-10, -10, 10, 10);
    int placed = 0;

    for (int idx : std::as_const(m_labelOrder)) {
        if (idx == card)
            continue;
        const Point &pt = m_points[idx];
        const QPointF s = v.toScreen(pt.n);
        if (!visible.contains(s) || tooClose(s))
            continue;

        const QString text = dosemap::formatDose(pt.value);
        const qreal tw = fm.horizontalAdvance(text) + 10;
        const QRectF cand[4] = {
            QRectF(s.x() + gap,           s.y() - th / 2,   tw, th),   // справа
            QRectF(s.x() - gap - tw,      s.y() - th / 2,   tw, th),   // слева
            QRectF(s.x() - tw / 2,        s.y() - gap - th, tw, th),   // сверху
            QRectF(s.x() - tw / 2,        s.y() + gap,      tw, th),   // снизу
        };
        for (const QRectF &r : cand) {
            if (collides(r.adjusted(-2, -2, 2, 2)))
                continue;
            const DoseColorScale::Level lvl = m_scale->level(pt.value);
            drawPill(p, r, text, levelTextColor(lvl), m_scale->color(pt.value),
                     lvl != DoseColorScale::Normal);
            p.setFont(font);
            reserve(r);
            reserve(QRectF(s.x() - R - 1, s.y() - R - 1, 2 * R + 2, 2 * R + 2));
            anchors.append(s);
            ++placed;
            break;
        }
        if (placed >= MAX_LABELS)
            break;
    }
}

void MeasurementLayer::drawMarker(QPainter &p, const QPointF &s, const Point &pt, bool selected)
{
    Q_UNUSED(pt)
    p.setBrush(Qt::NoBrush);
    const qreal r = m_spriteR + (selected ? 4.5 : 3.5);
    p.setPen(QPen(QColor(255, 255, 255, 230), 4));
    p.drawEllipse(s, r, r);
    p.setPen(QPen(selected ? kInfo : QColor(0x2B, 0x36, 0x40), 2));
    p.drawEllipse(s, r, r);
}

void MeasurementLayer::drawCard(QPainter &p, const QPointF &s, const Point &pt, int index,
                                const QRect &bounds)
{
    const DoseColorScale::Level lvl = m_scale->level(pt.value);
    const QString title = dosemap::formatDose(pt.value) + " мкЗв/год";

    QStringList lines;
    if (pt.has1m)
        lines << QString("На 1 м: %1 · на висоті: %2")
                     .arg(dosemap::formatDose(pt.paed1m), dosemap::formatDose(pt.paed));
    else
        lines << "Без перерахунку на 1 м";
    if (pt.lidarCm > 0)
        lines << QString("Висота: %1 м").arg(pt.lidarCm / 100.0, 0, 'f', 1);
    if (pt.time.isValid())
        lines << QString("Час: %1").arg(pt.time.toString("HH:mm:ss"));
    smappoint gp;
    gp.latt = pt.lat;
    gp.longt = pt.lon;
    lines << formatMapPoint(gp, m_coordType);
    lines << QString("Вимір #%1").arg(pt.id > 0 ? pt.id : index + 1);

    const QString warn = pt.reliable() ? QString()
                                       : QString("Недостовірна: %1").arg(flagsText(pt.flags));
    const QColor accent = pt.reliable() ? m_scale->color(pt.value) : kMuted;
    drawCardBox(p, s, title, pt.reliable() ? levelTextColor(lvl) : kMuted.darker(130),
                accent, lines, warn, bounds);
}

// ------------------------ СОБЫТИЯ ------------------------ //

int MeasurementLayer::hitTest(const QPoint &pos, const dosemap::View &v) const
{
    // Линейный проход: 20 тыс. точек - доли миллисекунды
    const QPointF n = v.toNorm(QPointF(pos));
    const double r = HIT_R / v.world;
    const double r2 = r * r;
    int best = -1;
    double bestD = r2;
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF d = m_points[i].n - n;
        const double d2 = d.x() * d.x() + d.y() * d.y();
        if (d2 > r2)
            continue;
        // при равных расстояниях - точка с большим значением (она нарисована сверху)
        if (best < 0 || d2 < bestD * 0.8 ||
            (d2 <= bestD * 1.25 && m_points[i].value > m_points[best].value)) {
            best = i;
            bestD = d2;
        }
    }
    return best;
}

bool MeasurementLayer::mouseMove(QMouseEvent *e)
{
    int h = -1;
    if (e->buttons() == Qt::NoButton && !m_points.isEmpty())
        h = hitTest(e->pos(), dosemap::currentView());
    if (h != m_hover) {
        m_hover = h;
        emit updateRequested();
    }
    return false;   // перетаскивание карты не блокируем
}

void MeasurementLayer::mouseLeave()
{
    if (m_hover >= 0) {
        m_hover = -1;
        emit updateRequested();
    }
}

bool MeasurementLayer::mouseRelease(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || m_points.isEmpty())
        return false;

    const int h = hitTest(e->pos(), dosemap::currentView());
    if (h >= 0) {
        m_selected = (h == m_selected) ? -1 : h;
        if (m_selected >= 0)
            emit measurementSelected(m_points[h].id, h);
        emit updateRequested();
        return true;
    }
    if (m_selected >= 0) {
        m_selected = -1;
        emit updateRequested();
    }
    return false;
}

bool MeasurementLayer::keyPress(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && m_selected >= 0) {
        m_selected = -1;
        emit updateRequested();
        return true;
    }
    return false;
}

void MeasurementLayer::buildContextMenu(const QPoint &pos, QMenu &menu)
{
    const int h = hitTest(pos, dosemap::currentView());
    if (h < 0)
        return;
    const Point pt = m_points[h];

    menu.addAction(QString("Вимір: %1 мкЗв/год").arg(dosemap::formatDose(pt.value)))->setEnabled(false);
    menu.addAction("Скопіювати значення", [pt] {
        QApplication::clipboard()->setText(dosemap::formatDose(pt.value));
    });
    const CoordType type = m_coordType;
    menu.addAction("Скопіювати координати точки", [pt, type] {
        smappoint gp;
        gp.latt = pt.lat;
        gp.longt = pt.lon;
        QApplication::clipboard()->setText(formatMapPoint(gp, type));
    });
    menu.addAction("Центрувати на точці", this, [this, pt] { emit centerRequested(pt.lat, pt.lon); });
}

// ======================================================================
// DoseGridLayer
// ======================================================================

DoseGridLayer::DoseGridLayer(const DoseColorScale *scale, QObject *parent)
    : MapLayer(parent), m_scale(scale)
{
}

void DoseGridLayer::setGrid(const GridData &grid)
{
    // Пустая сетка от ядра (новый полёт, новая область) — просто очистить слой
    if (grid.cols <= 0 || grid.rows <= 0) {
        clear();
        return;
    }
    const qint64 cells = qint64(grid.cols) * grid.rows;
    if (grid.cols <= 0 || grid.rows <= 0 || grid.values.size() != cells ||
        (!grid.inArea.isEmpty() && grid.inArea.size() != cells) ||
        grid.cols > 8192 || grid.rows > 8192 ||
        !(grid.latMax > grid.latMin) || !(grid.lonMax > grid.lonMin)) {
        qWarning() << "DoseGridLayer: некорректная сетка" << grid.cols << "x" << grid.rows
                   << "значений" << grid.values.size();
        return;
    }
    m_grid = grid;
    m_imageDirty = true;
    m_probeCell = -1;
    emit updateRequested();
}

void DoseGridLayer::clear()
{
    m_grid = GridData();
    m_imageGrid = GridData();
    m_image = QImage();
    m_probeCell = -1;
    emit updateRequested();
}

void DoseGridLayer::rebuildImage()
{
    // Таблица цветов с альфой: фон прозрачнее, высокие значения насыщеннее
    QRgb lut[256];
    const QVector<QRgb> &base = m_scale->lut();
    for (int i = 0; i < 256; ++i) {
        const int a = int(255 * (0.34 + 0.40 * i / 255.0));
        lut[i] = qPremultiply(qRgba(qRed(base[i]), qGreen(base[i]), qBlue(base[i]), a));
    }
    const QRgb noData = qPremultiply(qRgba(0x78, 0x82, 0x8C, 30));   // в области, но не облетели

    const int cols = m_grid.cols, rows = m_grid.rows;
    const bool hasMask = !m_grid.inArea.isEmpty();

    // Частичное обновление: ядро шлёт всю сетку после каждого измерения, но меняются
    // только клетки в радиусе R от новой точки - неизменные строки пропускаем
    const GridData &old = m_imageGrid;
    const bool partial = !m_image.isNull() && m_imageScale == m_scale->version() &&
                         old.cols == cols && old.rows == rows &&
                         old.latMin == m_grid.latMin && old.latMax == m_grid.latMax &&
                         old.lonMin == m_grid.lonMin && old.lonMax == m_grid.lonMax &&
                         old.inArea.size() == m_grid.inArea.size();
    if (!partial)
        m_image = QImage(cols, rows, QImage::Format_ARGB32_Premultiplied);

    const float *vals = m_grid.values.constData();
    const quint8 *mask = hasMask ? m_grid.inArea.constData() : nullptr;
    const float *oldVals = partial ? old.values.constData() : nullptr;
    const quint8 *oldMask = (partial && hasMask) ? old.inArea.constData() : nullptr;
    const double logLo = m_scale->logLo(), k = m_scale->invLogRange() * 255.0;

    for (int r = 0; r < rows; ++r) {
        const int base0 = r * cols;
        if (partial && std::memcmp(vals + base0, oldVals + base0, sizeof(float) * cols) == 0 &&
            (!mask || std::memcmp(mask + base0, oldMask + base0, cols) == 0))
            continue;

        // в GridData строка 0 - юг, в картинке строка 0 - север
        QRgb *line = reinterpret_cast<QRgb *>(m_image.scanLine(rows - 1 - r));
        for (int c = 0; c < cols; ++c) {
            const int i = base0 + c;
            if (mask && !mask[i]) {
                line[c] = 0;
                continue;
            }
            const float v = vals[i];
            if (std::isnan(v)) {
                line[c] = noData;
            } else {
                const int li = v > 0.0f ? int((std::log(v) - logLo) * k + 0.5) : 0;
                line[c] = lut[li < 0 ? 0 : li > 255 ? 255 : li];
            }
        }
    }
    m_imageGrid = m_grid;     // неявное разделение данных, копии нет
    m_imageScale = m_scale->version();
    m_imageDirty = false;
}

float DoseGridLayer::valueAt(double lat, double lon, bool *inArea) const
{
    if (inArea)
        *inArea = false;
    if (!hasData() || lat < m_grid.latMin || lat >= m_grid.latMax ||
        lon < m_grid.lonMin || lon >= m_grid.lonMax)
        return std::numeric_limits<float>::quiet_NaN();
    const int r = int((lat - m_grid.latMin) / (m_grid.latMax - m_grid.latMin) * m_grid.rows);
    const int c = int((lon - m_grid.lonMin) / (m_grid.lonMax - m_grid.lonMin) * m_grid.cols);
    const int i = qBound(0, r, m_grid.rows - 1) * m_grid.cols + qBound(0, c, m_grid.cols - 1);
    if (!m_grid.inArea.isEmpty() && !m_grid.inArea[i])
        return std::numeric_limits<float>::quiet_NaN();
    if (inArea)
        *inArea = true;
    return m_grid.values[i];
}

int DoseGridLayer::cellAt(const QPointF &screen, const dosemap::View &v) const
{
    if (!hasData())
        return -1;
    double lat = 0, lon = 0;
    dosemap::mercInverse(v.toNorm(screen), lat, lon);
    if (lat < m_grid.latMin || lat >= m_grid.latMax || lon < m_grid.lonMin || lon >= m_grid.lonMax)
        return -1;
    const int r = qBound(0, int((lat - m_grid.latMin) / (m_grid.latMax - m_grid.latMin) * m_grid.rows),
                         m_grid.rows - 1);
    const int c = qBound(0, int((lon - m_grid.lonMin) / (m_grid.lonMax - m_grid.lonMin) * m_grid.cols),
                         m_grid.cols - 1);
    const int i = r * m_grid.cols + c;
    if (!m_grid.inArea.isEmpty() && !m_grid.inArea[i])
        return -1;
    return i;
}

QRectF DoseGridLayer::cellRect(int cell, const dosemap::View &v) const
{
    const int r = cell / m_grid.cols, c = cell % m_grid.cols;
    const double dLat = (m_grid.latMax - m_grid.latMin) / m_grid.rows;
    const double dLon = (m_grid.lonMax - m_grid.lonMin) / m_grid.cols;
    const QPointF tl = v.toScreen(dosemap::mercNorm(m_grid.latMin + (r + 1) * dLat, m_grid.lonMin + c * dLon));
    const QPointF br = v.toScreen(dosemap::mercNorm(m_grid.latMin + r * dLat, m_grid.lonMin + (c + 1) * dLon));
    return QRectF(tl, br);
}

void DoseGridLayer::draw(QPainter &p)
{
    if (!hasData())
        return;
    if (m_imageDirty || m_imageScale != m_scale->version())
        rebuildImage();

    const dosemap::View v = dosemap::currentView();
    const QRectF screen(0, 0, p.device()->width(), p.device()->height());

    const int cols = m_grid.cols, rows = m_grid.rows;
    const double x0 = v.toScreen(dosemap::mercNorm(m_grid.latMax, m_grid.lonMin)).x();
    const double x1 = v.toScreen(dosemap::mercNorm(m_grid.latMax, m_grid.lonMax)).x();
    const double yTop = v.toScreen(dosemap::mercNorm(m_grid.latMax, m_grid.lonMin)).y();
    const double yBot = v.toScreen(dosemap::mercNorm(m_grid.latMin, m_grid.lonMin)).y();
    if (x1 < 0 || x0 > screen.width() || yBot < 0 || yTop > screen.height())
        return;

    // Видимые столбцы
    const double cellW = (x1 - x0) / cols;
    const int c0 = qBound(0, int(std::floor((0 - x0) / cellW)), cols);
    const int c1 = qBound(0, int(std::ceil((screen.width() - x0) / cellW)), cols);
    if (c1 <= c0)
        return;

    // Мелкие клетки сглаживаем, крупные рисуем честными квадратами
    p.setRenderHint(QPainter::SmoothPixmapTransform, cellW < 6.0);
    p.setOpacity(m_opacity);

    // Полосы по 32 строки: в каждой своя (точная) Меркаторова привязка по широте
    constexpr int BAND = 32;
    const double dLat = (m_grid.latMax - m_grid.latMin) / rows;
    for (int ir0 = 0; ir0 < rows; ir0 += BAND) {
        const int ir1 = qMin(rows, ir0 + BAND);
        const double yt = v.toScreen(dosemap::mercNorm(m_grid.latMax - ir0 * dLat, m_grid.lonMin)).y();
        const double yb = v.toScreen(dosemap::mercNorm(m_grid.latMax - ir1 * dLat, m_grid.lonMin)).y();
        if (yb < 0)
            continue;
        if (yt > screen.height())
            break;
        const QRectF target(x0 + c0 * cellW, yt, (c1 - c0) * cellW, yb - yt);
        const QRectF source(c0, ir0, c1 - c0, ir1 - ir0);
        p.drawImage(target, m_image, source);
    }
    p.setOpacity(1.0);
    p.setRenderHint(QPainter::Antialiasing);

    // Граница области
    if (m_grid.area.size() >= 3) {
        QPolygonF poly;
        for (const GeoPoint &g : m_grid.area)
            poly << v.toScreen(dosemap::mercNorm(g.lat, g.lon));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0x2B, 0x36, 0x40, 150), 1, Qt::DashLine));
        p.drawPolygon(poly);
    }

}

bool DoseGridLayer::mouseMove(QMouseEvent *e)
{
    int cell = -1;
    if (e->buttons() == Qt::NoButton && !(m_points && m_points->isVisible() && m_points->hasHover()))
        cell = cellAt(QPointF(e->pos()), dosemap::currentView());
    if (cell != m_probeCell) {
        m_probeCell = cell;
        emit updateRequested();
    }
    return false;
}

void DoseGridLayer::drawOverlay(QPainter &p)
{
    if (!hasData() || !isVisible())
        return;
    const dosemap::View v = dosemap::currentView();
    const QRectF screen(0, 0, p.device()->width(), p.device()->height());
    const int cols = m_grid.cols;
    const double dLat = (m_grid.latMax - m_grid.latMin) / m_grid.rows;
    p.setRenderHint(QPainter::Antialiasing);

    // Горячие точки (Ф8): "мишень". Близкие на экране (< 30 px) не дублируем - остаётся сильнейшая
    const double dLon = (m_grid.lonMax - m_grid.lonMin) / cols;
    QVector<int> hs;
    for (int idx : m_grid.hotspots)
        if (idx >= 0 && idx < m_grid.values.size() && !std::isnan(m_grid.values[idx]))
            hs.append(idx);
    std::sort(hs.begin(), hs.end(), [this](int a, int b) { return m_grid.values[a] > m_grid.values[b]; });
    QVector<QPointF> drawn;
    for (int idx : std::as_const(hs)) {
        const int r = idx / cols, c = idx % cols;
        const QPointF s = v.toScreen(dosemap::mercNorm(m_grid.latMin + (r + 0.5) * dLat,
                                                       m_grid.lonMin + (c + 0.5) * dLon));
        if (!screen.adjusted(-12, -12, 12, 12).contains(s))
            continue;
        bool near = false;
        for (const QPointF &d : std::as_const(drawn))
            if (QLineF(d, s).length() < 30)
                near = true;
        if (near)
            continue;
        drawn.append(s);
        const float val = m_grid.values[idx];
        const QColor col = m_scale->level(val) == DoseColorScale::Alarm ? QColor(0xE5, 0x48, 0x4D)
                                                                       : QColor(0xD9, 0x96, 0x1A);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 220), 4.5));
        p.drawEllipse(s, 9, 9);
        p.setPen(QPen(col, 2));
        p.drawEllipse(s, 9, 9);
        p.setPen(QPen(col, 1.5));
        p.drawLine(s + QPointF(-13, 0), s + QPointF(-6, 0));
        p.drawLine(s + QPointF(6, 0),   s + QPointF(13, 0));
        p.drawLine(s + QPointF(0, -13), s + QPointF(0, -6));
        p.drawLine(s + QPointF(0, 6),   s + QPointF(0, 13));

        // значение сетки в горячей точке
        const QFont f = mapFont(p.font(), 8);
        p.setFont(f);
        const QString text = dosemap::formatDose(val);
        const QFontMetricsF fm(f);
        const QRectF pr(s.x() + 15, s.y() - (fm.height() + 2) / 2, fm.horizontalAdvance(text) + 10, fm.height() + 2);
        drawPill(p, pr, text, levelTextColor(m_scale->level(val)), col, true);
    }

    // "Щуп": значение клетки под курсором
    if (m_probeCell >= 0 && m_probeCell < m_grid.values.size()) {
        const QRectF cr = cellRect(m_probeCell, v);
        if (cr.width() >= 5) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 220), 3));
            p.drawRect(cr);
            p.setPen(QPen(QColor(0x2B, 0x36, 0x40), 1));
            p.drawRect(cr);
        }
        const float val = m_grid.values[m_probeCell];
        const bool hot = m_grid.hotspots.contains(m_probeCell);
        const QString text = std::isnan(val)
                ? QStringLiteral("немає даних")
                : QString("%1≈ %2 мкЗв/год").arg(hot ? "Гаряча точка · " : "", dosemap::formatDose(val));
        const QFont f = mapFont(p.font(), 8);
        p.setFont(f);
        const QFontMetricsF fm(f);
        const qreal tw = fm.horizontalAdvance(text) + 14, th = fm.height() + 4;
        QRectF r(cr.right() + 8, cr.center().y() - th / 2, tw, th);
        if (r.right() > screen.right() - 4)
            r.moveRight(cr.left() - 8);
        const DoseColorScale::Level lvl = std::isnan(val) ? DoseColorScale::Normal : m_scale->level(val);
        drawPill(p, r, text, std::isnan(val) ? kMuted.darker(130) : levelTextColor(lvl),
                 std::isnan(val) ? kMuted : m_scale->color(val), false);
    }
}

void DoseGridLayer::mouseLeave()
{
    if (m_probeCell >= 0) {
        m_probeCell = -1;
        emit updateRequested();
    }
}

// ======================================================================
// DoseLegendLayer
// ======================================================================

DoseLegendLayer::DoseLegendLayer(const DoseColorScale *scale, MeasurementLayer *points,
                                 DoseGridLayer *grid, QObject *parent)
    : MapLayer(parent), m_scale(scale), m_points(points), m_grid(grid)
{
}

void DoseLegendLayer::draw(QPainter &p)
{
    if (m_grid) {
        p.save();
        m_grid->drawOverlay(p);
        p.restore();
    }
    if (m_points) {
        p.save();
        m_points->drawCardOverlay(p);
        p.restore();
    }

    const bool pts = m_points && m_points->isVisible() && m_points->hasData();
    const bool grd = m_grid && m_grid->isVisible() && m_grid->hasData();
    if (!pts && !grd)
        return;

    p.setRenderHint(QPainter::Antialiasing);
    const QFont fTitle = mapFont(p.font(), 8);
    const QFont fTick = mapFont(p.font(), 7.5);
    const QFont fMax = mapFont(p.font(), 8, true);
    const QFontMetricsF mt(fTitle), mk(fTick);

    const qreal W = 250, pad = 9;
    const qreal barH = 8;
    const qreal H = pad + mt.height() + 4 + barH + 3 + mk.height() + pad - 2;
    const QRectF box(10 + m_inset, p.device()->height() - 10 - H, W, H);

    p.setPen(QPen(QColor(0, 0, 0, 45), 1));
    p.setBrush(QColor(255, 255, 255, 236));
    p.drawRoundedRect(box, 6, 6);

    // заголовок + максимум
    p.setFont(fTitle);
    p.setPen(QColor(0x4A, 0x55, 0x60));
    const qreal ty = box.top() + pad + mt.ascent();
    p.drawText(QPointF(box.left() + pad, ty), "ПАЕД, мкЗв/год");

    const QRectF bar(box.left() + pad, box.top() + pad + mt.height() + 4, W - 2 * pad, barH);
    if (pts && m_points->maxValue() > 0) {
        const float mx = m_points->maxValue();
        const QString s = "макс " + dosemap::formatDose(mx);
        p.setFont(fMax);
        p.setPen(levelTextColor(m_scale->level(mx)));
        p.drawText(QPointF(box.right() - pad - QFontMetricsF(fMax).horizontalAdvance(s), ty), s);

        // треугольник над шкалой в позиции максимума
        const qreal x = bar.left() + bar.width() * m_scale->position(mx);
        QPolygonF tri;
        tri << QPointF(x - 4, bar.top() - 5) << QPointF(x + 4, bar.top() - 5) << QPointF(x, bar.top() - 0.5);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x2B, 0x36, 0x40));
        p.drawPolygon(tri);
    }

    // градиент из таблицы шкалы
    QImage grad(256, 1, QImage::Format_RGB32);
    for (int i = 0; i < 256; ++i)
        grad.setPixel(i, 0, m_scale->lut()[i]);
    QPainterPath clip;
    clip.addRoundedRect(bar, 3, 3);
    p.save();
    p.setClipPath(clip);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(bar, grad);
    p.restore();

    // отметки: начало, предупреждение, тревога, конец
    p.setFont(fTick);
    const float ticks[] = { m_scale->minValue(), m_scale->warn(), m_scale->alarm(), m_scale->maxValue() };
    for (int i = 0; i < 4; ++i) {
        const qreal x = bar.left() + bar.width() * m_scale->position(ticks[i]);
        const QString s = dosemap::formatDose(ticks[i]);
        const qreal tw = mk.horizontalAdvance(s);
        qreal tx = x - tw / 2;
        if (i == 0) tx = bar.left();
        if (i == 3) tx = bar.right() - tw;
        if (i == 1 || i == 2) {
            p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
            p.drawLine(QPointF(x, bar.top()), QPointF(x, bar.bottom()));
        }
        p.setPen(i == 1 ? levelTextColor(DoseColorScale::Warning)
                 : i == 2 ? levelTextColor(DoseColorScale::Alarm)
                          : QColor(0x6A, 0x75, 0x80));
        p.drawText(QPointF(tx, bar.bottom() + 3 + mk.ascent()), s);
    }
}
