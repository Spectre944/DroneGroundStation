#include "mapwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPolygon>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cstdlib>

#include "modules/notifier/notifier.h"

// Глобальные объекты из mapcore.cpp
extern mapdraw m_draw;
extern mapcore m_core;
extern cdirprovider m_prov;

// ======================================================================
// GeometryLayer
// ======================================================================

namespace {

constexpr int POINT_TOL = 14;   // px, наведение на вершину
constexpr int SEG_TOL   = 10;   // px, наведение на отрезок / клик по линии

// Информационная плашка, рисуется прямо на карте. Если якорь вне экрана - не рисуем.
void drawInfoBox(QPainter &p, const QPoint &anchor, const QString &title,
                 const QStringList &lines, const QColor &accent, const QRect &bounds)
{
    if (!bounds.adjusted(-5, -5, 5, 5).contains(anchor))
        return;

    p.save();
    QFont f = p.font();
    f.setPointSize(9);
    f.setBold(false);
    QFont fb = f;
    fb.setBold(true);
    const QFontMetrics fm(f), fmb(fb);

    int w = fmb.horizontalAdvance(title);
    for (const QString &l : lines)
        w = qMax(w, fm.horizontalAdvance(l));

    const int pad = 8;
    QRect r(0, 0, w + 2 * pad, fmb.height() + lines.size() * fm.height() + 2 * pad);
    r.moveTopLeft(anchor + QPoint(15, 15));
    if (r.right() > bounds.right())
        r.moveRight(anchor.x() - 15);
    if (r.bottom() > bounds.bottom())
        r.moveBottom(anchor.y() - 15);
    if (r.left() < bounds.left())
        r.moveLeft(bounds.left());
    if (r.top() < bounds.top())
        r.moveTop(bounds.top());

    p.setPen(QPen(accent, 2));
    p.setBrush(QColor(255, 255, 255, 240));
    p.drawRoundedRect(r, 6, 6);

    p.setFont(fb);
    p.setPen(accent.darker(130));
    p.drawText(r.left() + pad, r.top() + pad + fmb.ascent(), title);

    p.setFont(f);
    p.setPen(QColor(0x33, 0x33, 0x33));
    int y = r.top() + pad + fmb.height() + fm.ascent();
    for (const QString &l : lines) {
        p.drawText(r.left() + pad, y, l);
        y += fm.height();
    }
    p.restore();
}

void drawPointLabel(QPainter &p, int number, const QPoint &pos, const QColor &bg)
{
    p.save();
    QFont font = p.font();
    font.setBold(true);
    font.setPointSize(10);
    p.setFont(font);

    const QString text = QString::number(number);
    QRect r = QFontMetrics(font).boundingRect(text);
    r.moveCenter(pos + QPoint(0, -25));
    r.adjust(-4, -2, 4, 2);

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, 3, 3);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, text);
    p.restore();
}

} // namespace

GeometryLayer::GeometryLayer(GeometryStore *store, QObject *parent)
    : MapLayer(parent), m_store(store)
{
    m_pointIcon = QPixmap(":/icons/svg/custom/drawPoint.svg").toImage();

    // Ключи настроек - те же, что в старом коде (вместе с опечатками)
    using mapgeo::loadColor;
    m_lineColor  = loadColor("map/colors/compeltedLineColor",        QColor(0, 128, 255, 192));
    m_polyColor  = loadColor("map/colors/compeltedPoligonColor",     QColor(137, 0, 137, 128));
    m_polyFill   = loadColor("map/colors/compeltedPoligonFillColor", QColor(137, 0, 137, 32));
    m_reconColor = loadColor("map/colors/activeReconAreaColor",      QColor(135, 0, 252, 255));
    m_reconFill  = loadColor("map/colors/activeReconAreaFillColor",  QColor(135, 0, 252, 32));
    m_highlight  = loadColor("map/colors/highlightColor",            QColor(0, 85, 255, 192));
    m_hover      = loadColor("map/colors/hoverColor",                QColor(255, 140, 0, 220));
    m_labelBg    = loadColor("map/colors/labelBackgroundColor",      QColor(0, 85, 255, 128));

    connect(m_store, &GeometryStore::removed, this, [this](int id) {
        if (id == m_selectedId)
            clearSelection();
    });
    connect(m_store, &GeometryStore::changed, this, [this] {
        refreshSummary();            // роль могла поменяться
        emit updateRequested();
    });
}

// ------------------------ ВЫДЕЛЕНИЕ ------------------------ //

void GeometryLayer::select(int id)
{
    if (id >= 0 && !m_store->find(id))
        id = -1;
    if (id == m_selectedId)
        return;

    m_selectedId = id;
    m_hoverPoint = m_hoverSegment = -1;
    m_tip = InfoBox();
    refreshSummary();
    emit selectionChanged(id);
    emit updateRequested();
}

QVector<QPoint> GeometryLayer::screenPoints(const GeometryItem &it) const
{
    QVector<QPoint> s;
    s.reserve(it.points.size());
    for (const smappoint &pt : it.points)
        s.append(mapgeo::toScreen(pt));
    return s;
}

smappoint GeometryLayer::anchorOf(const GeometryItem &it) const
{
    if (it.kind == GeometryKind::Polygon)
        return mapgeo::centroid(it.points);

    // Линия: середина среднего отрезка
    const int i = (it.points.size() - 1) / 2;
    smappoint a = it.points[i];
    const smappoint &b = it.points[i + 1];
    a.latt  = (a.latt + b.latt) / 2.0;
    a.longt = (a.longt + b.longt) / 2.0;
    return a;
}

