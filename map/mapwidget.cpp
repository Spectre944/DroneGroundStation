#include "mapwidget.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPushButton>
#include <QHBoxLayout>
#include <cstdlib>

// Глобальные объекты из mapcore.cpp
extern mapdraw m_draw;
extern mapcore m_core;
extern cdirprovider m_prov;

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);   // для Esc / Enter / Backspace

    p_map = new QPixmap(4096, 4096);

    // Заглушка для отсутствующих тайлов
    QImage empty(TILE_SIZE, TILE_SIZE, QImage::Format_RGB32);
    empty.fill(QColor(220, 220, 220));
    m_prov.setEmptyTileIcon(empty);

    centerPosition = m_core.getview();

    // Слой рисования - обычный слой, как и любой другой
    drawing = new DrawingLayer(this);
    addLayer(drawing);
    connect(drawing, &DrawingLayer::modeChanged, this,
            [this](DrawingLayer::Mode m) { onDrawingModeChanged(m); });

    setupToolbar();
}

MapWidget::~MapWidget()
{
    delete p_map;
}

// ------------------------ КАРТА ------------------------ //

bool MapWidget::setMapFile(const QString &path)
{
    if (path.isEmpty())
        return false;

    const bool ok = m_prov.openTileDB(path.toUtf8().constData());
    update();
    return ok;
}

void MapWidget::setCenter(const smappoint &pt)
{
    m_core.setview(pt);
    syncCenter();
    update();
}

int MapWidget::zoom() const
{
    return m_core.getzoom();
}

void MapWidget::setZoom(int z)
{
    z = qBound(MIN_ZOOM, z, MAX_ZOOM);
    if (z == m_core.getzoom())
        return;
    m_core.setzoom(z);
    emit zoomChanged(z);
    update();
}

void MapWidget::zoomIn()  { adjustZoom(+120); }
void MapWidget::zoomOut() { adjustZoom(-120); }

void MapWidget::adjustZoom(int delta)
{
    setZoom(m_core.getzoom() + (delta > 0 ? 1 : -1));
}

void MapWidget::syncCenter()
{
    centerPosition = m_core.getview();
    emit centerPositionChanged(centerPosition);
}

// ------------------------ СЛОИ ------------------------ //

void MapWidget::addLayer(MapLayer *layer)
{
    if (!layer || layers.contains(layer))
        return;

    if (!layer->parent())
        layer->setParent(this);

    layers.append(layer);
    connect(layer, &MapLayer::updateRequested, this,
            QOverload<>::of(&MapWidget::update));
    update();
}

void MapWidget::removeLayer(MapLayer *layer)
{
    if (!layers.removeOne(layer))
        return;

    disconnect(layer, nullptr, this, nullptr);
    if (layer == drawing)
        drawing = nullptr;
    layer->deleteLater();
    update();
}

// ------------------------ DRAW ------------------------ //

void MapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    m_core.setcreensize(width(), height());

    QPainter p(p_map);
    m_draw.draw(p);
    p.end();

    const QRect screen(0, 0, width(), height());
    const QRect src(m_core.offs_map.x % TILE_SIZE,
                    m_core.offs_map.y % TILE_SIZE,
                    width(), height());
    painter.drawPixmap(screen, *p_map, src);

    // Перекрестие в центре
    painter.setPen(QPen(Qt::red, 2));
    const QPoint c = rect().center();
    painter.drawLine(c.x() - 10, c.y(), c.x() + 10, c.y());
    painter.drawLine(c.x(), c.y() - 10, c.x(), c.y() + 10);

    // Слои - в порядке добавления
    for (MapLayer *layer : layers) {
        if (layer->isVisible())
            layer->draw(painter);
    }
}

// ------------------------ INPUT ------------------------ //

