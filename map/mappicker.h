#ifndef MAPPICKER_H
#define MAPPICKER_H

#include <functional>

#include <QObject>
#include <QPointer>
#include <QVector>

#include "mapwidget.h"

// ============================================================================
//  MapPicker - "дай мне фигуру с карты".
//
//  Заменяет CoordinatesManager. Любой виджет (например, TextEditCoordinates)
//  просит пользователя нарисовать полигон или линию и получает результат
//  в колбэк:
//
//      MapPicker::instance()->pick(GeometryKind::Polygon, this,
//          [this](const QVector<smappoint> &pts) { setCoordinatesFromMap(pts); },
//          [this] { setPicking(false); });
//
//  Правила:
//   - активен только один запрос; новый запрос отменяет предыдущий
//     (у предыдущего вызывается onCanceled), без очереди;
//   - context - владелец запроса: если он удалён, запрос тихо снимается,
//     колбэки не вызываются (регистрировать поля заранее не нужно);
//   - Esc / "Відміна" / переключение режима на карте -> onCanceled;
//   - фигура остаётся на карте (в GeometryStore), как при обычном рисовании.
//
//  Экземпляр создаёт MainWindow (владелец карты); до этого instance() == nullptr.
// ============================================================================

class MapPicker : public QObject
{
    Q_OBJECT
public:
    using PickedFn   = std::function<void(const QVector<smappoint> &)>;
    using CanceledFn = std::function<void()>;

    explicit MapPicker(MapWidget *map, QObject *parent = nullptr);
    ~MapPicker() override;

    static MapPicker *instance();

    void pick(GeometryKind kind, QObject *context, PickedFn onPicked, CanceledFn onCanceled = {});
    void cancel();                                       // отменить текущий запрос (если есть)
    void cancelFor(QObject *context);                    // отменить, только если он от context

    bool isActive() const { return m_active; }
    bool isActiveFor(const QObject *context) const;

signals:
    void activeChanged(bool active);

private:
    void onGeometryFinished(GeometryKind kind, const QVector<smappoint> &pts);
    void onDrawingCanceled();
    void finishRequest(bool notifyCanceled);

    static MapPicker *s_instance;

    QPointer<MapWidget> m_map;

    bool m_active = false;
    GeometryKind m_kind = GeometryKind::Polygon;
    QPointer<QObject> m_context;
    PickedFn m_onPicked;
    CanceledFn m_onCanceled;
    bool m_startingDrawing = false;   // наш собственный старт рисования не считается "отменой"
};

#endif // MAPPICKER_H
