#ifndef DOSE_CORRECTION_H
#define DOSE_CORRECTION_H

/**
    @file  dose_correction.h
    @brief Обработка измерения перед картой, тревогами и базой:
           пересчёт ПАЕД на высоту 1 м (Ф6) и флаги достоверности (Ф9).

    Пересчёт — ВРЕМЕННАЯ модель (модель и коэффициенты — открытый вопрос):
        ПАЕД(1 м) = ПАЕД(h) · exp(μ · (h − 1)),  h — высота LIDAR, м;  μ — из настроек, 1/м.
    Пересчёт выключен (по умолчанию) — ПАЕД на 1 м не считается, показывается исходная.

    Флаги достоверности (Ф9) — пока два, остальные — по расшифровке байта Stat БДБГ
    (открытый вопрос):
      - MfLidarRange — высота LIDAR вне 1–600 м или слабый сигнал (< 100):
        так же, как логгер отбраковывает кадр LIDAR (lidar.h);
      - MfOutOfArea  — точка вне принятой области (ID04), по алгоритму логгера.
    Недостоверное измерение пишется в базу и показывается на карте,
    но не идёт в тепловую карту и не вызывает тревогу.

    @note  Все измерения проходят здесь до сетки, тревог и базы — у всех одно значение.
*/

#include <QVector>

#include "core_types.h"

class DoseCorrection
{
public:
    static constexpr quint16 LidarMinCm       = 100;     ///< 1 м — как LIDAR_MIN_DISTANCE_CM в логгере
    static constexpr quint16 LidarMaxCm       = 60000;   ///< 600 м — как LIDAR_MAX_DISTANCE_CM
    static constexpr quint16 LidarMinStrength = 100;     ///< как LIDAR_MIN_STRENGTH

    /**
        @brief Настройки пересчёта: включён ли, μ (correctionCoeffs[0]), допустимые высоты.
        @note  correctionMaxM ≤ correctionMinM — ограничения по высоте нет (кроме 1–600 м LIDAR).
    */
    void setSettings(const CoreSettings &s);

    /**
        @brief Принятая область (ID01 на ID04) — для флага «вне области»; пусто — флаг не ставится.
    */
    void setArea(const QVector<GeoPoint> &area) { m_area = area; }

    /**
        @brief  Обработать измерение: флаги достоверности и ПАЕД на 1 м.
        @return Копия измерения с заполненными flags, paed1m, hasPaed1m.
    */
    Measurement apply(const Measurement &m) const;

    /**
        @brief  ПАЕД на 1 м по временной модели.
        @param  paed     ПАЕД на высоте h, мкЗв/ч.
        @param  heightM  Высота h, м.
        @param  mu       Коэффициент μ, 1/м.
    */
    static float toOneMeter(float paed, double heightM, double mu);

private:
    bool   m_on = false;
    double m_mu = 0.0;
    double m_minM = 0.0;
    double m_maxM = 0.0;
    QVector<GeoPoint> m_area;
};

#endif // DOSE_CORRECTION_H