void GeometryLayer::refreshSummary()
{
    m_summary = InfoBox();
    const GeometryItem *it = m_store->find(m_selectedId);
    if (!it || it->points.size() < 2)
        return;

    const bool poly = it->kind == GeometryKind::Polygon;
    m_summary.title = QString(poly ? "Полігон #%1" : "Лінія #%1").arg(m_store->ordinal(it->id));

    if (poly) {
        m_summary.lines << QString("Площа: %1 км²").arg(mapgeo::areaKm2(it->points), 0, 'f', 2)
                        << QString("Периметр: %1 км").arg(mapgeo::lengthM(it->points, true) / 1000.0, 0, 'f', 2);
    } else {
        m_summary.lines << QString("Довжина: %1 км").arg(mapgeo::lengthM(it->points, false) / 1000.0, 0, 'f', 2);
    }
    m_summary.lines << QString("Точок: %1").arg(it->points.size());
    if (it->role == GeometryRole::Recon)
        m_summary.lines << (poly ? "★ Область розвідки" : "★ Маршрут розвідки");

    m_summary.valid = true;
    m_summaryAnchor = anchorOf(*it);
}

int GeometryLayer::hitTest(const QPoint &pos) const
{
    const QList<GeometryItem> &items = m_store->items();

    // Сначала линии (тонкие и точные), потом полигоны, сверху вниз
    for (int i = items.size() - 1; i >= 0; --i) {
        if (items[i].kind != GeometryKind::Line)
            continue;
        const QVector<QPoint> s = screenPoints(items[i]);
        for (int j = 0; j + 1 < s.size(); ++j)
            if (mapgeo::distToSegment(pos, s[j], s[j + 1]) <= SEG_TOL)
                return items[i].id;
    }

    const smappoint geo = mapgeo::toGeo(pos);
    for (int i = items.size() - 1; i >= 0; --i)
        if (items[i].kind == GeometryKind::Polygon && mapgeo::pointInPolygon(items[i].points, geo))
            return items[i].id;

    return -1;
}

// ------------------------ INPUT ------------------------ //

bool GeometryLayer::mouseMove(QMouseEvent *e)
{
    const GeometryItem *it = m_store->find(m_selectedId);
    if (!it)
        return false;

    const QPoint pos = e->pos();
    const QVector<QPoint> s = screenPoints(*it);
    const int n = s.size();
    const bool poly = it->kind == GeometryKind::Polygon;

    int pt = -1, seg = -1;
    InfoBox tip;

    for (int i = 0; i < n; ++i) {
        if ((pos - s[i]).manhattanLength() <= POINT_TOL) {
            pt = i;
            break;
        }
    }

    if (pt >= 0) {
        tip.title = QString("Точка #%1").arg(pt + 1);
        tip.lines << QString("Широта: %1").arg(it->points[pt].latt, 0, 'f', 6)
                  << QString("Довгота: %1").arg(it->points[pt].longt, 0, 'f', 6);
        tip.valid = true;
    } else {
        const int segs = poly ? n : n - 1;
        double best = SEG_TOL + 1.0;
        for (int i = 0; i < segs; ++i) {
            const double d = mapgeo::distToSegment(pos, s[i], s[(i + 1) % n]);
            if (d <= SEG_TOL && d < best) {
                best = d;
                seg = i;
            }
        }
        if (seg >= 0) {
            const int next = (seg + 1) % n;
            tip.title = QString("Відрізок %1 → %2").arg(seg + 1).arg(next + 1);
            tip.lines << QString("Відстань: %1 м").arg(mapgeo::distanceM(it->points[seg], it->points[next]), 0, 'f', 2);
            tip.valid = true;
        }
    }

    const bool changed = (pt != m_hoverPoint) || (seg != m_hoverSegment);
    const bool tipActive = tip.valid || m_tip.valid;

    m_hoverPoint = pt;
    m_hoverSegment = seg;
    tip.pos = pos;
    m_tip = tip;

    if (changed || tipActive)
        emit updateRequested();

    return false;   // перетаскивание карты не блокируем
}

bool GeometryLayer::mouseRelease(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return false;

    const bool ctrl = e->modifiers() & Qt::ControlModifier;

    if (ctrl) {
        const int id = hitTest(e->pos());
        select(id == m_selectedId ? -1 : id);
        return id >= 0;
    }

    // Обычный клик "в пустоту" снимает выделение, но событие не съедает
    if (m_selectedId >= 0)
        clearSelection();
    return false;
}

bool GeometryLayer::keyPress(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && m_selectedId >= 0) {
        clearSelection();
        return true;
    }
    return false;
}

// ------------------------ КОНТЕКСТНОЕ МЕНЮ ------------------------ //

void GeometryLayer::buildContextMenu(const QPoint &pos, QMenu &menu)
{
    const int id = hitTest(pos);
    if (id < 0) {
        clearSelection();
        return;
    }

    select(id);
    const GeometryItem *it = m_store->find(id);
    if (!it)
        return;

    const bool poly = it->kind == GeometryKind::Polygon;

    menu.addAction("Скопіювати координати", this, [this, id] { copyCoordinates(id); });

    if (it->role == GeometryRole::Recon) {
        menu.addAction("Зняти призначення розвідки", this, [this] { m_store->clearRecon(); });
    } else {
        menu.addAction(poly ? "Призначити областю розвідки" : "Призначити маршрутом розвідки",
                       this, [this, id] { m_store->assignRecon(id); });
    }

    menu.addSeparator();
    menu.addAction(poly ? "Видалити полігон" : "Видалити лінію",
                   this, [this, id] { m_store->remove(id); });
}

