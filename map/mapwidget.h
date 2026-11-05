#pragma once
// ============================================================================
//  mapwidget.h - виджет карты и все его слои.
//
//  ИЗ ПРОЕКТА (как и раньше, не определяются здесь):
//    TILE_SIZE, MIN_ZOOM, MAX_ZOOM, DRAG_THRESHOLD, GPS_DATA_TIMEOUT_MS,
//    CoordPairD, gpsData, airmarData, GeometryInfoWidget, mapcore/mapdraw/cdirprovider.
// ============================================================================

#include <functional>
#include <memory>

#include <QWidget>
#include <QObject>
#include <QList>
#include <QVector>
#include <QPair>
#include <QVariant>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QLineF>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QtMath>

#include "mapcore.h"        // smappoint, smarker, mapcore   (поправьте под ваши заголовки)
#include "maplayer.h"
#include "drawinglayer.h"
#include "coords/coordhandler.h"   // CoordHandler
#include "settings/settings.h"       // Settings
#include "utilities/utilities.h"
#include "data_types/data_types.h"

class QMenu;
class QPushButton;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QContextMenuEvent;
class QResizeEvent;
class QPaintEvent;

extern mapcore m_core;

// ============================================================================
//  ОСТАВИТЬ КАК ЕСТЬ: class MapLayer и class DrawingLayer из вашего текущего mapwidget.h
//  (в загруженных файлах их не было). Вставьте их сюда, выше всего остального.
//
//  В MapLayer добавить ОДНУ виртуальную функцию:
//      virtual void buildContextMenu(const QPoint &pos, QMenu &menu)
//      { Q_UNUSED(pos) Q_UNUSED(menu) }
//
//  Код ниже предполагает: MapLayer(QObject *parent), setVisible(bool)/isVisible(),
//  сигнал updateRequested(); у DrawingLayer - сигналы
//      polygonFinished(const QVector<smappoint>&), lineFinished(const QVector<smappoint>&).
// ============================================================================

// ======================================================================
// Геометрические помощники
// ======================================================================

// Общие геометрические помощники для слоёв карты.
// Заменяют дубли pointToSegmentDistance / calculateDistance / loadColor из старого файла.



extern mapcore m_core;

namespace mapgeo {

// longlatt2screen_xy отдаёт u_long: для точек левее/выше экрана значение "заворачивается".
// Приведение к int здесь возвращает правильную отрицательную координату.
inline QPoint toScreen(const smappoint &pt)
{
    u_long x = 0, y = 0;
    m_core.longlatt2screen_xy(x, y, pt);
    return QPoint(static_cast<int>(x), static_cast<int>(y));
}

inline smappoint toGeo(const QPoint &p)
{
    smappoint pt;
    m_core.screen_xy2longlatt(p.x(), p.y(), pt.longt, pt.latt);
    return pt;
}

inline QColor loadColor(const QString &key, const QColor &def)
{
    const QVariant v = Settings::instance().getValue(key);
    if (v.isValid()) {
        const QColor c(v.toString());
        if (c.isValid())
            return c;
    }
    return def;
}

inline double distanceM(const smappoint &a, const smappoint &b)
{
    return CoordHandler::distanceInMetersBetweenPoints(QPair<double, double>(a.latt, a.longt),
                                                       QPair<double, double>(b.latt, b.longt));
}

inline double lengthM(const QVector<smappoint> &pts, bool closed)
{
    double len = 0;
    for (int i = 0; i + 1 < pts.size(); ++i)
        len += distanceM(pts[i], pts[i + 1]);
    if (closed && pts.size() > 2)
        len += distanceM(pts.last(), pts.first());
    return len;
}

inline double areaKm2(const QVector<smappoint> &pts)
{
    QList<QPair<double, double>> l;
    for (const smappoint &p : pts)
        l.append(QPair<double, double>(p.latt, p.longt));
    return CoordHandler::calculatePolygonArea(l)[1].toDouble();   // [1] = км²
}

// Центроид полигона (взвешенный по площади; для вырожденного - среднее вершин).
inline smappoint centroid(const QVector<smappoint> &poly)
{
    smappoint c;
    c.latt = 0;
    c.longt = 0;
    const int n = poly.size();
    if (n == 0)
        return c;

    double a2 = 0, cx = 0, cy = 0;
    for (int i = 0; i < n; ++i) {
        const smappoint &p = poly[i];
        const smappoint &q = poly[(i + 1) % n];
        const double cross = p.longt * q.latt - q.longt * p.latt;
        a2 += cross;
        cx += (p.longt + q.longt) * cross;
        cy += (p.latt + q.latt) * cross;
    }

    if (qFuzzyIsNull(a2)) {
        for (const smappoint &p : poly) { c.latt += p.latt; c.longt += p.longt; }
        c.latt /= n;
        c.longt /= n;
    } else {
        c.longt = cx / (3.0 * a2);
        c.latt  = cy / (3.0 * a2);
    }
    return c;
}

// Ray casting
inline bool pointInPolygon(const QVector<smappoint> &poly, const smappoint &pt)
{
    bool inside = false;
    for (int i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].latt > pt.latt) != (poly[j].latt > pt.latt) &&
            pt.longt < (poly[j].longt - poly[i].longt) * (pt.latt - poly[i].latt) /
                               (poly[j].latt - poly[i].latt) + poly[i].longt)
            inside = !inside;
    }
    return inside;
}