void MapWidget::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (std::abs(delta) >= 16)
        adjustZoom(delta);
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    // события слоям - сверху вниз
    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->mousePress(event)) {
            update();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        dragStartPos = event->pos();
        isDragging = false;
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->mouseMove(event))
            return;
    }

    if (!(event->buttons() & Qt::LeftButton))
        return;

    if (!isDragging &&
        (event->pos() - dragStartPos).manhattanLength() > DRAG_THRESHOLD) {
        isDragging = true;
        updateCursor();
    }

    if (isDragging) {
        const QPoint d = event->pos() - dragStartPos;
        dragStartPos = event->pos();

        if (std::abs(d.x()) > 1) m_core.scroll_h(-d.x());
        if (std::abs(d.y()) > 1) m_core.scroll_v(-d.y());

        syncCenter();
        update();
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasDragging = isDragging;

    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        updateCursor();
    }

    // после перетаскивания карты "клик" слоям не отдаём
    if (wasDragging)
        return;

    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->mouseRelease(event)) {
            update();
            return;
        }
    }
}

void MapWidget::keyPressEvent(QKeyEvent *event)
{
    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->keyPress(event)) {
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_core.setcreensize(event->size().width(), event->size().height());
    update();
}

void MapWidget::updateCursor()
{
    if (isDragging)
        setCursor(Qt::ClosedHandCursor);
    else if (drawing && drawing->isActive())
        setCursor(Qt::CrossCursor);
    else
        setCursor(Qt::ArrowCursor);
}

// ------------------- ПАНЕЛЬ РИСОВАНИЯ ------------------- //

void MapWidget::setupToolbar()
{
    toolbar = new QWidget(this);
    toolbar->setObjectName("mapToolbar");
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    toolbar->setAttribute(Qt::WA_NoMousePropagation, true);  // клики по панели не двигают карту
    toolbar->setStyleSheet("#mapToolbar { background: rgba(255,255,255,220); border-radius: 6px; }");

    auto *lay = new QHBoxLayout(toolbar);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    auto makeBtn = [&](const QString &text, bool checkable) {
        auto *b = new QPushButton(text, toolbar);
        b->setCheckable(checkable);
        b->setFocusPolicy(Qt::NoFocus);   // чтобы не уводить фокус с карты
        lay->addWidget(b);
        return b;
    };

    btnLine    = makeBtn("Лінія", true);
    btnPolygon = makeBtn("Полігон", true);
    btnFinish  = makeBtn("Закінчити", false);
    btnUndo    = makeBtn("Видалити точку", false);
    btnCancel  = makeBtn("Відміна", false);
    btnClear   = makeBtn("Очистити", false);

    // Кнопка-переключатель: нажали - начали, нажали ещё раз - отменили
    connect(btnLine, &QPushButton::clicked, this, [this](bool on) {
        if (on) drawing->startLine(); else drawing->cancel();
    });
    connect(btnPolygon, &QPushButton::clicked, this, [this](bool on) {
        if (on) drawing->startPolygon(); else drawing->cancel();
    });
    connect(btnFinish, &QPushButton::clicked, this, [this] { drawing->finish(); });
    connect(btnUndo,   &QPushButton::clicked, this, [this] { drawing->undoLastPoint(); });
    connect(btnCancel, &QPushButton::clicked, this, [this] { drawing->cancel(); });
    connect(btnClear,  &QPushButton::clicked, this, [this] { drawing->clear(); });

    toolbar->move(10, 10);
    onDrawingModeChanged(DrawingLayer::Mode::None);
    toolbar->show();
    toolbar->raise();
}

void MapWidget::onDrawingModeChanged(DrawingLayer::Mode mode)
{
    const bool active = (mode != DrawingLayer::Mode::None);

    btnLine->setChecked(mode == DrawingLayer::Mode::Line);
    btnPolygon->setChecked(mode == DrawingLayer::Mode::Polygon);

    // "Закінчити / Видалити точку / Відміна" видны только во время рисования
    btnFinish->setVisible(active);
    btnUndo->setVisible(active);
    btnCancel->setVisible(active);

    toolbar->adjustSize();
    updateCursor();
    if (active)
        setFocus();   // чтобы работали горячие клавиши
}