void GeometryLayer::copyCoordinates(int id) const
{
    const GeometryItem *it = m_store->find(id);
    if (!it)
        return;

    // 'f', 6 - у старого кода arg(double) без формата округлял до 6 ЗНАЧАЩИХ цифр (~±10 м)
    QString text;
    for (const smappoint &pt : it->points)
        text += QString("WGS84:%1,%2/").arg(pt.latt, 0, 'f', 6).arg(pt.longt, 0, 'f', 6);
    text += "/";

    QApplication::clipboard()->setText(text, QClipboard::Clipboard);
    NotificationManager::showInfo("Координати збережено в буфер");
}

// ------------------------ DRAW ------------------------ //

void GeometryLayer::draw(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    for (const GeometryItem &it : m_store->items())
        drawItem(p, it);
    drawSelection(p);
    drawInfo(p);
}

void GeometryLayer::drawItem(QPainter &p, const GeometryItem &it)
{
    const bool recon = it.role == GeometryRole::Recon;
    const QVector<QPoint> s = screenPoints(it);
    if (s.size() < 2)
        return;

    if (it.kind == GeometryKind::Line) {
        p.setPen(QPen(recon ? m_reconColor : m_lineColor, 3));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(s.constData(), s.size());
    } else {
        p.setPen(QPen(recon ? m_reconColor : m_polyColor, recon ? 3 : 2));
        p.setBrush(recon ? m_reconFill : m_polyFill);
        p.drawPolygon(s.constData(), s.size());
    }

    // Метка "разведка" - только если фигура не выделена (у выделенной это есть в плашке)
    if (recon && it.id != m_selectedId) {
        drawInfoBox(p, mapgeo::toScreen(anchorOf(it)),
                    it.kind == GeometryKind::Polygon ? "Область розвідки" : "Маршрут розвідки",
                    {}, m_reconColor, p.viewport());
    }
}

void GeometryLayer::drawSelection(QPainter &p)
{
    const GeometryItem *it = m_store->find(m_selectedId);
    if (!it)
        return;

    const QVector<QPoint> s = screenPoints(*it);
    const int n = s.size();
    const int segs = (it->kind == GeometryKind::Polygon) ? n : n - 1;

    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < segs; ++i) {
        const bool hot = (i == m_hoverSegment);
        p.setPen(QPen(hot ? m_hover : m_highlight, hot ? 8 : 6));
        p.drawLine(s[i], s[(i + 1) % n]);
    }

    for (int i = 0; i < n; ++i) {
        if (i == m_hoverPoint) {
            p.setPen(QPen(m_hover, 3));
            p.setBrush(QColor(m_hover.red(), m_hover.green(), m_hover.blue(), 80));
            p.drawEllipse(s[i], 20, 20);
        }
        if (!m_pointIcon.isNull())
            p.drawImage(s[i] - QPoint(m_pointIcon.width() / 2, m_pointIcon.height() / 2), m_pointIcon);
        drawPointLabel(p, i + 1, s[i], m_labelBg);
    }
}

void GeometryLayer::drawInfo(QPainter &p)
{
    if (m_summary.valid)
        drawInfoBox(p, mapgeo::toScreen(m_summaryAnchor), m_summary.title, m_summary.lines,
                    m_highlight, p.viewport());
    if (m_tip.valid)
        drawInfoBox(p, m_tip.pos, m_tip.title, m_tip.lines, m_hover, p.viewport());
}

// ======================================================================
// Отрисовка угроз
// ======================================================================

IconThreatRenderer::IconThreatRenderer(QVector<QImage> icons, QColor highlight, qreal opacity)
    : m_icons(std::move(icons)), m_highlight(highlight), m_opacity(opacity)
{
}

QVector<QImage> IconThreatRenderer::loadDefaultIcons()
{
    static const char *paths[] = {
        ":/icons/svg/custom/black/checkpoint.svg",   // 0    чекпоинт, без угрозы
        ":/icons/png/aris/6_1_1R 1.png",             // 1    0001 радиация
        ":/icons/png/aris/6_1_2C 1.png",             // 2    0010 химия
        ":/icons/png/aris/6_1_4RC 1.png",            // 3    0011 радиация+химия
        ":/icons/png/aris/6_1_3B 1.png",             // 4    0100 биология
        ":/icons/png/aris/6_1_5RB 1.png",            // 5    0101 радиация+биология
        ":/icons/png/aris/6_1_6CB 1.png",            // 6    0110 химия+биология
        ":/icons/png/aris/6_1_7RCB 1.png"            // 7    0111 все три
    };

    QVector<QImage> icons;
    icons.reserve(8);
    icons.append(QPixmap(paths[0]).toImage());
    for (int i = 1; i < 8; ++i)
        icons.append(QPixmap(paths[i]).scaled(QSize(70, 70), Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation).toImage());
    return icons;
}

