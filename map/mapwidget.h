#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QVector>
#include "mapcore.h"      // подтягивает sqlite3.h, QPainter, QImage и т.д.
#include "maplayer.h"
#include "drawinglayer.h"

class QPushButton;

// Виджет карты: тайлы из SQLite, перетаскивание, зум, слои, рисование.
// Зависит только от mapcore.h (+ sqlite3.h) и Qt Widgets.
class MapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapWidget(QWidget *parent = nullptr);
    ~MapWidget() override;

    // Карта
    bool setMapFile(const QString &path);
    void setCenter(const smappoint &pt);
    smappoint center() const { return centerPosition; }

    int zoom() const;
    void setZoom(int z);
    void zoomIn();
    void zoomOut();

    // Слои. Виджет становится владельцем слоя.
    void addLayer(MapLayer *layer);
    void removeLayer(MapLayer *layer);
    DrawingLayer *drawingLayer() const { return drawing; }

signals:
    void centerPositionChanged(smappoint centerPosition);
    void zoomChanged(int zoom);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    static constexpr int MAX_ZOOM = 16;
    static constexpr int MIN_ZOOM = 3;
    static constexpr int DRAG_THRESHOLD = 5;

    QPixmap *p_map = nullptr;
    smappoint centerPosition{};

    bool isDragging = false;
    QPoint dragStartPos;

    QVector<MapLayer*> layers;
    DrawingLayer *drawing = nullptr;

    // Панель кнопок рисования
    QWidget *toolbar = nullptr;
    QPushButton *btnLine = nullptr;
    QPushButton *btnPolygon = nullptr;
    QPushButton *btnFinish = nullptr;
    QPushButton *btnUndo = nullptr;
    QPushButton *btnCancel = nullptr;
    QPushButton *btnClear = nullptr;

    void setupToolbar();
    void onDrawingModeChanged(DrawingLayer::Mode mode);
    void updateCursor();
    void adjustZoom(int delta);
    void syncCenter();
};

#endif // MAPWIDGET_H
