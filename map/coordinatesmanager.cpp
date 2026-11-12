#include "coordinatesmanager.h"
#include "ui/components/textedit/textedit.h"

#include <QApplication>
#include <QDebug>

CoordinatesManager* CoordinatesManager::m_instance = nullptr;

CoordinatesManager* CoordinatesManager::instance()
{
    if (!m_instance) {
        m_instance = new CoordinatesManager();
    }
    return m_instance;
}

CoordinatesManager::CoordinatesManager(QObject *parent)
    : QObject(parent)
{

}

void CoordinatesManager::setMapReference(MapWidget *map)
{
    m_map = map;
    connect(m_map, &MapWidget::coordinatesArrayReady, this, &CoordinatesManager::onCoordinatesArrayReady);
    connect(m_map, &MapWidget::drawingCanceled, this, &CoordinatesManager::onDrawingCanceled);

}

void CoordinatesManager::registerCoordinateField(TextEditCoordinates* field)
{
    if (!field) return;

    // Подключаемся к сигналу уничтожения объекта
    connect(field, &QObject::destroyed,
            this, &CoordinatesManager::onCoordinateFieldDestroyed);
}

void CoordinatesManager::unregisterCoordinateField(TextEditCoordinates* field)
{
    m_requestQueue.removeAll(field);
    if (m_currentRequester == field) {
        m_currentRequester = nullptr;
    }
}

void CoordinatesManager::requestCoordinatesFromMap(TextEditCoordinates* requester)
{
    if (!requester || !m_map) return;

    // Если уже есть активный запрос, добавляем в очередь
    if (m_currentRequester) {
        if (!m_requestQueue.contains(requester)) {
            m_requestQueue.enqueue(requester);
        }
        return;
    }

    m_currentRequester = requester;

    // Здесь можно добавить визуальную индикацию режима выбора координат на карте
    // Например, изменить курсор или показать сообщение
    if (m_map) {

        if(requester->getMode() == TextEditCoordinates::CoordinatesMode::Area)
            m_map->startPoligonDrawing();
        if(requester->getMode() == TextEditCoordinates::CoordinatesMode::Route)
            m_map->startLineDrawing();
    }
}

void CoordinatesManager::onCoordinatesArrayReady(QVector<smappoint> coordinates)
{
    if (!m_currentRequester) return;

    // Передаем координаты текущему запросчику
    m_currentRequester->setCoordinatesFromMap(coordinates);

    // Сбрасываем режим выбора на карте
    if (m_map) {
        // m_map->setCoordinateSelectionMode(false);
        // m_map->unsetCursor();
    }

    m_currentRequester = nullptr;

    // Обрабатываем следующий запрос в очереди
    if (!m_requestQueue.isEmpty()) {
        auto nextRequester = m_requestQueue.dequeue();
        requestCoordinatesFromMap(nextRequester);
    }
}

void CoordinatesManager::onDrawingCanceled()
{
    m_currentRequester = nullptr;
}

void CoordinatesManager::onCoordinateFieldDestroyed(QObject* field)
{
    auto coordField = static_cast<TextEditCoordinates*>(field);
    unregisterCoordinateField(coordField);
}