void IconThreatRenderer::draw(QPainter &p, const QVector<smarker> &threats, const ThreatRenderContext &ctx)
{
    p.save();
    p.setOpacity(m_opacity);

    const QRect visible = ctx.viewport.adjusted(-60, -60, 60, 60);

    for (int i = 0; i < threats.size(); ++i) {
        const int type = int(threats[i].type);
        if (type < 0 || type >= m_icons.size())
            continue;

        const QPoint c = mapgeo::toScreen(threats[i].pt);
        if (!ctx.viewport.isNull() && !visible.contains(c))
            continue;

        if (i == ctx.selectedIndex) {
            p.setPen(QPen(m_highlight, 0));
            p.setBrush(QColor(m_highlight.red(), m_highlight.green(), m_highlight.blue(), 100));
            p.drawEllipse(c, 40, 40);
        }

        const QImage &icon = m_icons[type];
        p.drawImage(c.x() - icon.width() / 2, c.y() - icon.height() / 2, icon);
    }
    p.restore();
}

int IconThreatRenderer::hitTest(const QVector<smarker> &threats, const QPoint &pos,
                                const ThreatRenderContext &) const
{
    for (int i = threats.size() - 1; i >= 0; --i) {
        if ((pos - mapgeo::toScreen(threats[i].pt)).manhattanLength() <= 20)
            return i;
    }
    return -1;
}

// ======================================================================
// ThreatLayer
// ======================================================================

ThreatLayer::ThreatLayer(Kind kind, QObject *parent)
    : MapLayer(parent), m_kind(kind)
{
    // Иконки пока одинаковые для активных и архивных; когда появятся новые -
    // подставить другой набор здесь (или передать извне через setRenderer).
    setRenderer(std::make_unique<IconThreatRenderer>(IconThreatRenderer::loadDefaultIcons(),
                                                     QColor(255, 165, 0, 200)));
}

void ThreatLayer::setRenderer(std::unique_ptr<ThreatRenderer> renderer)
{
    m_renderer = std::move(renderer);
    emit updateRequested();
}

ThreatRenderContext ThreatLayer::context() const
{
    ThreatRenderContext ctx;
    ctx.viewport = m_lastViewport;
    ctx.zoom = m_core.getzoom();
    ctx.selectedIndex = m_selected;
    ctx.dataVersion = m_version;
    return ctx;
}

void ThreatLayer::dataTouched()
{
    ++m_version;
    emit updateRequested();
}

void ThreatLayer::setThreats(const QVector<smarker> &threats)
{
    m_threats = threats;
    if (m_selected >= m_threats.size())
        m_selected = -1;
    dataTouched();
}

void ThreatLayer::addThreat(const smarker &threat)
{
    m_threats.append(threat);
    dataTouched();
}

void ThreatLayer::clear()
{
    m_threats.clear();
    m_selected = -1;
    dataTouched();
}

void ThreatLayer::selectAndCenter(int index)
{
    if (index < 0 || index >= m_threats.size())
        return;
    m_selected = index;
    emit threatSelected(index, m_threats[index]);
    emit centerRequested(m_threats[index]);
    emit updateRequested();
}

void ThreatLayer::draw(QPainter &p)
{
    m_lastViewport = p.viewport();
    if (m_threats.isEmpty() || !m_renderer)
        return;
    m_renderer->draw(p, m_threats, context());
}

bool ThreatLayer::mouseRelease(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_renderer)
        return false;

    const int idx = m_renderer->hitTest(m_threats, e->pos(), context());
    if (idx >= 0) {
        m_selected = idx;
        emit threatSelected(idx, m_threats[idx]);
        emit updateRequested();
        return true;
    }

    if (m_selected >= 0) {          // клик мимо - снять выделение, событие не съедаем
        m_selected = -1;
        emit updateRequested();
    }
    return false;
}

// ======================================================================
// VehicleLayer
// ======================================================================

VehicleLayer::VehicleLayer(QObject *parent)
    : MapLayer(parent)
{
    m_icon = QPixmap(":/icons/svg/custom/RECONNAISSANCE.svg").toImage();
    m_pos.latt = 0;
    m_pos.longt = 0;
}

void VehicleLayer::updateVehiclePosition(const smappoint &pos)
{
    // (0,0) - "нет фикса"
    if (qFuzzyIsNull(pos.latt) && qFuzzyIsNull(pos.longt)) {
        m_valid = false;
        m_pos.latt = 0;
        m_pos.longt = 0;
        emit updateRequested();
        return;
    }

    m_valid = true;
    m_pos = pos;

    // Не копим дубликаты, пока машина стоит
    if (m_path.isEmpty() ||
        qAbs(m_path.last().latt - pos.latt) > 1e-9 ||
        qAbs(m_path.last().longt - pos.longt) > 1e-9) {
        m_path.append(pos);
        if (m_path.size() > MAX_PATH_POINTS)
            m_path.removeFirst();
    }
    emit updateRequested();
}

void VehicleLayer::updateVehiclePosition(const CoordPairD &pos)
{
    smappoint pt;
    pt.latt = pos.first;
    pt.longt = pos.second;
    updateVehiclePosition(pt);
}