inline double distToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 == 0.0)
        return QLineF(p, a).length();
    const double t = qBound(0.0, ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2, 1.0);
    return QLineF(p, a + ab * t).length();
}

// Сколько пикселей в одном метре в окрестности точки `at` на текущем зуме.
// Нужно для рендереров, у которых размер объекта задан в метрах (гекс-сетка и т.п.).
inline double pixelsPerMeter(const smappoint &at)
{
    smappoint b = at;
    b.latt += 0.001;                         // ~111 м
    const double m = distanceM(at, b);
    if (m <= 0.0)
        return 0.0;
    return QLineF(toScreen(at), toScreen(b)).length() / m;
}

} // namespace mapgeo

// ======================================================================
// GeometryStore
// ======================================================================

// Единый источник правды для пользовательских линий и полигонов.
// DrawingLayer кладёт сюда готовые фигуры, GeometryLayer рисует и выделяет,
// MapWidget/GeometryInfoWidget подписываются на сигналы.



enum class GeometryKind { Polygon, Line };
enum class GeometryRole { None, Recon };   // Recon = область/маршрут разведки

struct GeometryItem {
    int id = -1;                                   // стабильный id (индексы "плывут" при удалении)
    GeometryKind kind = GeometryKind::Polygon;
    GeometryRole role = GeometryRole::None;
    QVector<smappoint> points;
};

class GeometryStore : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    int add(GeometryKind kind, const QVector<smappoint> &pts)
    {
        GeometryItem it;
        it.id = m_nextId++;
        it.kind = kind;
        it.points = pts;
        m_items.append(it);
        emit added(it.id);
        emit changed();
        return it.id;
    }

    bool remove(int id)
    {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].id != id)
                continue;
            const bool wasRecon = m_items[i].role == GeometryRole::Recon;
            m_items.removeAt(i);
            emit removed(id);
            if (wasRecon)
                emit reconChanged(-1);
            emit changed();
            return true;
        }
        return false;
    }

    // По умолчанию область разведки НЕ стирается кнопкой "Очистити".
    void clear(bool keepRecon = true)
    {
        const bool hadRecon = reconId() >= 0;
        QList<int> gone;
        for (int i = m_items.size() - 1; i >= 0; --i) {
            if (keepRecon && m_items[i].role == GeometryRole::Recon)
                continue;
            gone << m_items[i].id;
            m_items.removeAt(i);
        }
        if (gone.isEmpty())
            return;
        for (int id : gone)
            emit removed(id);
        if (!keepRecon && hadRecon)
            emit reconChanged(-1);
        emit changed();
    }

    const GeometryItem *find(int id) const
    {
        for (const GeometryItem &it : m_items)
            if (it.id == id)
                return &it;
        return nullptr;
    }

    const QList<GeometryItem> &items() const { return m_items; }

    // Порядковый номер (с 1) среди фигур того же типа - для подписей "Полігон #3"
    int ordinal(int id) const
    {
        const GeometryItem *target = find(id);
        if (!target)
            return 0;
        int n = 0;
        for (const GeometryItem &it : m_items) {
            if (it.kind == target->kind)
                ++n;
            if (it.id == id)
                return n;
        }
        return 0;
    }

    // id n-го (с 0) полигона/линии - для старого GeometryInfoWidget, который работает с индексами
    int idAt(GeometryKind kind, int n) const
    {
        for (const GeometryItem &it : m_items)
            if (it.kind == kind && n-- == 0)
                return it.id;
        return -1;
    }

    int reconId() const
    {
        for (const GeometryItem &it : m_items)
            if (it.role == GeometryRole::Recon)
                return it.id;
        return -1;
    }

    // Разведка одна: назначение новой снимает роль со старой.
    void assignRecon(int id)
    {
        GeometryItem *target = findMutable(id);
        if (!target || target->role == GeometryRole::Recon)
            return;
        for (GeometryItem &it : m_items)
            if (it.role == GeometryRole::Recon)
                it.role = GeometryRole::None;
        target->role = GeometryRole::Recon;
        emit reconChanged(id);
        emit changed();
    }

    void clearRecon()
    {
        GeometryItem *cur = findMutable(reconId());
        if (!cur)
            return;
        cur->role = GeometryRole::None;
        emit reconChanged(-1);
        emit changed();
    }

