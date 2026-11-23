#ifndef RECON_AREA_H
#define RECON_AREA_H

/**
    @file  recon_area.h
    @brief Область разведки (Ф3): проверка перед ID04 и «точка внутри области?».

    Логгер сам область почти не проверяет (только число вершин). Кривую область
    он примет, но будет ошибаться «внутри / снаружи». Поэтому всё проверяется здесь.

    @note  «Точка внутри» — алгоритм логгера один в один
           (LoggerStateMachine_IsPointInsideArea, logger_state_machine.c):
           наша карта и логгер всегда согласны.
    @note  Логгер получает вершины во float (около 7 значащих цифр).
           Поэтому проверки и contains() считают с вершинами, округлёнными до float.
    @note  Обычный класс, не QObject: только расчёт, без сигналов и потоков.
*/

#include <QVector>

#include "core_types.h"

class ReconArea
{
public:
    /**
        @brief Охватывающий прямоугольник области.
    */
    struct Bounds {
        double latMin = 0.0;
        double latMax = 0.0;
        double lonMin = 0.0;
        double lonMax = 0.0;
    };

    static constexpr double MinVertexDistM = 1.0;   ///< вершины ближе — повтор; отклонение от прямой меньше — «на одной прямой»

    /**
        @brief  Проверить область по правилам логгера.
        @param  vertices  Вершины подряд по контуру (по часовой или против).
        @param  isRoute   true — нарисована линия (маршрут): логгер её не поддерживает.
        @param  maxAreaKm Наибольший размер области, км (настройка, по умолчанию 50).
        @return ok, код ошибки и номер вершины, к которой она относится (-1 — ко всей области).
        @note   Порядок проверок: линия, число вершин, координаты, 180-й меридиан,
                повтор, три на прямой, пересечение сторон, выпуклость, размер.
                Первая не прошедшая — ответ.
    */
    static AreaCheck check(const QVector<GeoPoint> &vertices, bool isRoute = false,
                           double maxAreaKm = 50.0);

    /**
        @brief  Точка внутри области или на её границе — как считает логгер.
        @return false и для неправильной области (вершин не 3–20).
    */
    static bool contains(const QVector<GeoPoint> &vertices, double lat, double lon);

    /**
        @brief Охватывающий прямоугольник — для сетки (Ф7) и карты.
    */
    static Bounds bounds(const QVector<GeoPoint> &vertices);

    /**
        @brief Расстояние между точками по поверхности Земли, м (гаверсинус, как в логгере).
    */
    static double distanceM(const GeoPoint &a, const GeoPoint &b);
};

#endif // RECON_AREA_H
