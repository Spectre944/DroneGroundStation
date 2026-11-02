#include "drawinglayer.h"

DrawingLayer::DrawingLayer(QObject *parent) : MapLayer(parent) {}

// ---------------------- УПРАВЛЕНИЕ ---------------------- //

void DrawingLayer::setMode(Mode m)
{
    m_current.clear();
    m_hasMouse = false;
    if (m_mode != m) {
        m_mode = m;
        emit modeChanged(m_mode);
    }
    emit updateRequested();
}

void DrawingLayer::startLine()    { setMode(Mode::Line); }
void DrawingLayer::startPolygon() { setMode(Mode::Polygon); }
void DrawingLayer::cancel()       { setMode(Mode::None); }

void DrawingLayer::finish()
{
    if (m_mode == Mode::Polygon && m_current.size() >= 3) {
        m_polygons.append(m_current);
        emit polygonFinished(m_current);
    }
    else if (m_mode == Mode::Line && m_current.size() >= 2) {
        m_lines.append(m_current);
        emit lineFinished(m_current);
    }
    setMode(Mode::None);
}

void DrawingLayer::undoLastPoint()
{
    if (!m_current.isEmpty()) {
        m_current.removeLast();
        emit updateRequested();
    }
}

void DrawingLayer::clear()
{
    m_polygons.clear();
    m_lines.clear();
    emit updateRequested();
}

// ------------------------ СОБЫТИЯ ------------------------ //

bool DrawingLayer::mouseMove(QMouseEvent *e)
{
    if (!isActive()) return false;
    m_mousePos = e->pos();
    m_hasMouse = true;
    emit updateRequested();
    return false;   // не блокируем перетаскивание карты
}

bool DrawingLayer::mouseRelease(QMouseEvent *e)
{
    if (!isActive()) return false;

    if (e->button() == Qt::RightButton) {
        finish();
        return true;
    }
    if (e->button() != Qt::LeftButton)
        return false;

    const QPoint pos = e->pos();

    // клик у первой точки полигона / последней точки линии - завершение
    if (m_mode == Mode::Polygon && m_current.size() >= 3) {
        if ((pos - toScreen(m_current.first())).manhattanLength() <= CLOSE_DISTANCE) {
            finish();
            return true;
        }
    }
    else if (m_mode == Mode::Line && m_current.size() >= 2) {
        if ((pos - toScreen(m_current.last())).manhattanLength() <= CLOSE_DISTANCE) {
            finish();
            return true;
        }
    }

    m_current.append(toGeo(pos));
    emit updateRequested();
    return true;
}

bool DrawingLayer::keyPress(QKeyEvent *e)
{
    if (!isActive()) return false;

    switch (e->key()) {
    case Qt::Key_Escape:    cancel();          return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:     finish();          return true;
    case Qt::Key_Backspace: undoLastPoint();   return true;
    default:                return false;
    }
}

// ----------------------- ОТРИСОВКА ----------------------- //

void DrawingLayer::drawPoint(QPainter &p, const QPoint &pt, bool highlight)
{
    p.save();
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(highlight ? QColor(255, 140, 0) : QColor(30, 30, 30));
    const int r = highlight ? 7 : 5;
    p.drawEllipse(pt, r, r);
    p.restore();
}

void DrawingLayer::draw(QPainter &p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // Готовые линии
    p.setPen(QPen(QColor(0, 120, 255), 3));
    p.setBrush(Qt::NoBrush);
    for (const auto &line : m_lines) {
        QPolygon poly;
        for (const auto &pt : line) poly << toScreen(pt);
        p.drawPolyline(poly);
    }

    // Готовые полигоны
    p.setPen(QPen(QColor(0, 160, 60), 2));
    p.setBrush(QColor(0, 160, 60, 60));
    for (const auto &polygon : m_polygons) {
        QPolygon poly;
        for (const auto &pt : polygon) poly << toScreen(pt);
        p.drawPolygon(poly);
    }

    drawCurrent(p);
    p.restore();
}

void DrawingLayer::drawCurrent(QPainter &p)
{
    if (!isActive() || m_current.isEmpty()) return;

    QPolygon pts;
    for (const auto &pt : m_current) pts << toScreen(pt);

    QPen pen(m_mode == Mode::Polygon ? QColor(0, 160, 60) : QColor(0, 120, 255), 3,
             Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(pts);

    // линия до курсора
    if (m_hasMouse)
        p.drawLine(pts.last(), m_mousePos);

    // точки; первую точку полигона подсвечиваем, когда его уже можно замкнуть
    for (int i = 0; i < pts.size(); ++i) {
        const bool canClose = (m_mode == Mode::Polygon && i == 0 && pts.size() >= 3);
        drawPoint(p, pts[i], canClose);
    }
}