void VehicleLayer::draw(QPainter &p)
{
    if (m_pathVisible && m_path.size() > 1) {
        const int segments = m_path.size() - 1;
        QPoint prev = mapgeo::toScreen(m_path[0]);
        for (int i = 0; i < segments; ++i) {
            const QPoint next = mapgeo::toScreen(m_path[i + 1]);
            const double progress = double(i) / segments;       // старое - прозрачнее
            QColor c = m_pathColor;
            c.setAlpha(int(100 + 125 * progress * progress));
            p.setPen(QPen(c, 3));
            p.drawLine(prev, next);
            prev = next;
        }
    }

    if (m_vehicleVisible && m_valid) {
        const QPoint c = mapgeo::toScreen(m_pos);
        if (!m_icon.isNull()) {
            p.drawImage(c.x() - m_icon.width() / 2, c.y() - m_icon.height() / 2, m_icon);
        } else {                                                // иконка не загрузилась
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(m_pathColor);
            p.drawEllipse(c, 8, 8);
        }
    }
}

// ======================================================================
// SearchAreaLayer
// ======================================================================

SearchAreaLayer::SearchAreaLayer(QObject *parent)
    : MapLayer(parent)
{
    m_color = mapgeo::loadColor("map/colors/activeSearchAreaColor",     QColor(0, 0, 255, 128));
    m_fill  = mapgeo::loadColor("map/colors/activeSearchAreaFillColor", QColor(0, 0, 255, 32));
}

void SearchAreaLayer::activeSearchAreaRecived(const QString &areaCoordinates)
{
    m_area.clear();
    const QPair<bool, QVector<smappoint>> result =
        CoordHandler::convertPackageCoordsToMapPoints(areaCoordinates);
    if (result.first)
        m_area = result.second;
    emit updateRequested();
}

void SearchAreaLayer::clear()
{
    m_area.clear();
    emit updateRequested();
}

void SearchAreaLayer::draw(QPainter &p)
{
    if (m_area.size() < 3)
        return;

    QPolygon poly;
    for (const smappoint &pt : m_area)
        poly << mapgeo::toScreen(pt);

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(m_color, 2));
    p.setBrush(m_fill);
    p.drawPolygon(poly);
}

// ======================================================================
// MapWidget
// ======================================================================

// Глобальные объекты из mapcore.cpp

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);   // для Esc / Enter / Backspace

    p_map = new QPixmap(4096, 4096);

    // Заглушка для отсутствующих тайлов
    QImage empty(TILE_SIZE, TILE_SIZE, QImage::Format_RGB32);
    empty.fill(QColor(220, 220, 220));
    m_prov.setEmptyTileIcon(empty);

    centerPosition = m_core.getview();

    setupLayers();
    setupToolbar();        // слева сверху: рисование
    setupControls();       // справа сверху: zoom / автотрекинг / список фигур / фильтр
    setupFilterMenu();
    setupConnections();
}

MapWidget::~MapWidget()
{
    delete p_map;
}

// ------------------------ КАРТА ------------------------ //

bool MapWidget::setMapFile(const QString &path)
{
    if (path.isEmpty())
        return false;

    const bool ok = m_prov.openTileDB(path.toUtf8().constData());
    update();
    return ok;
}

void MapWidget::setCenter(const smappoint &pt)
{
    m_core.setview(pt);
    syncCenter();
    update();
}

int MapWidget::zoom() const
{
    return m_core.getzoom();
}

void MapWidget::setZoom(int z)
{
    z = qBound(MIN_ZOOM, z, MAX_ZOOM);
    if (z == m_core.getzoom())
        return;
    m_core.setzoom(z);
    emit zoomChanged(z);
    update();
}

void MapWidget::zoomIn()  { adjustZoom(+120); }
void MapWidget::zoomOut() { adjustZoom(-120); }

void MapWidget::adjustZoom(int delta)
{
    setZoom(m_core.getzoom() + (delta > 0 ? 1 : -1));
}

void MapWidget::syncCenter()
{
    centerPosition = m_core.getview();
    emit centerPositionChanged(centerPosition);
}

void MapWidget::centerOnPoint(const smarker &marker)
{
    disableTracking();          // иначе следующий GPS-тик вернёт карту обратно на машину
    setCenter(marker.pt);
}

// ------------------------ СЛОИ ------------------------ //

void MapWidget::addLayer(MapLayer *layer)
{
    if (!layer || layers.contains(layer))
        return;

    if (!layer->parent())
        layer->setParent(this);

    layers.append(layer);
    connect(layer, &MapLayer::updateRequested, this,
            QOverload<>::of(&MapWidget::update));
    update();
}

void MapWidget::removeLayer(MapLayer *layer)
{
    if (!layers.removeOne(layer))
        return;

    disconnect(layer, nullptr, this, nullptr);
    if (layer == drawing)
        drawing = nullptr;
    layer->deleteLater();
    update();
}

// Порядок добавления = порядок отрисовки снизу вверх.
// События мыши/клавиш идут в обратном порядке (сверху вниз), поэтому
// слой рисования - последний: пока он активен, остальные слои клики не получают.
void MapWidget::setupLayers()
{
    store = new GeometryStore(this);

    searchLayer   = new SearchAreaLayer(this);
    geometryLayer = new GeometryLayer(store, this);
    archiveLayer  = new ThreatLayer(ThreatLayer::Kind::Archive, this);
    threatLayer   = new ThreatLayer(ThreatLayer::Kind::Active, this);
    vehicleLayer  = new VehicleLayer(this);
    drawing       = new DrawingLayer(this);

    addLayer(searchLayer);
    addLayer(geometryLayer);
    addLayer(archiveLayer);
    addLayer(threatLayer);
    addLayer(vehicleLayer);
    addLayer(drawing);

    // geometryInfoWidget = new GeometryInfoWidget(this);
}

