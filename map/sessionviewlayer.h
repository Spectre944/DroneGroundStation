#pragma once
/**
    @file  sessionviewlayer.h
    @brief Слой просмотра прошлого полёта: маршрут и области разведки (Р2-2).
    @note  Точки измерений и тепловая карта прошлого полёта рисуются обычными слоями
           (MeasurementLayer, DoseGridLayer) — MapWidget на время просмотра подменяет в них
           данные и потом возвращает живые. Здесь — только то, чего у живого полёта
           в этих слоях нет: путь дрона (SessionData::route) и все области (SessionData::areas).
*/

#include <QVector>
#include <QPointF>

#include "maplayer.h"
#include "classes/core_types.h"

class SessionViewLayer : public MapLayer
{
    Q_OBJECT
public:
    explicit SessionViewLayer(QObject *parent = nullptr);

    void setData(const QVector<RoutePoint> &route, const QVector<QVector<GeoPoint>> &areas);
    void clear();
    bool hasData() const { return !m_route.isEmpty() || !m_areas.isEmpty(); }

    void draw(QPainter &p) override;

private:
    QVector<QPointF>          m_route;   ///< нормированный Меркатор
    QVector<QVector<QPointF>> m_areas;
};
