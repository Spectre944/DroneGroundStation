#include "sessionviewlayer.h"
#include "doselayers.h"          // dosemap::mercNorm, dosemap::currentView

#include <QPainter>
#include <QPainterPath>

namespace {
const QColor kRoute(0x3D, 0x8B, 0xFD);      // маршрут — StatusColor::info
const QColor kArea(0x8E, 0x6C, 0xEF);       // области прошлого полёта — фиолетовый «спектр»
}

SessionViewLayer::SessionViewLayer(QObject *parent)
    : MapLayer(parent)
{
}

void SessionViewLayer::setData(const QVector<RoutePoint> &route, const QVector<QVector<GeoPoint>> &areas)
{
    m_route.clear();
    m_route.reserve(route.size());
    for (const RoutePoint &r : route)
        if (r.lat != 0.0 || r.lon != 0.0)
            m_route << dosemap::mercNorm(r.lat, r.lon);

    m_areas.clear();
    for (const QVector<GeoPoint> &a : areas) {
        QVector<QPointF> poly;
        for (const GeoPoint &g : a)
            poly << dosemap::mercNorm(g.lat, g.lon);
        if (poly.size() >= 3)
            m_areas << poly;
    }
    emit updateRequested();
}

void SessionViewLayer::clear()
{
    m_route.clear();
    m_areas.clear();
    emit updateRequested();
}

void SessionViewLayer::draw(QPainter &p)
{
    if (!hasData())
        return;
    const dosemap::View v = dosemap::currentView();
    p.setRenderHint(QPainter::Antialiasing);

    // Области: пунктир; последняя (по ней строится тепловая карта) — плотнее
    for (int i = 0; i < m_areas.size(); ++i) {
        QPolygonF poly;
        for (const QPointF &n : m_areas[i])
            poly << v.toScreen(n);
        const bool last = i == m_areas.size() - 1;
        QColor fill = kArea;
        fill.setAlpha(last ? 28 : 12);
        p.setBrush(fill);
        p.setPen(QPen(kArea, last ? 2.5 : 1.5, Qt::DashLine));
        p.drawPolygon(poly);
    }

    // Маршрут: белая подложка + линия; начало — кружок, конец — квадрат
    if (m_route.size() >= 2) {
        QPainterPath path;
        path.moveTo(v.toScreen(m_route.first()));
        for (int i = 1; i < m_route.size(); ++i)
            path.lineTo(v.toScreen(m_route[i]));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 200), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
        p.setPen(QPen(kRoute, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }
    if (!m_route.isEmpty()) {
        const QPointF a = v.toScreen(m_route.first());
        const QPointF b = v.toScreen(m_route.last());
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(kRoute);
        p.drawEllipse(a, 6, 6);
        p.drawRect(QRectF(b.x() - 5.5, b.y() - 5.5, 11, 11));
    }
}
