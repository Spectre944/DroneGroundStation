#ifndef SPECTRUMPLOT_H
#define SPECTRUMPLOT_H

#include <QWidget>
#include <QColor>
#include <QStringList>
#include <QVector>

/**
    @brief График спектра на QPainter (без QtCharts).
    @note  Управление как в WinSpectr.qml:
             ЛКМ + протянуть вправо  — увеличить выделенную рамку;
             ЛКМ + протянуть влево   — сбросить масштаб;
             ЛКМ щелчок              — маркер канала (повторный щелчок по нему — убрать);
             ПКМ + протянуть         — сдвиг увеличенного графика;
           и дополнительно: колесо — масштаб по X вокруг курсора, двойной щелчок — сброс.
           По оси Y — линейная или логарифмическая шкала (log10(n + 1), чтобы 0 был виден).
*/
class SpectrumPlot : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumPlot(QWidget *parent = nullptr);

    void setData(const QVector<quint16> &channels);
    void setOverlay(const QVector<quint16> &channels);  ///< спектр для сравнения — пунктиром
    void clearOverlay();
    void clear();

    void setLogScale(bool on);
    bool logScale() const               { return m_log; }

    void setXRange(double from, double to);             ///< каналы; Y подстраивается сам
    void resetZoom();
    double xFrom() const                { return m_x0; }
    double xTo() const                  { return m_x1; }

    void setInfoLines(const QStringList &lines);        ///< блок в правом верхнем углу
    void setLineColor(const QColor &c);

    int marker() const                  { return m_marker; }
    void setMarker(int channel);                        ///< -1 — убрать

    QSize minimumSizeHint() const override { return QSize(320, 200); }

signals:
    void xRangeChanged(double from, double to);
    void markerChanged(int channel);
    void logScaleChanged(bool on);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    enum class Drag { None, Zoom, Pan };

    int    channelCount() const;
    QRectF plotRect() const;
    double tr_(double v) const;                 ///< значение → ось Y (лин или log10(v+1))
    double untr(double t) const;
    double xToPx(double ch) const;
    double pxToX(double px) const;
    double tToPx(double t) const;
    double pxToT(double py) const;
    quint16 valueAt(int ch) const;

    void autoY();
    void clampX();
    void emitRange();

    void drawGrid(QPainter &p, const QRectF &r);
    void drawSeries(QPainter &p, const QRectF &r, const QVector<quint16> &d, const QColor &c, bool fill, bool dashed);
    void drawBadge(QPainter &p, const QPointF &anchor, const QString &text, const QColor &bg, Qt::Alignment where);
    void drawInfo(QPainter &p, const QRectF &r);

    QVector<quint16> m_data;
    QVector<quint16> m_overlay;
    bool   m_log = false;

    double m_x0 = -0.5, m_x1 = 1023.5;          ///< видимые каналы
    double m_t0 = 0.0,  m_t1 = 1.0;             ///< видимый диапазон Y (в единицах оси)
    bool   m_yZoomed = false;                   ///< Y задан рамкой, а не автоматически

    Drag    m_drag = Drag::None;
    QPointF m_press, m_mouse;
    double  m_px0 = 0, m_px1 = 0, m_pt0 = 0, m_pt1 = 0;    ///< диапазоны в момент нажатия ПКМ
    bool    m_hover = false;

    int         m_marker = -1;
    QStringList m_info;
    QColor      m_line;
};

#endif // SPECTRUMPLOT_H