signals:
    void added(int id);
    void removed(int id);
    void reconChanged(int id);   // -1 = разведка не назначена
    void changed();              // любое изменение набора/ролей -> перерисовка

private:
    GeometryItem *findMutable(int id)
    {
        for (GeometryItem &it : m_items)
            if (it.id == id)
                return &it;
        return nullptr;
    }

    QList<GeometryItem> m_items;
    int m_nextId = 1;
};

// ======================================================================
// GeometryLayer
// ======================================================================

// Слой пользовательских линий и полигонов:
//  - отрисовка (обычные + область/маршрут разведки)
//  - выделение (Ctrl+ЛКМ или ПКМ), подсветка вершин/сегментов
//  - информация ТОЛЬКО оверлеем на карте (без отдельных окон-подсказок)
//  - контекстное меню, в том числе "Призначити областю розвідки"



class GeometryLayer : public MapLayer
{
    Q_OBJECT
public:
    explicit GeometryLayer(GeometryStore *store, QObject *parent = nullptr);

    void draw(QPainter &p) override;
    bool mouseMove(QMouseEvent *e) override;
    bool mouseRelease(QMouseEvent *e) override;
    bool keyPress(QKeyEvent *e) override;
    void buildContextMenu(const QPoint &pos, QMenu &menu);

    int  selectedId() const { return m_selectedId; }
    void select(int id);
    void clearSelection() { select(-1); }

signals:
    void selectionChanged(int id);

private:
    struct InfoBox {
        QString title;
        QStringList lines;
        QPoint pos;          // только для hover-подсказки (экранные координаты курсора)
        bool valid = false;
    };

    int  hitTest(const QPoint &pos) const;
    QVector<QPoint> screenPoints(const GeometryItem &it) const;
    smappoint anchorOf(const GeometryItem &it) const;
    void refreshSummary();
    void copyCoordinates(int id) const;

    void drawItem(QPainter &p, const GeometryItem &it);
    void drawSelection(QPainter &p);
    void drawInfo(QPainter &p);

    GeometryStore *m_store;

    int m_selectedId = -1;
    int m_hoverPoint = -1;
    int m_hoverSegment = -1;

    InfoBox   m_summary;          // постоянная плашка у выделенной фигуры
    smappoint m_summaryAnchor;    // в гео-координатах, чтобы ездила вместе с картой
    InfoBox   m_tip;              // подсказка под курсором (точка / отрезок)

    QImage m_pointIcon;
    QColor m_lineColor, m_polyColor, m_polyFill;
    QColor m_reconColor, m_reconFill;
    QColor m_highlight, m_hover, m_labelBg;
};

// ======================================================================
// Отрисовка угроз (ThreatRenderer)
// ======================================================================

// Стратегия отрисовки угроз. ThreatLayer хранит данные и выделение,
// а КАК они выглядят на карте решает сменный ThreatRenderer.
//
//   IconThreatRenderer      - иконка на точку (как сейчас)
//   HexGridThreatRenderer   - (позже) гекс-сетка, цвет по уровню радиации
//   ...                     - тепловая карта, кластеры и т.д.