void MapWidget::setupConnections()
{
    // --- рисование -> хранилище фигур ---
    connect(drawing, &DrawingLayer::modeChanged, this,
            [this](DrawingLayer::Mode m) { onDrawingModeChanged(m); });

    connect(drawing, &DrawingLayer::polygonFinished, this,
            [this](const QVector<smappoint> &pts) {
                store->add(GeometryKind::Polygon, pts);
                emit coordinatesArrayReady(pts);
            });
    connect(drawing, &DrawingLayer::lineFinished, this,
            [this](const QVector<smappoint> &pts) {
                store->add(GeometryKind::Line, pts);
                emit coordinatesArrayReady(pts);
            });

    // --- разведка назначается через контекстное меню фигуры ---
    connect(store, &GeometryStore::reconChanged, this, &MapWidget::onReconChanged);

    // // --- список фигур (старый GeometryInfoWidget работает с индексами среди своего типа) ---
    // connect(store, &GeometryStore::changed, this, &MapWidget::syncGeometryList);
    // connect(geometryInfoWidget, &GeometryInfoWidget::deletePolygonRequested, this,
    //         [this](int index) { store->remove(store->idAt(GeometryKind::Polygon, index)); });
    // connect(geometryInfoWidget, &GeometryInfoWidget::deleteLineRequested, this,
    //         [this](int index) { store->remove(store->idAt(GeometryKind::Line, index)); });
    // connect(geometryInfoWidget, &GeometryInfoWidget::clearAllRequested, this,
    //         [this] { store->clear(); });

    // --- угрозы ---
    connect(threatLayer, &ThreatLayer::threatSelected, this,
            [this](int index) { emit threatClicked(index); });
    connect(archiveLayer, &ThreatLayer::threatSelected, this,
            [this](int index) { emit archiveThreatClicked(index); });
    connect(threatLayer,  &ThreatLayer::centerRequested, this, &MapWidget::centerOnPoint);
    connect(archiveLayer, &ThreatLayer::centerRequested, this, &MapWidget::centerOnPoint);
}

// ------------------------ DRAW ------------------------ //

void MapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    m_core.setcreensize(width(), height());

    QPainter p(p_map);
    m_draw.draw(p);
    p.end();

    const QRect screen(0, 0, width(), height());
    const QRect src(m_core.offs_map.x % TILE_SIZE,
                    m_core.offs_map.y % TILE_SIZE,
                    width(), height());
    painter.drawPixmap(screen, *p_map, src);

    // Перекрестие в центре
    painter.setPen(QPen(Qt::red, 2));
    const QPoint c = rect().center();
    painter.drawLine(c.x() - 10, c.y(), c.x() + 10, c.y());
    painter.drawLine(c.x(), c.y() - 10, c.x(), c.y() + 10);

    // Слои - в порядке добавления. save/restore: слой не должен "протекать"
    // пером/кистью/antialiasing в соседние (в старом коде это было).
    for (MapLayer *layer : layers) {
        if (!layer->isVisible())
            continue;
        painter.save();
        layer->draw(painter);
        painter.restore();
    }
}

// ------------------------ INPUT ------------------------ //

void MapWidget::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (std::abs(delta) >= 16)
        adjustZoom(delta);
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    // события слоям - сверху вниз
    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->mousePress(event)) {
            update();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        dragStartPos = event->pos();
        isDragging = false;
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->mouseMove(event))
            return;
    }

    if (!(event->buttons() & Qt::LeftButton))
        return;

    if (!isDragging &&
        (event->pos() - dragStartPos).manhattanLength() > DRAG_THRESHOLD) {
        isDragging = true;
        disableTracking();      // ручное перемещение выключает автотрекинг
        updateCursor();
    }

    if (isDragging) {
        const QPoint d = event->pos() - dragStartPos;
        dragStartPos = event->pos();

        if (std::abs(d.x()) > 1) m_core.scroll_h(-d.x());
        if (std::abs(d.y()) > 1) m_core.scroll_v(-d.y());

        syncCenter();
        update();
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasDragging = isDragging;

    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        updateCursor();
    }

    // после перетаскивания карты "клик" слоям не отдаём
    if (wasDragging)
        return;

    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->mouseRelease(event)) {
            update();
            return;
        }
    }
}

