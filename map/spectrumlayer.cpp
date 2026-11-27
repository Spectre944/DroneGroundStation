#include "spectrumlayer.h"
#include <QCoreApplication>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "doselayers.h"
#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor

namespace {

constexpr qreal kHalf  = 10;    ///< половина стороны значка, px
constexpr qreal kHitR  = 14;    ///< радиус попадания мышью
constexpr qreal kLift  = 17;    ///< значок — над точкой набора (как булавка), точку измерения не закрывает

/// Центр значка для точки набора c
QPointF iconCenter(const QPointF &c) { return c - QPointF(0, kLift); }

QString mmss(int s)
{
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString stopText(SpectrumStopReason r)
{
    switch (r) {
    case SpectrumStopReason::None:          return QString();
    case SpectrumStopReason::Measurement:   return QCoreApplication::translate("SpectrumLayer", "логер повернувся до вимірів");
    case SpectrumStopReason::CommandStop:   return QCoreApplication::translate("SpectrumLayer", "зупинено оператором");
    case SpectrumStopReason::CommandFinish: return QCoreApplication::translate("SpectrumLayer", "закінчено розвідку");
    case SpectrumStopReason::NewArea:       return QCoreApplication::translate("SpectrumLayer", "нова область");
    case SpectrumStopReason::NewSpectrum:   return QCoreApplication::translate("SpectrumLayer", "почався новий спектр");
    case SpectrumStopReason::LoggerError:   return QCoreApplication::translate("SpectrumLayer", "аварія");
    case SpectrumStopReason::TimeElapsed:   return QCoreApplication::translate("SpectrumLayer", "минув заданий час");
    case SpectrumStopReason::ChannelFull:   return QCoreApplication::translate("SpectrumLayer", "канал заповнено");
    }
    return QString();
}

} // namespace

SpectrumLayer::SpectrumLayer(QObject *parent)
    : MapLayer(parent)
{
}

/////////////////////////////////// Данные ///////////////////////////////////

bool SpectrumLayer::same(const SpectrumInfo &a, const SpectrumInfo &b)
{
    if (a.id > 0 && b.id > 0)
        return a.id == b.id;
    return a.number == b.number && a.startTime == b.startTime;
}

void SpectrumLayer::setSpectra(const QVector<SpectrumInfo> &list)
{
    m_items.clear();
    for (const SpectrumInfo &s : list)
        if (s.lat != 0.0 || s.lon != 0.0)       // без координат на карту не ставим
            m_items << s;
    m_hover = m_selected = -1;
    emit updateRequested();
}

void SpectrumLayer::upsert(const SpectrumInfo &info)
{
    if (info.lat == 0.0 && info.lon == 0.0)
        return;
    for (SpectrumInfo &s : m_items)
        if (same(s, info)) {
            s = info;
            emit updateRequested();
            return;
        }
    m_items << info;
    emit updateRequested();
}

void SpectrumLayer::clear()
{
    m_items.clear();
    m_hover = m_selected = -1;
    emit updateRequested();
}

void SpectrumLayer::select(qint64 id, quint16 number)
{
    m_selected = -1;
    for (int i = 0; i < m_items.size(); ++i)
        if ((id > 0 && m_items[i].id == id) || (id <= 0 && number > 0 && m_items[i].number == number))
            m_selected = i;
    emit updateRequested();
}

/////////////////////////////////// Рисование ///////////////////////////////////

void SpectrumLayer::draw(QPainter &p)
{
    if (m_items.isEmpty())
        return;
    p.setRenderHint(QPainter::Antialiasing);
    const dosemap::View v = dosemap::currentView();
    const QRect screen(0, 0, p.device()->width(), p.device()->height());

    for (int i = 0; i < m_items.size(); ++i) {
        const QPointF c = v.toScreen(dosemap::mercNorm(m_items[i].lat, m_items[i].lon));
        if (!screen.adjusted(-30, -30, 30, 30).contains(c.toPoint()))
            continue;
        drawIcon(p, c, m_items[i], i == m_selected, i == m_hover);
    }

    const int card = m_hover >= 0 ? m_hover : -1;
    if (card >= 0)
        drawCard(p, v.toScreen(dosemap::mercNorm(m_items[card].lat, m_items[card].lon)), m_items[card], screen);
}

/**
    @brief Значок: скруглённый квадрат с «гистограммой» и номером под ним.
*/
void SpectrumLayer::drawIcon(QPainter &p, const QPointF &c, const SpectrumInfo &s, bool selected, bool hover) const
{
    const QColor accent = StatusColor::spectrum();
    const qreal h = hover ? kHalf + 1.5 : kHalf;
    const QPointF ic = iconCenter(c);
    const QRectF box(ic.x() - h, ic.y() - h, 2 * h, 2 * h);

    // «ножка» булавки к точке набора и маленькая точка в ней
    p.setPen(Qt::NoPen);
    p.setBrush(accent.darker(115));
    QPolygonF stem;
    stem << QPointF(c.x() - 4, box.bottom() - 1) << QPointF(c.x() + 4, box.bottom() - 1) << c;
    p.drawPolygon(stem);
    p.setBrush(Qt::white);
    p.drawEllipse(c, 1.6, 1.6);

    if (selected) {
        QColor ring = accent;
        ring.setAlpha(90);
        p.setPen(QPen(ring, 4));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box.adjusted(-4, -4, 4, 4), 7, 7);
    }

