#ifndef TIMESERIESVIEW_H
#define TIMESERIESVIEW_H

/**
    @file  timeseriesview.h
    @brief «Графіки польоту» (Р2-9, Ф11) для страницы «Прилади»: ПАЕД, ПАЕД на 1 м,
           высота LIDAR и напряжение аккумулятора по времени полёта.
    @note  Два источника данных:
             - текущий полёт — живые измерения (DroneCore::measurement → appendLive),
               копятся с начала полёта (sessionStarted → clearLive);
             - прошлый полёт — по запросу: showArchive(id) → MainWindow вызывает
               DroneCore::requestTimeSeries(id) → timeSeriesReady → onTimeSeriesReady.
           Живые данные копятся и во время просмотра прошлого полёта — при возврате
           (showLive) график текущего полёта сразу полный.
           Галочки — какие панели показывать; ПАЕД — линейная или логарифмическая шкала.
           Пороги «попередження» и «тривога» — пунктиром на панели ПАЕД.
           Вид «Разом» (по умолчанию) / «Окремо» — кнопками над графиком, выбор запоминается
           (graphs/combined). «Разом»: ПАЕД — линии, высота — бледная заливка (шкала справа),
           аккумулятор — цветная полоса под осью по порогам setBatteryLevels.
*/

#include <QVector>
#include <QWidget>

#include "backend/classes/core_types.h"

class QCheckBox;
class QLabel;
class QPushButton;
class TimeSeriesPlot;

class TimeSeriesView : public QWidget
{
    Q_OBJECT
public:
    explicit TimeSeriesView(QWidget *parent = nullptr);

public slots:
    void appendLive(const Measurement &m);              ///< новое измерение текущего полёта
    void clearLive(const QString &flightName = QString());  ///< начат новый полёт
    void showLive();                                    ///< показать текущий полёт
    void showArchive(qint64 sessionId, const QString &name);   ///< ждать timeSeriesReady(id)
    void onTimeSeriesReady(qint64 sessionId, const QVector<Measurement> &series);
    void setThresholds(double warn, double alarm);      ///< мкЗв/год
    /// Пороги полосы аккумулятора «Разом»: ≥ ok — зелёный, ≥ low — жёлтый, ниже — красный
    void setBatteryLevels(double ok, double low);

private:
    void fill(const QVector<Measurement> &series);
    void addToPlot(const Measurement &m);
    void updateTitle();

    TimeSeriesPlot *m_plot = nullptr;
    QLabel      *m_source = nullptr;    ///< «поточний політ» / «політ «…»»
    QCheckBox   *m_paed = nullptr;
    QCheckBox   *m_height = nullptr;
    QCheckBox   *m_batt = nullptr;
    QCheckBox   *m_log = nullptr;
    QPushButton *m_reset = nullptr;
    QPushButton *m_together = nullptr;  ///< вид «Разом»
    QPushButton *m_apart = nullptr;     ///< вид «Окремо»

    int m_pPaed = -1, m_sPaed = -1, m_sPaed1m = -1;     ///< панель ПАЕД и её линии
    int m_pHeight = -1, m_sHeight = -1;
    int m_pBatt = -1, m_sBatt = -1;

    QVector<Measurement> m_live;        ///< измерения текущего полёта
    QString m_liveName;
    bool    m_showingLive = true;
    qint64  m_archiveId = -1;
    QString m_archiveName;
    bool    m_loading = false;
};

#endif // TIMESERIESVIEW_H