struct ThreatRenderContext {
    QRect viewport;               // видимая область (для отсечения). При hitTest может быть пустым
    int zoom = 0;                 // текущий зум: рендереры с кэшем перестраивают его при смене зума
    int selectedIndex = -1;       // индекс выбранной угрозы в массиве данных слоя
    quint64 dataVersion = 0;      // растёт при любом изменении данных: ключ для ленивых кэшей
};

class ThreatRenderer
{
public:
    virtual ~ThreatRenderer() = default;

    virtual QString id() const = 0;

    virtual void draw(QPainter &p,
                      const QVector<smarker> &threats,
                      const ThreatRenderContext &ctx) = 0;

    // Индекс угрозы под курсором или -1. Для агрегирующих рендереров (гекс) -
    // индекс "представителя" ячейки, например самой сильной угрозы в ней.
    virtual int hitTest(const QVector<smarker> &threats,
                        const QPoint &pos,
                        const ThreatRenderContext &ctx) const = 0;
};

// ---------------------------------------------------------------------------

class IconThreatRenderer : public ThreatRenderer
{
public:
    IconThreatRenderer(QVector<QImage> icons, QColor highlight, qreal opacity = 1.0);

    QString id() const override { return QStringLiteral("icons"); }
    void draw(QPainter &p, const QVector<smarker> &threats, const ThreatRenderContext &ctx) override;
    int hitTest(const QVector<smarker> &threats, const QPoint &pos, const ThreatRenderContext &ctx) const override;

    // Набор иконок по битовой маске типа (0 = чекпоинт, 1..7 = R/C/B комбинации)
    static QVector<QImage> loadDefaultIcons();

private:
    QVector<QImage> m_icons;
    QColor m_highlight;
    qreal m_opacity;
};

// ======================================================================
// ThreatLayer
// ======================================================================

// Один класс вместо BaseThreatLayer + ThreatLayer + ArchiveThreatLayer.
// Активные и архивные угрозы - два экземпляра с разным Kind.



class ThreatLayer : public MapLayer
{
    Q_OBJECT
public:
    enum class Kind { Active, Archive };

    explicit ThreatLayer(Kind kind, QObject *parent = nullptr);

    Kind kind() const { return m_kind; }

    // Сменить способ отображения "на лету" (иконки <-> гекс-сетка ...)
    void setRenderer(std::unique_ptr<ThreatRenderer> renderer);
    const ThreatRenderer *renderer() const { return m_renderer.get(); }

    void setThreats(const QVector<smarker> &threats);
    void addThreat(const smarker &threat);
    void clear();

    int  selectedIndex() const { return m_selected; }
    void selectAndCenter(int index);     // из внешнего списка: выделить + центрировать

    void draw(QPainter &p) override;
    bool mouseRelease(QMouseEvent *e) override;

signals:
    void threatSelected(int index, const smarker &threat);
    void centerRequested(const smarker &threat);

private:
    ThreatRenderContext context() const;
    void dataTouched();

    Kind m_kind;
    std::unique_ptr<ThreatRenderer> m_renderer;
    QVector<smarker> m_threats;
    int m_selected = -1;
    quint64 m_version = 1;
    QRect m_lastViewport;
};

// ======================================================================
// VehicleLayer
// ======================================================================

// Маркер машины + пройденный путь (с градиентом прозрачности).



class VehicleLayer : public MapLayer
{
    Q_OBJECT
public:
    explicit VehicleLayer(QObject *parent = nullptr);

    void draw(QPainter &p) override;

    void updateVehiclePosition(const smappoint &pos);
    void updateVehiclePosition(const CoordPairD &pos);   // first = latt, second = longt
    smappoint getVehiclePosition() const { return m_pos; }
    bool isValidPosition() const { return m_valid; }
    void setPositionValidation(bool valid) { m_valid = valid; emit updateRequested(); }

    void setVehicleVisible(bool on) { m_vehicleVisible = on; emit updateRequested(); }
    void setPathVisible(bool on)    { m_pathVisible = on;    emit updateRequested(); }

    void onReconEnded() { clearPath(); }
    void clearPath() { m_path.clear(); emit updateRequested(); }

private:
    static constexpr int MAX_PATH_POINTS = 5000;   // раньше путь рос бесконечно

    QImage m_icon;
    smappoint m_pos;
    QList<smappoint> m_path;
    QColor m_pathColor = QColor(255, 0, 0);
    bool m_valid = false;
    bool m_vehicleVisible = true;
    bool m_pathVisible = true;
};

