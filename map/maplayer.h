#ifndef MAPLAYER_H
#define MAPLAYER_H

#include <QObject>
#include <QPainter>
#include <QPoint>
#include <QMouseEvent>
#include <QKeyEvent>
#include "mapcore.h"

// Базовый класс слоя карты.
//
// Чтобы добавить свой слой:
//   1. Наследуйтесь от MapLayer и реализуйте draw().
//   2. При необходимости переопределите mousePress/Move/Release/keyPress
//      (вернуть true = событие обработано, дальше оно не пойдёт).
//   3. Когда данные изменились - вызовите emit updateRequested().
//   4. Добавьте слой: mapWidget->addLayer(new MyLayer(mapWidget));
//
// Слои рисуются в порядке добавления (последний - сверху),
// а события получают в обратном порядке (сверху вниз).
class MapLayer : public QObject
{
    Q_OBJECT
public:
    explicit MapLayer(QObject *parent = nullptr) : QObject(parent) {}
    ~MapLayer() override = default;

    bool isVisible() const { return m_visible; }
    void setVisible(bool v)
    {
        if (m_visible == v) return;
        m_visible = v;
        emit updateRequested();
    }

    // Рисование в координатах виджета (пиксели экрана)
    virtual void draw(QPainter &p) = 0;

    // События. Возврат true - слой обработал событие.
    virtual bool mousePress(QMouseEvent *)   { return false; }
    virtual bool mouseMove(QMouseEvent *)    { return false; }
    virtual bool mouseRelease(QMouseEvent *) { return false; }
    virtual bool keyPress(QKeyEvent *)       { return false; }

signals:
    void updateRequested();   // попросить виджет перерисоваться

protected:
    // географические координаты -> пиксели виджета
    static QPoint toScreen(const smappoint &pt)
    {
        u_long x = 0, y = 0;
        m_core.longlatt2screen_xy(x, y, pt);
        return QPoint(int(x), int(y));
    }

    // пиксели виджета -> географические координаты
    static smappoint toGeo(const QPoint &pos)
    {
        smappoint pt{};
        m_core.screen_xy2longlatt(pos.x(), pos.y(), pt.longt, pt.latt);
        return pt;
    }

private:
    bool m_visible = true;
};

#endif // MAPLAYER_H
