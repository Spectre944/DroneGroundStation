#pragma once
// ============================================================================
//  dosetestflight.h - ТЕСТОВЫЕ данные для слоёв дозы (только для проверки карты).
//
//  Генерирует "полёт змейкой" над прямоугольной областью вокруг заданной точки:
//  фон + несколько источников, шум, немного недостоверных точек и вылеты за
//  область на разворотах. Сетка считается простым IDW (как в описании DoseGrid,
//  Ф7) - настоящую сетку будет присылать ядро через DroneCore::gridUpdated.
//
//  Сигналы совпадают по смыслу с DroneCore (measurement / gridUpdated), поэтому
//  MapWidget подключает генератор к тем же слотам, что и ядро.
//
//  Когда появится заглушка ядра (П2) - этот файл можно удалить.
// ============================================================================

#include <QObject>
#include <QTimer>
#include <QVector>

#include "classes/core_types.h"

class DoseTestFlight : public QObject
{
    Q_OBJECT
public:
    explicit DoseTestFlight(QObject *parent = nullptr);

    // Параметры сетки - как CoreSettings (gridCellM / gridRadiusM / gridPower / warnLevel)
    void setGridParams(double cellM, double radiusM, double power, float warnLevel);

    // Сгенерировать полёт с центром в (lat, lon). Ничего не отправляет.
    void generate(double centerLat, double centerLon);

    const QVector<Measurement> &measurements() const { return m_points; }

    // Сетка IDW по первым count измерениям (count < 0 - по всем)
    GridData buildGrid(int count = -1) const;

    // Отправить всё сразу: measurement() по каждой точке не шлётся
    // (для больших объёмов есть MapWidget::setMeasurements), только сетка.
    // "Живой" режим: pointsPerTick точек каждые intervalMs, сетка - на каждом тике.
    void startLive(int pointsPerTick = 6, int intervalMs = 100);
    void stop();
    bool isRunning() const { return m_timer.isActive(); }

signals:
    void measurement(const Measurement &m);
    void gridUpdated(const GridData &grid);
    void finished();

private:
    void tick();

    // Локальная система: u - вдоль галсов, v - поперёк (метры от центра)
    void toLatLon(double u, double v, double &lat, double &lon) const;
    void toLocal(double lat, double lon, double &u, double &v) const;
    double field(double u, double v) const;

    double m_lat0 = 0, m_lon0 = 0;
    double m_mPerDegLat = 111320.0, m_mPerDegLon = 111320.0;
    double m_angle = 0.35;              // поворот области, рад (~20°)
    double m_halfU = 350, m_halfV = 225;// полуразмеры области, м

    double m_cellM = 10, m_radiusM = 50, m_power = 2;
    float  m_warn = 0.3f;

    QVector<Measurement> m_points;
    int    m_sent = 0;
    int    m_perTick = 6;
    QTimer m_timer;
};
