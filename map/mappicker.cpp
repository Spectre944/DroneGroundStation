#include "mappicker.h"

MapPicker *MapPicker::s_instance = nullptr;

MapPicker::MapPicker(MapWidget *map, QObject *parent)
    : QObject(parent)
    , m_map(map)
{
    Q_ASSERT_X(!s_instance, "MapPicker", "only one MapPicker is expected");
    s_instance = this;

    if (m_map) {
        connect(m_map, &MapWidget::geometryFinished, this, &MapPicker::onGeometryFinished);
        connect(m_map, &MapWidget::drawingCanceled,  this, &MapPicker::onDrawingCanceled);
    }
}

MapPicker::~MapPicker()
{
    if (s_instance == this)
        s_instance = nullptr;
}

MapPicker *MapPicker::instance()
{
    return s_instance;
}

bool MapPicker::isActiveFor(const QObject *context) const
{
    return m_active && context && m_context == context;
}

void MapPicker::pick(GeometryKind kind, QObject *context, PickedFn onPicked, CanceledFn onCanceled)
{
    if (!m_map || !context)
        return;

    // Новый запрос вытесняет старый: старому сообщаем об отмене
    if (m_active)
        finishRequest(true);

    m_active     = true;
    m_kind       = kind;
    m_context    = context;
    m_onPicked   = std::move(onPicked);
    m_onCanceled = std::move(onCanceled);

    // Владелец удалён (закрыли страницу и т.п.) - снимаем запрос и рисование
    // (QPointer m_context к этому моменту уже обнулён; соединение снимается в finishRequest,
    //  так что сюда попадаем, только если удаляется владелец текущего запроса.)
    connect(context, &QObject::destroyed, this, [this] {
        if (!m_active)
            return;
        finishRequest(false);
        if (m_map)
            m_map->cancelDrawing();
    });

    emit activeChanged(true);

    // Переключение режима рисования на карте генерирует drawingCanceled -
    // это "отмена" прошлой фигуры, а не нашего запроса.
    m_startingDrawing = true;
    if (kind == GeometryKind::Line)
        m_map->startLineDrawing();
    else
        m_map->startPolygonDrawing();
    m_startingDrawing = false;
}

void MapPicker::cancel()
{
    if (!m_active)
        return;
    finishRequest(true);
    if (m_map)
        m_map->cancelDrawing();   // drawingCanceled придёт, когда запрос уже снят - будет проигнорирован
}

void MapPicker::cancelFor(QObject *context)
{
    if (isActiveFor(context))
        cancel();
}

void MapPicker::onGeometryFinished(GeometryKind kind, const QVector<smappoint> &pts)
{
    if (!m_active || kind != m_kind)
        return;

    PickedFn onPicked = m_onPicked;
    QPointer<QObject> context = m_context;
    finishRequest(false);

    if (context && onPicked)
        onPicked(pts);
}

void MapPicker::onDrawingCanceled()
{
    if (!m_active || m_startingDrawing)
        return;
    finishRequest(true);
}

void MapPicker::finishRequest(bool notifyCanceled)
{
    if (!m_active)
        return;

    CanceledFn onCanceled = m_onCanceled;
    QPointer<QObject> context = m_context;

    if (context)
        disconnect(context, &QObject::destroyed, this, nullptr);

    m_active = false;
    m_context = nullptr;
    m_onPicked = nullptr;
    m_onCanceled = nullptr;

    emit activeChanged(false);

    if (notifyCanceled && context && onCanceled)
        onCanceled();
}
