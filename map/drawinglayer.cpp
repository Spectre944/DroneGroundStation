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
    // Готовую фигуру забирает GeometryStore (через polygonFinished/lineFinished),
    // её рисует GeometryLayer. Здесь копию не храним: иначе после удаления
    // из контекстного меню фигура оставалась нарисованной этим слоем.
    if (m_mode == Mode::Polygon && m_current.size() >= 3)
        emit polygonFinished(m_current);
    else if (m_mode == Mode::Line && m_current.size() >= 2)
        emit lineFinished(m_current);
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
    setMode(Mode::None);   // только недорисованная фигура - готовые живут в GeometryStore
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

    drawCounter(p, m_hasMouse ? m_mousePos : pts.last());
}

// Счётчик точек у курсора: "7 / 20". Ближе к лимиту - жёлтый, сверх - красный.
void DrawingLayer::drawCounter(QPainter &p, const QPoint &anchor)
{
    const int n = m_current.size();
    const bool limited = (m_mode == Mode::Polygon && m_vertexLimit > 0);

    QString text = limited ? tr("Точок: %1 / %2").arg(n).arg(m_vertexLimit)
                           : tr("Точок: %1").arg(n);
    QColor fg(0x2B, 0x36, 0x40), border(0, 0, 0, 50);
    if (limited && n > m_vertexLimit) {
        text += tr(" · забагато для розвідки");
        fg = QColor(0xB4, 0x23, 0x2A);
        border = QColor(0xE5, 0x48, 0x4D);
    } else if (limited && n == m_vertexLimit) {
        text += tr(" · Enter — завершити");
        fg = QColor(0x8A, 0x5A, 0x00);
        border = QColor(0xD9, 0x96, 0x1A);
    } else if (limited && n >= m_vertexLimit - 2) {
        fg = QColor(0x8A, 0x5A, 0x00);
        border = QColor(0xD9, 0x96, 0x1A);
    }

    QFont f = p.font();
    f.setPointSizeF(8.5);
    p.setFont(f);
    const QFontMetrics fm(f);
    QRect r(0, 0, fm.horizontalAdvance(text) + 16, fm.height() + 6);
    r.moveTopLeft(anchor + QPoint(16, 16));
    const QRect vp = p.viewport();
    if (r.right() > vp.right() - 4)
        r.moveRight(anchor.x() - 12);
    if (r.bottom() > vp.bottom() - 4)
        r.moveBottom(anchor.y() - 12);

    p.setPen(QPen(border, 1));
    p.setBrush(QColor(255, 255, 255, 238));
    p.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);
    p.setPen(fg);
    p.drawText(r, Qt::AlignCenter, text);
}


