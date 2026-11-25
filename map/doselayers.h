#pragma once
// ============================================================================
//  doselayers.h - слои карты для данных радиационной разведки.
//
//    DoseColorScale    - общая цветовая шкала ПАЕД (лог. шкала, привязана к порогам)
//    DoseGridLayer     - тепловая карта по сетке GridData (Ф7) + горячие точки (Ф8)
//    MeasurementLayer  - точки измерений (Measurement) со значениями (Ф5, Ф9)
//    DoseLegendLayer   - компактная легенда шкалы в углу карты
//
//  Производительность:
//    * координаты точек переводятся в нормированный Меркатор [0..1] один раз при
//      добавлении; на каждом кадре экранная позиция = n * 256*2^zoom - смещение;
//    * точки рисуются в кэш-картинку (видимая область + запас). При панорамировании
//      кэш только сдвигается, перестраивается при смене зума / шкалы / выходе за запас;
//      новая точка дорисовывается в кэш без перестройки;
//    * тепловая карта - QImage "одна клетка = один пиксель", пересобирается только
//      при новой сетке; на экран выводится только видимая часть полосами по 32 строки
//      (полосы нужны, чтобы Меркатор по широте не "плыл" на больших областях);
//    * размер точек зависит от зума; при перестройке кэша точки прореживаются по
//      экранной сетке (остаётся бОльшая в ячейке), чтобы плотный трек не закрывал
//      тепловую карту и не тормозил на мелком зуме;
//    * подписи значений - жадная раскладка без наложений: сначала самые большие
//      значения, затем остальные, не ближе LABEL_SPACING друг к другу.
// ============================================================================

#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QVector>

#include "maplayer.h"
#include "classes/core_types.h"
#include "coords/coordhandler.h"     // CoordType

class QMenu;

namespace dosemap {

// Нормированный Web Mercator: x,y в [0..1], (0,0) - северо-запад
QPointF mercNorm(double lat, double lon);
void    mercInverse(const QPointF &n, double &lat, double &lon);

// Текущее положение карты (берётся из m_core)
struct View {
    int     zoom = 0;
    double  world = 256.0;     // размер мира в пикселях на этом зуме
    QPointF offset;            // мировые пиксели левого верхнего угла экрана

    QPointF toScreen(const QPointF &n) const { return n * world - offset; }
    QPointF toNorm(const QPointF &s) const   { return (s + offset) / world; }
};
View currentView();

// Число для подписи: 0.08 / 0.42 / 3.41 / 12.4 / 150
QString formatDose(float v);

} // namespace dosemap

// ============================================================================
//  DoseColorScale
// ============================================================================

class DoseColorScale
{
public:
    enum Level { Normal = 0, Warning = 1, Alarm = 2 };

    DoseColorScale();

    // Пороги из настроек ядра (CoreSettings::warnLevel / alarmLevel), мкЗв/ч
    void  setThresholds(float warn, float alarm);
    float warn() const  { return m_warn; }
    float alarm() const { return m_alarm; }

    // Диапазон шкалы: warn/4 .. alarm*10, логарифмический
    float  minValue() const { return float(m_lo); }
    float  maxValue() const { return float(m_hi); }
    double position(float v) const;          // 0..1 вдоль шкалы
    double logLo() const { return m_logLo; }
    double invLogRange() const { return m_invLogRange; }
    int    lutIndex(float v) const;          // 0..255
    QRgb   rgb(float v) const { return m_lut[lutIndex(v)]; }
    QColor color(float v) const { return QColor(rgb(v)); }
    const QVector<QRgb> &lut() const { return m_lut; }
    Level  level(float v) const;

    quint64 version() const { return m_version; }   // растёт при смене порогов

private:
    void rebuild();

    float  m_warn = 0.3f;
    float  m_alarm = 1.0f;
    double m_lo = 0.075, m_hi = 10.0;
    double m_logLo = 0, m_invLogRange = 1;   // для position(): один логарифм на значение
    QVector<QRgb> m_lut;
    quint64 m_version = 1;
};

// ============================================================================
//  MeasurementLayer - точки измерений
// ============================================================================

class MeasurementLayer : public MapLayer
{
    Q_OBJECT
public:
    explicit MeasurementLayer(const DoseColorScale *scale, QObject *parent = nullptr);

    void addMeasurement(const Measurement &m);
    void setMeasurements(const QVector<Measurement> &list);
    void clear();

    bool  hasData() const { return !m_points.isEmpty(); }
    int   count() const { return m_points.size(); }
    float maxValue() const { return m_maxIndex >= 0 ? m_points[m_maxIndex].value : 0.0f; }

    void setLabelsVisible(bool on) { m_labels = on; emit updateRequested(); }
    bool labelsVisible() const { return m_labels; }
    void setCoordType(CoordType type) { m_coordType = type; }

    bool hasHover() const { return m_hover >= 0; }

    // Места, где подписи не ставим (горячие точки сетки), в нормированном Меркаторе
    void setAvoidPoints(const QVector<QPointF> &norm) { m_avoid = norm; emit updateRequested(); }

    // Карточка наведённой/выбранной точки - самый верх, рисует DoseLegendLayer
    void drawCardOverlay(QPainter &p);
    void selectAndCenter(qint64 measurementId);
    void selectIndexAndCenter(int index);        ///< по номеру точки в слое (таблица «База даних»)