void MapWidget::keyPressEvent(QKeyEvent *event)
{
    for (int i = layers.size() - 1; i >= 0; --i) {
        if (layers[i]->isVisible() && layers[i]->keyPress(event)) {
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

// Одно меню на всех: каждый слой (сверху вниз) добавляет свои пункты,
// между блоками разных слоёв ставится разделитель.
void MapWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // Во время рисования ПКМ - это "закончить", а не меню (решает DrawingLayer)
    if (drawing && drawing->isActive()) {
        event->ignore();
        return;
    }

    QMenu menu(this);
    for (int i = layers.size() - 1; i >= 0; --i) {
        if (!layers[i]->isVisible())
            continue;

        QAction *sep = menu.isEmpty() ? nullptr : menu.addSeparator();
        const int before = menu.actions().size();
        layers[i]->buildContextMenu(event->pos(), menu);
        if (sep && menu.actions().size() == before)
            menu.removeAction(sep);       // слой ничего не добавил
    }

    if (!menu.isEmpty()) {
        menu.exec(event->globalPos());
        event->accept();
        update();
    }
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_core.setcreensize(event->size().width(), event->size().height());

    if (controls)
        controls->move(width() - controls->width() - 10, 10);

    update();
}

void MapWidget::updateCursor()
{
    if (isDragging)
        setCursor(Qt::ClosedHandCursor);
    else if (drawing && drawing->isActive())
        setCursor(Qt::CrossCursor);
    else
        setCursor(Qt::ArrowCursor);
}

// ------------------- ПАНЕЛЬ РИСОВАНИЯ ------------------- //

void MapWidget::setupToolbar()
{
    toolbar = new QWidget(this);
    toolbar->setObjectName("mapToolbar");
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    toolbar->setAttribute(Qt::WA_NoMousePropagation, true);  // клики по панели не двигают карту
    toolbar->setStyleSheet("#mapToolbar { background: rgba(255,255,255,220); border-radius: 6px; }");

    auto *lay = new QHBoxLayout(toolbar);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    auto makeBtn = [&](const QString &text, bool checkable) {
        auto *b = new QPushButton(text, toolbar);
        b->setCheckable(checkable);
        b->setFocusPolicy(Qt::NoFocus);   // чтобы не уводить фокус с карты
        lay->addWidget(b);
        return b;
    };

    btnLine    = makeBtn("Лінія", true);
    btnPolygon = makeBtn("Полігон", true);
    btnFinish  = makeBtn("Закінчити", false);
    btnUndo    = makeBtn("Видалити точку", false);
    btnCancel  = makeBtn("Відміна", false);
    btnClear   = makeBtn("Очистити", false);

    // Кнопка-переключатель: нажали - начали, нажали ещё раз - отменили
    connect(btnLine, &QPushButton::clicked, this, [this](bool on) {
        if (on) drawing->startLine(); else drawing->cancel();
    });
    connect(btnPolygon, &QPushButton::clicked, this, [this](bool on) {
        if (on) drawing->startPolygon(); else drawing->cancel();
    });
    connect(btnFinish, &QPushButton::clicked, this, [this] { drawing->finish(); });
    connect(btnUndo,   &QPushButton::clicked, this, [this] { drawing->undoLastPoint(); });
    connect(btnCancel, &QPushButton::clicked, this, [this] { drawing->cancel(); });
    connect(btnClear,  &QPushButton::clicked, this, [this] {
        drawing->clear();
        store->clear();       // область разведки по умолчанию остаётся (см. GeometryStore::clear)
    });

    toolbar->move(10, 10);
    onDrawingModeChanged(DrawingLayer::Mode::None);
    toolbar->show();
    toolbar->raise();
}

void MapWidget::onDrawingModeChanged(DrawingLayer::Mode mode)
{
    const bool active = (mode != DrawingLayer::Mode::None);

    btnLine->setChecked(mode == DrawingLayer::Mode::Line);
    btnPolygon->setChecked(mode == DrawingLayer::Mode::Polygon);

    // "Закінчити / Видалити точку / Відміна" видны только во время рисования
    btnFinish->setVisible(active);
    btnUndo->setVisible(active);
    btnCancel->setVisible(active);

    toolbar->adjustSize();
    updateCursor();

    if (active)
        setFocus();   // чтобы работали горячие клавиши
    else if (drawingWasActive)
        emit drawingCanceled();   // как и раньше: и при отмене, и при завершении
    drawingWasActive = active;
}

// ------------------- ПАНЕЛЬ УПРАВЛЕНИЯ КАРТОЙ ------------------- //

void MapWidget::setupControls()
{
    controls = new QWidget(this);
    controls->setObjectName("mapControls");
    controls->setAttribute(Qt::WA_StyledBackground, true);
    controls->setAttribute(Qt::WA_NoMousePropagation, true);
    controls->setStyleSheet("#mapControls { background: rgba(255,255,255,220); border-radius: 6px; }");

    auto *lay = new QVBoxLayout(controls);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    auto makeBtn = [&](const QString &icon, const QString &tip, bool checkable) {
        auto *b = new QPushButton(controls);
        b->setIcon(IconHelper::createThemedIcon(icon));
        b->setIconSize(QSize(20, 20));
        b->setFixedSize(32, 32);
        b->setToolTip(tip);
        b->setCheckable(checkable);
        b->setFocusPolicy(Qt::NoFocus);
        lay->addWidget(b);
        return b;
    };

    btnZoomIn   = makeBtn(":/icons/svg/bs5/plus.svg",          "Збільшити масштаб", false);
    btnZoomOut  = makeBtn(":/icons/svg/bs5/dash.svg",          "Зменшити масштаб", false);
    btnTrack    = makeBtn(":/icons/svg/custom/GPSTrack.svg",   "Центрувати карту на машині", true);
    btnGeomList = makeBtn(":/icons/svg/bs5/list.svg",          "Управління полігонами та лініями", false);
    btnFilter   = makeBtn(":/icons/svg/bs5/layers-half.svg",   "Відображення об'єктів", false);

    connect(btnZoomIn,  &QPushButton::clicked, this, &MapWidget::zoomIn);
    connect(btnZoomOut, &QPushButton::clicked, this, &MapWidget::zoomOut);

    connect(btnTrack, &QPushButton::toggled, this, [this](bool on) {
        trackMarkerMode = on;
        if (on && vehicleLayer->isValidPosition())
            setCenter(vehicleLayer->getVehiclePosition());
    });

    // connect(btnGeomList, &QPushButton::clicked, this, [this] {
    //     geometryInfoWidget->show();
    //     geometryInfoWidget->raise();
    //     geometryInfoWidget->activateWindow();
    // });

    controls->adjustSize();
    controls->move(width() - controls->width() - 10, 10);
    controls->show();
    controls->raise();
}

// Фильтр отображаемых объектов: каждый пункт - отдельный флаг видимости.
// Новый объект/слой = одна строка addFilter().
void MapWidget::setupFilterMenu()
{
    filterMenu = new QMenu(this);
    btnFilter->setMenu(filterMenu);

    addFilter("Активні загрози",   true, [this](bool on) { threatLayer->setVisible(on); });
    addFilter("Архівні дані",      true, [this](bool on) { archiveLayer->setVisible(on); });
    addFilter("Маркер машини",     true, [this](bool on) { vehicleLayer->setVehicleVisible(on); });
    addFilter("Шлях машини",       true, [this](bool on) { vehicleLayer->setPathVisible(on); });
    addFilter("Область пошуку",    true, [this](bool on) { searchLayer->setVisible(on); });
    addFilter("Фігури користувача", true, [this](bool on) { geometryLayer->setVisible(on); });
}

void MapWidget::addFilter(const QString &text, bool checked, std::function<void(bool)> setter)
{
    QAction *a = filterMenu->addAction(text);
    a->setCheckable(true);
    a->setChecked(checked);
    connect(a, &QAction::toggled, this, [this, setter](bool on) {
        setter(on);
        update();
    });
}

void MapWidget::disableTracking()
{
    if (btnTrack && btnTrack->isChecked())
        btnTrack->setChecked(false);      // toggled сбросит trackMarkerMode
}

// ------------------- ФИГУРЫ ПОЛЬЗОВАТЕЛЯ / РАЗВЕДКА ------------------- //

void MapWidget::syncGeometryList()
{
    // if (!geometryInfoWidget || syncingGeometryList)
    //     return;

    // // GeometryInfoWidget хранит свой список, поэтому пересобираем его из хранилища:
    // // иначе удаление из контекстного меню рассинхронизирует список.
    // syncingGeometryList = true;
    // geometryInfoWidget->clearAll();
    // for (const GeometryItem &it : store->items()) {
    //     if (it.kind == GeometryKind::Polygon)
    //         geometryInfoWidget->addPolygon(it.points);
    //     else
    //         geometryInfoWidget->addLine(it.points);
    // }
    // syncingGeometryList = false;
}

void MapWidget::onReconChanged(int id)
{
    if (const GeometryItem *it = store->find(id))
        emit reconAssigned(it->points, it->kind == GeometryKind::Line);
    else
        emit reconCleared();
}

void MapWidget::onReconEnded()
{
    clearThreadLayerData();
    vehicleLayer->onReconEnded();
    store->clearRecon();       // фигура остаётся на карте как обычная
}

// ------------------------ УГРОЗЫ / ПОИСК ------------------------ //

void MapWidget::setupActiveThreads(const QVector<smarker> &threats)   { threatLayer->setThreats(threats); }
void MapWidget::appendActiveThread(const smarker &threat)             { threatLayer->addThreat(threat); }
void MapWidget::setupdArchiveThreads(const QVector<smarker> &threats) { archiveLayer->setThreats(threats); }

void MapWidget::clearThreadLayerData()
{
    threatLayer->clear();
}

void MapWidget::clearArchiveThreadLayerData()
{
    archiveLayer->clear();
    searchLayer->clear();
}

void MapWidget::markSelectedDanger(int index)        { threatLayer->selectAndCenter(index); }
void MapWidget::markSelectedArchiveDanger(int index) { archiveLayer->selectAndCenter(index); }

void MapWidget::activeSearchAreaRecived(QString coordinates)
{
    searchLayer->activeSearchAreaRecived(coordinates);
}

// ------------------------ МАШИНА ------------------------ //

// void MapWidget::onUpdateGpsPositionData(gpsData data)
// {
//     lastGps = data;
//     lastGpsUpdate = QDateTime::currentDateTime();
//     updateUnifiedPositionData();
// }

// void MapWidget::onUpdateAirmarPositionData(airmarData data)
// {
//     lastAirmar = data;
//     lastAirmarUpdate = QDateTime::currentDateTime();
//     updateUnifiedPositionData();
// }

// void MapWidget::updateUnifiedPositionData()
// {
//     const auto now = QDateTime::currentDateTime();
//     const bool gpsAlive    = lastGpsUpdate.isValid()   && lastGpsUpdate.msecsTo(now)   < GPS_DATA_TIMEOUT_MS;
//     const bool airmarAlive = lastAirmarUpdate.isValid() && lastAirmarUpdate.msecsTo(now) < GPS_DATA_TIMEOUT_MS;

//     // Приоритет: GPS, потом Airmar
//     const bool useGps    = gpsAlive && lastGps.coordFlag;
//     const bool useAirmar = !useGps && airmarAlive && lastAirmar.coordFlag;

//     if (useGps || useAirmar) {
//         vehicleLayer->updateVehiclePosition(useGps ? lastGps.coord : lastAirmar.coord);

//         if (trackMarkerMode && vehicleLayer->isValidPosition()) {
//             m_core.setview(vehicleLayer->getVehiclePosition());
//             syncCenter();
//         }
//     } else {
//         vehicleLayer->setPositionValidation(false);
//     }
// }