    // тень, чтобы значок читался на любой подложке
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawRoundedRect(box.translated(0, 1.5), 5, 5);

    const bool collecting = !s.finished;
    const bool sd = s.source == DataSource::Sd;
    p.setPen(QPen(sd ? QColor(40, 40, 40, 220) : QColor(255, 255, 255, 235), 1.5));
    p.setBrush(collecting ? QColor(255, 255, 255, 235) : accent);
    p.drawRoundedRect(box, 5, 5);
    if (collecting) {
        p.setPen(QPen(accent, 1.8, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box.adjusted(1.5, 1.5, -1.5, -1.5), 4, 4);
    }

    // «гистограмма»: 5 столбиков с пиком
    static const qreal bars[] = {0.35, 0.6, 1.0, 0.5, 0.25};
    const qreal bw = (2 * h - 6) / 5.0;
    const qreal base = box.bottom() - 4;
    const qreal top = box.height() - 8;
    p.setPen(Qt::NoPen);
    p.setBrush(collecting ? accent : QColor(255, 255, 255));
    for (int k = 0; k < 5; ++k) {
        const qreal bh = top * bars[k];
        p.drawRect(QRectF(box.left() + 3 + k * bw + 0.5, base - bh, bw - 1, bh));
    }

    // номер справа от значка
    QFont f = p.font();
    f.setPointSizeF(7.5);
    f.setBold(true);
    p.setFont(f);
    const QString num = QStringLiteral("№%1").arg(s.number);
    const QFontMetricsF fm(f);
    const QRectF tr(box.right() + 3, ic.y() - fm.height() / 2, fm.horizontalAdvance(num) + 6, fm.height());
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawRoundedRect(tr, 3, 3);
    p.setPen(accent.darker(140));
    p.drawText(tr, Qt::AlignCenter, num);
}

void SpectrumLayer::drawCard(QPainter &p, const QPointF &c, const SpectrumInfo &s, const QRect &bounds) const
{
    QStringList lines;
    if (s.startTime.isValid())
        lines << tr("Початок: %1").arg(s.startTime.toLocalTime().toString("HH:mm:ss"));
    lines << (s.setDurationS > 0 ? tr("Задано: %1").arg(mmss(s.setDurationS))
                                 : tr("Задано: до команди «Зупинити»"));
    lines << tr("Порцій: %1").arg(s.portions);
    if (s.portions > 0)
        lines << tr("ПАЕД: %1 мкЗв/год").arg(dosemap::formatDose(s.lastPaed));
    if (s.lidarCm > 0)
        lines << tr("Висота: %1 м").arg(s.lidarCm / 100.0, 0, 'f', 1);
    if (s.source == DataSource::Sd)
        lines << tr("Джерело: SD-карта");
    lines << (s.finished ? tr("Клік — відкрити спектр") : tr("Клік — дивитися набір"));

    const QString state = s.finished ? stopText(s.stopReason) : tr("йде набір");
    const QString title = tr("Спектр №%1").arg(s.number);
    dosemap::drawCard(p, iconCenter(c) + QPointF(0, -kHalf), title, StatusColor::spectrum().darker(130), StatusColor::spectrum(),
                      lines, state.isEmpty() ? QString() : tr("Стан: %1").arg(state), bounds);
}

/////////////////////////////////// Мышь ///////////////////////////////////

int SpectrumLayer::hitTest(const QPoint &pos) const
{
    const dosemap::View v = dosemap::currentView();
    int best = -1;
    qreal bestD = kHitR * kHitR;
    for (int i = 0; i < m_items.size(); ++i) {
        const QPointF d = iconCenter(v.toScreen(dosemap::mercNorm(m_items[i].lat, m_items[i].lon))) - QPointF(pos);
        const qreal dd = d.x() * d.x() + d.y() * d.y();
        if (dd <= bestD) {
            bestD = dd;
            best = i;
        }
    }
    return best;
}

bool SpectrumLayer::mouseMove(QMouseEvent *e)
{
    const int h = e->buttons() == Qt::NoButton ? hitTest(e->pos()) : -1;
    if (h != m_hover) {
        m_hover = h;
        emit updateRequested();
    }
    return false;
}

void SpectrumLayer::mouseLeave()
{
    if (m_hover >= 0) {
        m_hover = -1;
        emit updateRequested();
    }
}

bool SpectrumLayer::mouseRelease(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || m_items.isEmpty())
        return false;
    const int h = hitTest(e->pos());
    if (h < 0)
        return false;
    m_selected = h;
    emit spectrumClicked(m_items[h].id, m_items[h].number, m_items[h].startTime);
    emit updateRequested();
    return true;
}
