#ifndef DRAWINGLAYER_H
#define DRAWINGLAYER_H

#include <QVector>
#include <QColor>
#include "maplayer.h"

// Слой рисования линий и полигонов.
//  ЛКМ              - добавить точку
//  ЛКМ у первой точки полигона / у последней точки линии - завершить
//  ПКМ / Enter      - завершить
//  Backspace        - удалить последнюю точку
//  Esc              - отменить
class DrawingLayer : public MapLayer
{
    Q_OBJECT
public:
    enum class Mode { None, Line, Polygon };

    explicit DrawingLayer(QObject *parent = nullptr);

    // Управление режимом
    void startLine();
    void startPolygon();
    void finish();            // завершить (если точек достаточно - сохранить)
    void cancel();            // отменить текущую фигуру
    void undoLastPoint();
    void clear();             // удалить все готовые фигуры

    bool isActive() const { return m_mode != Mode::None; }
    Mode mode() const { return m_mode; }

    const QVector<QVector<smappoint>> &polygons() const { return m_polygons; }
    const QVector<QVector<smappoint>> &lines() const { return m_lines; }

    // MapLayer
    void draw(QPainter &p) override;
    bool mouseMove(QMouseEvent *e) override;
    bool mouseRelease(QMouseEvent *e) override;
    bool keyPress(QKeyEvent *e) override;

signals:
    void polygonFinished(const QVector<smappoint> &polygon);
    void lineFinished(const QVector<smappoint> &line);
    void modeChanged(DrawingLayer::Mode mode);

private:
    static constexpr int CLOSE_DISTANCE = 15;   // px

    Mode m_mode = Mode::None;
    QVector<smappoint> m_current;
    QVector<QVector<smappoint>> m_polygons;
    QVector<QVector<smappoint>> m_lines;

    QPoint m_mousePos;
    bool m_hasMouse = false;

    void setMode(Mode m);
    void drawCurrent(QPainter &p);
    static void drawPoint(QPainter &p, const QPoint &pt, bool highlight = false);
};

Q_DECLARE_METATYPE(DrawingLayer::Mode)

#endif // DRAWINGLAYER_H