// ======================================================================
// SearchAreaLayer
// ======================================================================

// Область поиска (приходит снаружи строкой координат, как раньше).



class SearchAreaLayer : public MapLayer
{
    Q_OBJECT
public:
    explicit SearchAreaLayer(QObject *parent = nullptr);

    void draw(QPainter &p) override;

    void activeSearchAreaRecived(const QString &areaCoordinates);   // имя сохранено для совместимости
    void clear();

private:
    QVector<smappoint> m_area;
    QColor m_color, m_fill;
};

// ======================================================================
// MapWidget
// ======================================================================

class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);
    ~MapWidget() override;

    // --- карта ---
    bool setMapFile(const QString &path);
    void setCenter(const smappoint &pt);
    int  zoom() const;
    void setZoom(int z);

    // --- слои ---
    void addLayer(MapLayer *layer);
    void removeLayer(MapLayer *layer);

    const GeometryStore *geometry() const { return store; }

signals:
    void zoomChanged(int zoom);
    void centerPositionChanged(const smappoint &pt);

    void coordinatesArrayReady(const QVector<smappoint> &coords);   // завершена линия/полигон
    void threatClicked(int index);
    void archiveThreatClicked(int index);
    void drawingCanceled();

    // Разведка назначается через контекстное меню фигуры (вместо ReconAreaLayer)
    void reconAssigned(const QVector<smappoint> &points, bool isRoute);  // линия = маршрут, полигон = область
    void reconCleared();

public slots:
    void zoomIn();
    void zoomOut();
    void centerOnPoint(const smarker &marker);

    void onReconEnded();

    void setupActiveThreads(const QVector<smarker> &threats);
    void appendActiveThread(const smarker &threat);
    void setupdArchiveThreads(const QVector<smarker> &threats);
    void clearThreadLayerData();
    void clearArchiveThreadLayerData();
    void markSelectedDanger(int index);
    void markSelectedArchiveDanger(int index);
    void activeSearchAreaRecived(QString coordinates);

    // void onUpdateGpsPositionData(gpsData data);
    // void onUpdateAirmarPositionData(airmarData data);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupLayers();
    void setupConnections();
    void setupToolbar();
    void setupControls();
    void setupFilterMenu();
    void addFilter(const QString &text, bool checked, std::function<void(bool)> setter);

    void onDrawingModeChanged(DrawingLayer::Mode mode);
    void onReconChanged(int id);
    void syncGeometryList();
    void syncCenter();
    void adjustZoom(int delta);
    void updateCursor();
    void disableTracking();
    void updateUnifiedPositionData();

    // --- карта ---
    QPixmap *p_map = nullptr;
    smappoint centerPosition;

    // --- слои (порядок добавления = порядок отрисовки снизу вверх) ---
    QList<MapLayer *> layers;
    GeometryStore   *store = nullptr;
    SearchAreaLayer *searchLayer = nullptr;
    GeometryLayer   *geometryLayer = nullptr;
    ThreatLayer     *archiveLayer = nullptr;
    ThreatLayer     *threatLayer = nullptr;
    VehicleLayer    *vehicleLayer = nullptr;
    DrawingLayer    *drawing = nullptr;
    // GeometryInfoWidget *geometryInfoWidget = nullptr;

    // --- перетаскивание ---
    QPoint dragStartPos;
    bool isDragging = false;

    // --- панель рисования ---
    QWidget *toolbar = nullptr;
    QPushButton *btnLine = nullptr, *btnPolygon = nullptr, *btnFinish = nullptr,
                *btnUndo = nullptr, *btnCancel = nullptr, *btnClear = nullptr;
    bool drawingWasActive = false;

    // --- панель управления картой ---
    QWidget *controls = nullptr;
    QPushButton *btnZoomIn = nullptr, *btnZoomOut = nullptr, *btnTrack = nullptr,
                *btnGeomList = nullptr, *btnFilter = nullptr;
    QMenu *filterMenu = nullptr;

    // --- машина ---
    bool trackMarkerMode = false;
    bool syncingGeometryList = false;
    // gpsData lastGps;        QDateTime lastGpsUpdate;
    // airmarData lastAirmar;  QDateTime lastAirmarUpdate;

    static const int MAX_ZOOM = 16;
    static const int MIN_ZOOM = 3;
    static const int DRAG_THRESHOLD = 5;
};