    void draw(QPainter &p) override;
    bool mouseMove(QMouseEvent *e) override;
    bool mouseRelease(QMouseEvent *e) override;
    bool keyPress(QKeyEvent *e) override;
    void mouseLeave() override;
    void buildContextMenu(const QPoint &pos, QMenu &menu) override;

signals:
    void measurementSelected(qint64 id, int index);   // index - порядковый номер точки в слое
    void centerRequested(double lat, double lon);

private:
    struct Point {
        QPointF   n;              // нормированный Меркатор
        double    lat = 0, lon = 0;
        float     value = 0;      // shownPaed()
        float     paed = 0;
        float     paed1m = 0;
        bool      has1m = false;
        quint16   lidarCm = 0;
        quint32   flags = 0;
        QDateTime time;
        qint64    id = 0;
        bool reliable() const { return flags == MfNone; }
    };

    static Point makePoint(const Measurement &m);
    void appendPoint(const Point &pt);
    int  hitTest(const QPoint &pos, const dosemap::View &v) const;

    static qreal dotRadius(int zoom);
    void ensureSprites(qreal dpr, qreal radius);
    const QImage &spriteFor(const Point &pt) const;
    void ensureCache(const dosemap::View &v, const QSize &screen, qreal dpr);
    void paintPointIntoCache(QPainter &cp, const Point &pt, double world) const;

    void drawLabels(QPainter &p, const dosemap::View &v, const QRect &screen);
    void drawMarker(QPainter &p, const QPointF &s, const Point &pt, bool selected);
    void drawCard(QPainter &p, const QPointF &s, const Point &pt, int index, const QRect &bounds);

    const DoseColorScale *m_scale;
    QVector<Point> m_points;
    QVector<int>   m_ascending;      // порядок отрисовки: большие значения сверху
    QVector<int>   m_labelOrder;     // достоверные, по убыванию значения
    QVector<QPointF> m_avoid;
    bool           m_orderDirty = false;
    int            m_maxIndex = -1;  // максимум среди достоверных

    // кэш точек
    QImage  m_cache;
    QRect   m_cacheWorld;            // прямоугольник кэша в мировых пикселях
    int     m_cacheZoom = -1;
    qreal   m_cacheDpr = 0;
    quint64 m_cacheScale = 0;
    bool    m_cacheValid = false;

    // спрайты точек: 64 оттенка шкалы + "недостоверная"
    QVector<QImage> m_sprites;
    QImage  m_badSprite;
    qreal   m_spriteDpr = 0;
    qreal   m_spriteR = 0;
    quint64 m_spriteScale = 0;

    int  m_hover = -1;
    int  m_selected = -1;
    bool m_labels = true;
    CoordType m_coordType = CoordType::WGS84_String;

    static constexpr int HIT_R = 8;            // радиус попадания, px
    static constexpr int CACHE_MARGIN = 256;   // запас кэша вокруг экрана, px
    static constexpr int MAX_LABELS = 45;
    static constexpr int LABEL_SPACING = 64;   // мин. расстояние между подписанными точками, px
};

// ============================================================================
//  DoseGridLayer - тепловая карта
// ============================================================================

class DoseGridLayer : public MapLayer
{
    Q_OBJECT
public:
    explicit DoseGridLayer(const DoseColorScale *scale, QObject *parent = nullptr);

    void setGrid(const GridData &grid);
    void clear();
    bool hasData() const { return m_grid.cols > 0 && m_grid.rows > 0; }
    const GridData &grid() const { return m_grid; }

    // Значение в точке: NaN - нет данных; inArea=false - точка вне области/сетки
    float valueAt(double lat, double lon, bool *inArea = nullptr) const;

    void setOpacity(qreal o) { m_opacity = qBound(0.1, o, 1.0); emit updateRequested(); }

    // Точки измерений лежат выше: пока курсор на точке, "щуп" сетки не показываем
    void setPointsLayer(const MeasurementLayer *points) { m_points = points; }

    void draw(QPainter &p) override;
    bool mouseMove(QMouseEvent *e) override;
    void mouseLeave() override;

    // Горячие точки и "щуп" рисуются поверх точек измерений - их вызывает DoseLegendLayer
    void drawOverlay(QPainter &p);

private:
    void rebuildImage();
    int  cellAt(const QPointF &screen, const dosemap::View &v) const;   // индекс клетки или -1
    QRectF cellRect(int cell, const dosemap::View &v) const;

    const DoseColorScale *m_scale;
    const MeasurementLayer *m_points = nullptr;
    GridData m_grid;
    GridData m_imageGrid;          // по какой сетке построена картинка (для частичного обновления)
    QImage   m_image;              // cols x rows, строка 0 - север
    quint64  m_imageScale = 0;
    bool     m_imageDirty = false;
    qreal    m_opacity = 1.0;

    int    m_probeCell = -1;       // клетка под курсором
};

// ============================================================================
//  DoseLegendLayer - верхний слой дозы: горячие точки, "щуп" сетки, легенда
// ============================================================================

class DoseLegendLayer : public MapLayer
{
    Q_OBJECT
public:
    DoseLegendLayer(const DoseColorScale *scale, MeasurementLayer *points,
                    DoseGridLayer *grid, QObject *parent = nullptr);

    void setLeftInset(int px) { m_inset = px; emit updateRequested(); }
    void draw(QPainter &p) override;

private:
    const DoseColorScale  *m_scale;
    MeasurementLayer *m_points;
    DoseGridLayer *m_grid;
    int m_inset = 0;
};
