#ifndef TIMESERIESPLOT_H
#define TIMESERIESPLOT_H

/**
    @file  timeseriesplot.h
    @brief Графики по времени полёта (Ф11) на QPainter: несколько панелей одна под другой
           с общей осью времени.
    @note  Панель = величина со своей шкалой Y (ПАЕД, высота, напряжение); в панели —
           одна или несколько линий (ПАЕД и ПАЕД на 1 м). Время — мс от эпохи (UTC),
           подписи оси — местное время.
           Управление как у SpectrumPlot:
             колесо — масштаб по времени вокруг курсора;
             ЛКМ + протянуть — сдвиг; двойной щелчок — весь полёт.
           Наведение — вертикальная линия через все панели, время внизу и значения
           каждой линии в ближайшей точке.
           Разрыв линии — если между соседними точками больше gapMs (нет данных).

           Два вида (setCombined):
             «Окремо» — каждая панель своей полосой, одна под другой;
             «Разом»  — всё на одном графике, у каждой панели своя роль (setPanelRole):
               Primary    — линии на переднем плане, шкала и сетка слева, пороги;
               Background — бледная заливка от низа, шкала справа без сетки;
               Band       — цветная полоса под осью: зелёная / жёлтая / красная по порогам
                            (setBandLevels), точное значение — при наведении.
             При наведении в виде «Разом» — одна плашка со всеми значениями.
*/

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class TimeSeriesPlot : public QWidget
{
    Q_OBJECT
public:
    /// Роль панели в виде «Разом»
    enum class Role { Primary, Background, Band };

    /// Одна линия: точки (время мс, значение)
    struct Series {
        QString name;
        QColor  color;
        QVector<QPointF> points;        ///< x — время, мс от эпохи; y — значение
    };
    /// Пороговая линия (пунктир) на панели
    struct Level {
        double  value = 0;
        QColor  color;
        QString name;
    };
    /// Панель со своей шкалой Y
    struct Panel {
        QString title;                  ///< подпись в левом верхнем углу
        QString unit;                   ///< единица для значений при наведении
        int     decimals = 2;
        bool    log = false;            ///< логарифмическая шкала (значения > 0)
        bool    visible = true;
        Role    role = Role::Primary;
        double  bandOk = 0, bandLow = 0; ///< пороги полосы (Role::Band)
        QVector<Series> series;
        QVector<Level>  levels;
    };

    explicit TimeSeriesPlot(QWidget *parent = nullptr);

    int  addPanel(const QString &title, const QString &unit, int decimals);   ///< номер панели
    int  addSeries(int panel, const QString &name, const QColor &color);      ///< номер линии в панели
    void setSeriesData(int panel, int series, const QVector<QPointF> &points);
    void appendPoint(int panel, int series, const QPointF &point);
    void setLevels(int panel, const QVector<Level> &levels);
    void setPanelVisible(int panel, bool visible);
    void setPanelLog(int panel, bool log);
    void clearData();

    void setCombined(bool on);                                  ///< «Разом» / «Окремо»
    bool combined() const { return m_combined; }
    void setPanelRole(int panel, Role role);                    ///< для вида «Разом»
    /// Пороги цветной полосы: ≥ ok — зелёный, ≥ low — жёлтый, ниже — красный
    void setBandLevels(int panel, double ok, double low);

    void resetZoom();                   ///< весь полёт
    void setGapMs(qint64 ms)            { m_gapMs = ms; update(); }
    void setEmptyText(const QString &t) { m_empty = t; update(); }

    QSize minimumSizeHint() const override { return QSize(320, 260); }

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    struct Range { double lo = 0, hi = 1; };

    bool   hasData() const;
    void   dataRange(double &t0, double &t1) const;
    QVector<int> visiblePanels() const;
    QRectF panelRect(int order, int count) const;
    Range  yRange(const Panel &p) const;
    double xToPx(double t, const QRectF &r) const;
    double pxToX(double px, const QRectF &r) const;
    double yToPx(double v, const Range &y, const Panel &p, const QRectF &r) const;
    void   clampX();

    void drawPanel(QPainter &p, const Panel &panel, const QRectF &r, bool bottom);
    void drawTimeAxis(QPainter &p, const QRectF &r);
    void drawHover(QPainter &p, const QVector<int> &order);
    void drawBadge(QPainter &p, const QPointF &anchor, const QString &text, const QColor &bg, bool left);

    // ---- вид «Разом» ----
    QRectF xRect() const;               ///< область по X (для мыши) в текущем виде
    QRectF combinedRect() const;        ///< поле графика без полосы и оси времени
    QRectF bandRect() const;            ///< цветная полоса под графиком
    bool   bandVisible() const;
    bool   backgroundVisible() const;
    QColor bandColor(const Panel &p, double v) const;
    void paintCombined(QPainter &p);
    void drawBackground(QPainter &p, const Panel &panel, const QRectF &r);
    void drawBand(QPainter &p, const Panel &panel, const QRectF &r);
    void drawCombinedHover(QPainter &p);
    static int nearest(const QVector<QPointF> &pts, double t);

    QVector<Panel> m_panels;
    double  m_x0 = 0, m_x1 = 0;         ///< видимое окно времени, мс; 0/0 — весь полёт
    bool    m_follow = true;            ///< окно следует за новыми точками (не увеличено вручную)
    qint64  m_gapMs = 60000;
    QString m_empty;
    bool    m_combined = false;

    bool    m_drag = false;
    QPoint  m_dragFrom;
    double  m_dragX0 = 0, m_dragX1 = 0;
    QPoint  m_mouse{-1, -1};
};

#endif // TIMESERIESPLOT_H
