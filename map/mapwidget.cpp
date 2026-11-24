#include "mapwidget.h"
#include "ui_mapwidget.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace {
const char KEY_COORD_TYPE[] = "interface/mapCoordType";   // формат координат в панели над картой
}

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
#include "dosetestflight.h"

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
    if (id == m_selectedId && !m_activeSelected)
        return;

    m_activeSelected = false;
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

QVector<smappoint> mapgeo::simplifyPolygon(const QVector<smappoint> &poly, int maxPoints)
{
    if (maxPoints < 3 || poly.size() <= maxPoints)
        return poly;

    // локальная плоская система в метрах
    const double lat0 = qDegreesToRadians(poly.first().latt);
    const double kx = 111320.0 * std::cos(lat0), ky = 111320.0;
    QVector<QPointF> xy;
    xy.reserve(poly.size());
    for (const smappoint &p : poly)
        xy << QPointF(p.longt * kx, p.latt * ky);

    QVector<int> idx(poly.size());
    for (int i = 0; i < idx.size(); ++i)
        idx[i] = i;

    auto tri = [&](int k) {
        const int n = idx.size();
        const QPointF &a = xy[idx[(k + n - 1) % n]], &b = xy[idx[k]], &c = xy[idx[(k + 1) % n]];
        return std::fabs((b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y())) / 2.0;
    };

    while (idx.size() > maxPoints) {
        int best = 0;
        double bestA = tri(0);
        for (int k = 1; k < idx.size(); ++k) {
            const double a = tri(k);
            if (a < bestA) {
                bestA = a;
                best = k;
            }
        }
        idx.removeAt(best);
    }

    QVector<smappoint> out;
    out.reserve(idx.size());
    for (int i : idx)
        out << poly[i];
    return out;
}

void GeometryLayer::refreshSummary()
{
    m_summary = InfoBox();

    // Выделена активная область (её нет в GeometryStore — только вершины от логера)
    if (m_activeSelected && m_active.size() >= 3) {
        m_summary.title = QStringLiteral("Активна область розвідки");
        m_summary.lines << reconStatusLines(nullptr)
                        << QString("Площа: %1 км²").arg(mapgeo::areaKm2(m_active), 0, 'f', 2)
                        << QString("Периметр: %1 км").arg(mapgeo::lengthM(m_active, true) / 1000.0, 0, 'f', 2)
                        << QString("Точок: %1").arg(m_active.size());
        m_summary.valid = true;
        m_summaryAnchor = mapgeo::centroid(m_active);
        return;
    }

    const GeometryItem *it = m_store->find(m_selectedId);
    if (!it || it->points.size() < 2)
        return;

    const bool poly = it->kind == GeometryKind::Polygon;
    const bool recon = it->role == GeometryRole::Recon;
    if (recon && poly)
        m_summary.title = sameAsActive(*it) ? QStringLiteral("Активна область розвідки")
                                            : QStringLiteral("Чернетка області розвідки");
    else if (recon)
        m_summary.title = QStringLiteral("Маршрут розвідки (чернетка)");
    else
        m_summary.title = QString(poly ? "Полігон #%1" : "Лінія #%1").arg(m_store->ordinal(it->id));
    if (recon)
        m_summary.lines << reconStatusLines(it);

    if (poly) {
        m_summary.lines << QString("Площа: %1 км²").arg(mapgeo::areaKm2(it->points), 0, 'f', 2)
                        << QString("Периметр: %1 км").arg(mapgeo::lengthM(it->points, true) / 1000.0, 0, 'f', 2);
    } else {
        m_summary.lines << QString("Довжина: %1 км").arg(mapgeo::lengthM(it->points, false) / 1000.0, 0, 'f', 2);
    }
    m_summary.lines << QString("Точок: %1").arg(it->points.size());
    if (poly && it->points.size() > CoreConst::AreaMaxVertices)
        m_summary.lines << QString("⚠ Для розвідки — не більше %1 точок (ПКМ → «Спростити»)")
                               .arg(CoreConst::AreaMaxVertices);
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
        if (m_coordType == CoordType::WGS84_String)
            tip.lines << QString("Широта: %1").arg(it->points[pt].latt, 0, 'f', 6)
                      << QString("Довгота: %1").arg(it->points[pt].longt, 0, 'f', 6);
        else
            tip.lines << formatMapPoint(it->points[pt], m_coordType);
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
        if (id < 0 && activeContains(e->pos())) {
            const bool was = m_activeSelected;
            select(-1);
            m_activeSelected = !was;     // Ctrl+клик по активной области — выделить / снять
            refreshSummary();
            emit updateRequested();
            return true;
        }
        select(id == m_selectedId ? -1 : id);
        return id >= 0;
    }

    // Обычный клик "в пустоту" снимает выделение, но событие не съедает
    if (m_selectedId >= 0 || m_activeSelected)
        clearSelection();
    return false;
}

bool GeometryLayer::keyPress(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && (m_selectedId >= 0 || m_activeSelected)) {
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
        if (activeContains(pos)) {
            // Активная область — только сведения и координаты (удалять/менять нельзя)
            m_activeSelected = true;
            refreshSummary();
            emit updateRequested();
            const QVector<smappoint> pts = m_active;
            const CoordType type = m_coordType;
            menu.addAction("Скопіювати координати активної області", this, [pts, type] {
                QString text;
                for (const smappoint &pt : pts)
                    text += QString("WGS84:%1,%2/").arg(pt.latt, 0, 'f', 6).arg(pt.longt, 0, 'f', 6);
                text += "/";
                if (type != CoordType::WGS84_String) {
                    const QPair<bool, QString> conv = CoordHandler::convertPackageCoords(text, type);
                    if (conv.first)
                        text = conv.second;
                }
                QApplication::clipboard()->setText(text);
            });
        }
        return;
    }

    select(id);
    const GeometryItem *it = m_store->find(id);
    if (!it)
        return;

    const bool poly = it->kind == GeometryKind::Polygon;

    menu.addAction("Скопіювати координати", this, [this, id] { copyCoordinates(id); });

    // Логер принимает область из 3–20 вершин (ID04). Рисовать больше не запрещаем,
    // но назначить такую область разведкой можно только через упрощение - без перерисовки.
    const int maxV = CoreConst::AreaMaxVertices;
    const bool tooMany = poly && it->points.size() > maxV;

    // «Підготувати» = заполнить страницу «Розвідка» и открыть её. Дрону уходит только
    // по кнопке «Відправити на дрон (ID04)» на странице — после ответа область станет активной.
    if (it->role == GeometryRole::Recon) {
        menu.addAction("Відкрити сторінку «Розвідка»", this, [this] { emit reconPrepareRequested(); });
        menu.addAction("Зняти призначення розвідки", this, [this] { m_store->clearRecon(); });
    } else if (tooMany) {
        menu.addAction(QString("Спростити до %1 точок і підготувати для розвідки").arg(maxV),
                       this, [this, id] {
                           simplify(id);
                           m_store->assignRecon(id);
                           emit reconPrepareRequested();
                       });
    } else {
        menu.addAction(poly ? "Підготувати для розвідки" : "Підготувати маршрут для розвідки",
                       this, [this, id] {
                           m_store->assignRecon(id);
                           emit reconPrepareRequested();
                       });
    }
    if (tooMany)
        menu.addAction(QString("Спростити до %1 точок").arg(maxV), this, [this, id] { simplify(id); });

    // Область разведки из меню не удаляется — сначала «Зняти призначення»
    if (it->role != GeometryRole::Recon) {
        menu.addSeparator();
        menu.addAction(poly ? "Видалити полігон" : "Видалити лінію",
                       this, [this, id] { m_store->remove(id); });
    }
}

void GeometryLayer::simplify(int id)
{
    const GeometryItem *it = m_store->find(id);
    if (!it || it->kind != GeometryKind::Polygon)
        return;
    const int before = it->points.size();
    const double areaBefore = mapgeo::areaKm2(it->points);
    const QVector<smappoint> pts = mapgeo::simplifyPolygon(it->points, CoreConst::AreaMaxVertices);
    if (pts.size() == before)
        return;

    m_store->setPoints(id, pts);
    refreshSummary();
    emit updateRequested();

    const double areaAfter = mapgeo::areaKm2(pts);
    const double diff = areaBefore > 0 ? std::fabs(areaAfter - areaBefore) / areaBefore * 100.0 : 0.0;
    NotificationManager::showSuccess(
        QString("Точок: %1 → %2, площа змінилась на %3%")
            .arg(before).arg(pts.size()).arg(diff, 0, 'f', diff < 1 ? 2 : 1),
        "Полігон спрощено");
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

    // В формат, выбранный в панели над картой
    if (m_coordType != CoordType::WGS84_String) {
        const QPair<bool, QString> conv = CoordHandler::convertPackageCoords(text, m_coordType);
        if (conv.first)
            text = conv.second;
    }

    QApplication::clipboard()->setText(text, QClipboard::Clipboard);
    NotificationManager::showInfo("Координати збережено в буфер");
}

// ------------------------ DRAW ------------------------ //

void GeometryLayer::draw(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    drawActive(p);
    for (const GeometryItem &it : m_store->items())
        drawItem(p, it);
    drawSelection(p);
    drawInfo(p);
}

void GeometryLayer::drawItem(QPainter &p, const GeometryItem &it)
{
    const bool recon = it.role == GeometryRole::Recon;
    if (recon && sameAsActive(it))
        return;                     // черновик совпадает с принятой логером — рисуется активная

    const QVector<QPoint> s = screenPoints(it);
    if (s.size() < 2)
        return;

    // Черновик разведки — пунктир; цвет по проверке ядра: зелёный годится, красный — нет
    QColor reconColor = m_reconColor, reconFill = m_reconFill;
    if (recon && m_reconCheck >= 0) {
        reconColor = m_reconCheck ? QColor(0x2E, 0x9E, 0x6A) : QColor(0xE5, 0x48, 0x4D);
        reconFill = reconColor;
        reconFill.setAlpha(m_reconFill.alpha() / 2);
    }

    if (it.kind == GeometryKind::Line) {
        p.setPen(QPen(recon ? reconColor : m_lineColor, 3, recon ? Qt::DashLine : Qt::SolidLine));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(s.constData(), s.size());
    } else {
        p.setPen(QPen(recon ? reconColor : m_polyColor, recon ? 3 : 2, recon ? Qt::DashLine : Qt::SolidLine));
        p.setBrush(recon ? reconFill : m_polyFill);
        p.drawPolygon(s.constData(), s.size());
    }

    // Вершина, к которой относится ошибка
    if (recon && m_reconCheck == 0 && m_reconBadVertex >= 0 && m_reconBadVertex < s.size()) {
        const QPoint v = s[m_reconBadVertex];
        p.setBrush(QColor(0xE5, 0x48, 0x4D, 70));
        p.setPen(QPen(Qt::white, 5));
        p.drawEllipse(v, 13, 13);
        p.setPen(QPen(QColor(0xE5, 0x48, 0x4D), 3));
        p.drawEllipse(v, 13, 13);
        drawPointLabel(p, m_reconBadVertex + 1, v, QColor(0xE5, 0x48, 0x4D));
    }

    // Постоянной подписи нет: черновик узнаётся по пунктиру, статус — в плашке при выделении
}

bool GeometryLayer::sameAsActive(const GeometryItem &it) const
{
    if (m_active.isEmpty() || it.kind != GeometryKind::Polygon || it.points.size() != m_active.size())
        return false;
    for (int i = 0; i < m_active.size(); ++i)
        if (std::fabs(it.points[i].latt - m_active[i].latt) > 1e-6 ||
            std::fabs(it.points[i].longt - m_active[i].longt) > 1e-6)
            return false;
    return true;
}

void GeometryLayer::setActiveRecon(const QVector<smappoint> &points, const QDateTime &acceptedAt,
                                   const QStringList &params)
{
    m_active = points;
    m_activeAt = acceptedAt;
    m_activeParams = params;
    if (m_active.isEmpty())
        m_activeSelected = false;
    refreshSummary();
    emit updateRequested();
}

// Активная область — сплошной контур цветом разведки (как раньше вся область разведки)
void GeometryLayer::drawActive(QPainter &p)
{
    if (m_active.size() < 3)
        return;
    QVector<QPoint> s;
    s.reserve(m_active.size());
    for (const smappoint &pt : std::as_const(m_active))
        s << mapgeo::toScreen(pt);

    if (m_activeSelected) {
        p.setPen(QPen(m_highlight, 6));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(s.constData(), s.size());
    }
    p.setPen(QPen(m_reconColor, 3));
    p.setBrush(m_reconFill);
    p.drawPolygon(s.constData(), s.size());
}

bool GeometryLayer::activeContains(const QPoint &pos) const
{
    return m_active.size() >= 3 && mapgeo::pointInPolygon(m_active, mapgeo::toGeo(pos));
}

QStringList GeometryLayer::reconStatusLines(const GeometryItem *it) const
{
    // Активная: логер принял именно эти вершины
    if (!it || sameAsActive(*it)) {
        const QString time = m_activeAt.isValid() ? m_activeAt.toLocalTime().toString("HH:mm") : QString();
        QStringList l{ time.isEmpty() ? QStringLiteral("Прийнята логером")
                                      : QStringLiteral("Прийнята логером о %1").arg(time) };
        return l + m_activeParams;
    }
    QStringList l{ QStringLiteral("Ще не відправлено на дрон") };
    if (m_reconCheck == 1)
        l << QStringLiteral("✓ Перевірку пройдено");
    else if (m_reconCheck == 0 && !m_reconError.isEmpty())
        l << QStringLiteral("✕ ") + m_reconError;
    else
        l << QStringLiteral("Перевірка…");
    return l;
}

void GeometryLayer::setReconCheck(int state, int vertex, const QString &text)
{
    m_reconCheck = state;
    m_reconBadVertex = vertex;
    m_reconError = text;
    refreshSummary();
    emit updateRequested();
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
    setupButtons();        // слева: рисование, справа: слои / zoom / автотрекинг, сверху: координаты
    setupFilterMenu();
    setupConnections();
    syncCenter();          // заполнить поле координат
}

MapWidget::~MapWidget()
{
    delete ui;
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

QString formatMapPoint(const smappoint &pt, CoordType type)
{
    const QString wgs = QString("WGS84:%1,%2").arg(pt.latt, 0, 'f', 6).arg(pt.longt, 0, 'f', 6);
    if (type == CoordType::WGS84_String)
        return wgs;
    const QPair<bool, QString> conv = CoordHandler::convertSingleCoord(wgs, type);
    return conv.first ? conv.second : wgs;
}

void MapWidget::syncCenter()
{
    centerPosition = m_core.getview();
    if (ui)
        ui->currentCoordLineEdit->setText(formatMapPoint(centerPosition, coordType));
    emit centerPositionChanged(centerPosition);
}

void MapWidget::setupCoordTypeBox()
{
    QComboBox *box = ui->coordTypeComboBox;
    box->addItem("WGS84", QVariant::fromValue(CoordType::WGS84_String));
    box->addItem("MGRS",  QVariant::fromValue(CoordType::MGRS));
    box->addItem("UTM",   QVariant::fromValue(CoordType::UTM));
    box->addItem("DMS",   QVariant::fromValue(CoordType::DMS));
    box->addItem("СК-42", QVariant::fromValue(CoordType::CK42));

    // Последний выбранный формат
    const int saved = Settings::instance().getValue(KEY_COORD_TYPE).toInt();
    const int idx = box->findData(QVariant::fromValue(static_cast<CoordType>(saved)));
    box->setCurrentIndex(idx >= 0 ? idx : 0);
    setCoordType(box->currentData().value<CoordType>());

    connect(box, &QComboBox::currentIndexChanged, this, [this, box](int) {
        setCoordType(box->currentData().value<CoordType>());
        Settings::instance().setValue(KEY_COORD_TYPE, static_cast<int>(coordType));
        Settings::instance().saveSettings();
    });
}

void MapWidget::setCoordType(CoordType type)
{
    coordType = type;
    if (geometryLayer)
        geometryLayer->setCoordType(type);
    if (measurementLayer)
        measurementLayer->setCoordType(type);
    syncCenter();
}

void MapWidget::copyCenterCoordinates()
{
    QApplication::clipboard()->setText(ui->currentCoordLineEdit->text());
    NotificationManager::showInfo("Координати скопійовано");
}

void MapWidget::centerOnPoint(const smarker &marker)
{
    disableTracking();          // иначе следующий GPS-тик вернёт карту обратно на машину
    setCenter(marker.pt);
}



void MapWidget::updateDronePosition(double lat, double lon)
{
    smappoint pt;
    pt.latt  = lat;
    pt.longt = lon;
    vehicleLayer->updateVehiclePosition(pt);   // иконка + путь, перерисовка слоя

    if (trackMarkerMode && vehicleLayer->isValidPosition())
        setCenter(pt);                          // карта следует за дроном
}


// ------------------------ РИСОВАНИЕ ------------------------ //

void MapWidget::startPolygonDrawing() { if (drawing) drawing->startPolygon(); }
void MapWidget::startLineDrawing()    { if (drawing) drawing->startLine(); }
void MapWidget::cancelDrawing()       { if (drawing) drawing->cancel(); }
bool MapWidget::isDrawing() const     { return drawing && drawing->isActive(); }

void MapWidget::setLeftInset(int px)
{
    // Сдвигаем всю сетку кнопок: левая колонка уходит из-под страницы,
    // поле координат центрируется в оставшейся части карты
    QMargins m = ui->verticalLayout->contentsMargins();
    m.setLeft(qMax(0, px) + defaultLeftMargin);
    ui->verticalLayout->setContentsMargins(m);
    if (doseLegend)
        doseLegend->setLeftInset(qMax(0, px));
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

    searchLayer      = new SearchAreaLayer(this);
    doseGridLayer    = new DoseGridLayer(&doseScale, this);
    geometryLayer    = new GeometryLayer(store, this);
    archiveLayer     = new ThreatLayer(ThreatLayer::Kind::Archive, this);
    measurementLayer = new MeasurementLayer(&doseScale, this);
    threatLayer      = new ThreatLayer(ThreatLayer::Kind::Active, this);
    vehicleLayer     = new VehicleLayer(this);
    doseLegend       = new DoseLegendLayer(&doseScale, measurementLayer, doseGridLayer, this);
    drawing          = new DrawingLayer(this);

    doseGridLayer->setPointsLayer(measurementLayer);
    drawing->setPolygonVertexLimit(CoreConst::AreaMaxVertices);

    // Тепловая карта - под фигурами пользователя (контур области поверх неё),
    // точки измерений - над архивом, легенда - поверх всего, кроме рисования
    addLayer(searchLayer);
    addLayer(doseGridLayer);
    addLayer(geometryLayer);
    addLayer(archiveLayer);
    addLayer(measurementLayer);
    addLayer(threatLayer);
    addLayer(vehicleLayer);
    addLayer(doseLegend);
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
                emit geometryFinished(GeometryKind::Polygon, pts);
                emit coordinatesArrayReady(pts);
            });
    connect(drawing, &DrawingLayer::lineFinished, this,
            [this](const QVector<smappoint> &pts) {
                store->add(GeometryKind::Line, pts);
                emit geometryFinished(GeometryKind::Line, pts);
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
    connect(geometryLayer, &GeometryLayer::reconPrepareRequested, this, &MapWidget::reconPrepareRequested);

    // --- точки измерений ---
    connect(measurementLayer, &MeasurementLayer::measurementSelected, this, &MapWidget::measurementClicked);
    connect(measurementLayer, &MeasurementLayer::centerRequested, this, [this](double lat, double lon) {
        smarker m;
        m.pt.latt = lat;
        m.pt.longt = lon;
        m.type = 0;
        m.is_sel = false;
        centerOnPoint(m);
    });
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

    update();
}

void MapWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    for (MapLayer *layer : layers)
        layer->mouseLeave();
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

void MapWidget::setupButtons()
{
    ui = new Ui::MapWidget;
    ui->setupUi(this);
    defaultLeftMargin = ui->verticalLayout->contentsMargins().left();

    // Рисование - левая колонка
    btnLine    = ui->pushButtonCreateLine;
    btnPolygon = ui->pushButtonCreatePolygon;
    btnFinish  = ui->pushButtonFinishDrawing;
    btnUndo    = ui->pushButtonUndoPoint;
    btnCancel  = ui->pushButtonCancelDrawing;
    btnClear   = ui->pushButtonClearMap;

    // Управление картой - правая колонка
    btnFilter   = ui->pushButtonLayerMenu;
    btnTrack    = ui->pushButtonAutoTrack;
    btnZoomIn   = ui->pushButtonMapZoomIn;
    btnZoomOut  = ui->pushButtonMapZoomOut;

    // Кнопки не забирают фокус - горячие клавиши рисования остаются у карты
    for (QPushButton *b : findChildren<QPushButton *>())
        b->setFocusPolicy(Qt::NoFocus);

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

    connect(btnZoomIn,  &QPushButton::clicked, this, &MapWidget::zoomIn);
    connect(btnZoomOut, &QPushButton::clicked, this, &MapWidget::zoomOut);

    trackMarkerMode = btnTrack->isChecked();   // в ui автотрекинг включён по умолчанию
    connect(btnTrack, &QPushButton::toggled, this, [this](bool on) {
        trackMarkerMode = on;
        if (on && vehicleLayer->isValidPosition())
            setCenter(vehicleLayer->getVehiclePosition());
    });


    connect(ui->currentCoordLineEdit, &LineEditDisplayCoordinates::doubleClicked,
            this, &MapWidget::copyCenterCoordinates);
    setupCoordTypeBox();

    applyMapTheme();

    onDrawingModeChanged(DrawingLayer::Mode::None);
}

// ------------------- ТЕМА КНОПОК КАРТЫ ------------------- //

namespace {

// SVG с currentColor / чёрным контуром / белой заливкой -> иконка нужных цветов
QPixmap themedSvg(const QString &path, const QColor &fg, const QColor &bg, int px, qreal dpr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QPixmap();
    QByteArray svg = f.readAll();
    const QByteArray fgName = fg.name().toLatin1(), bgName = bg.name().toLatin1();
    svg.replace("currentColor", fgName);
    svg.replace("stroke=\"black\"", "stroke=\"" + fgName + "\"");
    svg.replace("fill=\"black\"", "fill=\"" + fgName + "\"");
    svg.replace("fill=\"white\"", "fill=\"" + bgName + "\"");

    QSvgRenderer r(svg);
    QPixmap pm(QSize(px, px) * dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

} // namespace

// Кнопки поверх карты должны быть непрозрачными (стиль Windows 11 рисует их
// полупрозрачными - на карте их не видно), но при этом следовать теме Windows.
// Поэтому цвета берём не из palette(button) (там может быть альфа), а выбираем
// светлый или тёмный набор по яркости фона окна. Иконки перекрашиваем под текст.
void MapWidget::applyMapTheme()
{
    // Палитра приложения, а не виджета: у MapWidget своя таблица стилей, и его
    // собственная палитра при смене темы Windows может не обновиться
    const QPalette pal = QApplication::palette();
    QColor win = pal.color(QPalette::Window);
    win.setAlpha(255);
    const bool dark = win.lightness() < 128;
    QColor accent = pal.color(QPalette::Highlight);
    accent.setAlpha(255);
    if (accent.saturation() < 40)
        accent = dark ? QColor(0x4C, 0xA3, 0xF0) : QColor(0x00, 0x78, 0xD7);

    const QString key = QStringLiteral("%1|%2").arg(dark).arg(accent.name());
    if (key == themeKey)
        return;
    themeKey = key;

    struct C { QColor bg, field, border, hover, pressed, checked, fg, muted; };
    const C c = dark
        ? C{ QColor(0x2B, 0x2D, 0x31), QColor(0x1F, 0x21, 0x25), QColor(0x4A, 0x4E, 0x55),
             QColor(0x36, 0x3A, 0x40), QColor(0x24, 0x38, 0x4E), QColor(0x1E, 0x3A, 0x5A),
             QColor(0xE8, 0xEA, 0xED), QColor(0x7D, 0x84, 0x8C) }
        : C{ QColor(0xFA, 0xFA, 0xFA), QColor(0xFF, 0xFF, 0xFF), QColor(0xAA, 0xAA, 0xAA),
             QColor(0xE5, 0xF1, 0xFB), QColor(0xCC, 0xE4, 0xF7), QColor(0xCC, 0xE4, 0xF7),
             QColor(0x1F, 0x23, 0x28), QColor(0x9A, 0xA0, 0xA6) };

    // Стрелка выпадающего списка - перекрашенный SVG во временном файле
    // (в таблице стилей картинку можно задать только путём)
    const QString arrowPath = QDir::temp().filePath(
        QStringLiteral("dgs_chevron_%1.png").arg(c.fg.name().mid(1)));
    if (!QFile::exists(arrowPath))
        themedSvg(QStringLiteral(":/icons/svg/bs5/chevron-down.svg"), c.fg, c.field, 20, 1.0).save(arrowPath);

    setStyleSheet(QStringLiteral(R"(
        MapWidget > QPushButton {
            background-color: %1; color: %7;
            border: 1px solid %3; border-radius: 6px;
            min-width: 35px; min-height: 35px;
        }
        MapWidget > QPushButton:hover    { background-color: %4; border-color: %9; }
        MapWidget > QPushButton:pressed  { background-color: %5; }
        MapWidget > QPushButton:checked  { background-color: %6; border-color: %9; }
        MapWidget > QPushButton:disabled { color: %8; }
        MapWidget > QPushButton::menu-indicator { image: none; width: 0px; }
        #coordFrame {
            background-color: %1;
            border: 1px solid %3; border-radius: 6px;
        }
        #coordFrame QLineEdit {
            background: transparent; border: none; color: %7;
            selection-background-color: %9; selection-color: white;
        }
        #coordFrame QComboBox {
            background-color: %2; color: %7;
            border: 1px solid %3; border-radius: 4px;
            padding: 2px 22px 2px 6px;
        }
        #coordFrame QComboBox:hover { border-color: %9; }
        #coordFrame QComboBox::drop-down { border: none; width: 20px; }
        #coordFrame QComboBox::down-arrow { image: url(%10); width: 10px; height: 10px; }
        #coordFrame QComboBox QAbstractItemView {
            background-color: %2; color: %7; border: 1px solid %3; outline: 0;
            selection-background-color: %9; selection-color: white;
        }
    )")
        .arg(c.bg.name(), c.field.name(), c.border.name(), c.hover.name(), c.pressed.name(),
             c.checked.name(), c.fg.name(), c.muted.name(), accent.name())
        .arg(QDir::fromNativeSeparators(arrowPath)));

    // Иконки: обычная - цветом текста, нажатая - акцентом, недоступная - приглушённо
    const struct { QPushButton *btn; const char *svg; } icons[] = {
        { btnFilter,  ":/icons/svg/bs5/layers-half.svg" },
        { btnLine,    ":/icons/svg/custom/line.svg" },
        { btnPolygon, ":/icons/svg/custom/ReconAreaMode.svg" },
        { btnFinish,  ":/icons/svg/bs5/check-lg.svg" },
        { btnUndo,    ":/icons/svg/bs5/arrow-counterclockwise.svg" },
        { btnCancel,  ":/icons/svg/bs5/x-lg.svg" },
        { btnClear,   ":/icons/svg/bs5/trash3.svg" },
        { btnTrack,   ":/icons/svg/custom/GPSTrack.svg" },
        { btnZoomIn,  ":/icons/svg/bs5/plus.svg" },
        { btnZoomOut, ":/icons/svg/bs5/dash.svg" },
    };
    const qreal dpr = devicePixelRatioF();
    for (const auto &i : icons) {
        if (!i.btn)
            continue;
        const int px = qMax(16, i.btn->iconSize().width());
        const QString path = QString::fromLatin1(i.svg);
        QIcon icon;
        icon.addPixmap(themedSvg(path, c.fg, c.bg, px, dpr), QIcon::Normal, QIcon::Off);
        icon.addPixmap(themedSvg(path, accent, c.checked, px, dpr), QIcon::Normal, QIcon::On);
        icon.addPixmap(themedSvg(path, accent, c.hover, px, dpr), QIcon::Active, QIcon::Off);
        icon.addPixmap(themedSvg(path, accent, c.checked, px, dpr), QIcon::Active, QIcon::On);
        icon.addPixmap(themedSvg(path, c.muted, c.bg, px, dpr), QIcon::Disabled, QIcon::Off);
        i.btn->setIcon(icon);
    }
}

bool MapWidget::event(QEvent *event)
{
    const bool res = QWidget::event(event);
    // Смена темы Windows: ThemeChange / ApplicationPaletteChange / PaletteChange.
    // StyleChange приходит и от нашего же setStyleSheet - повторно тема не
    // применяется (сверяется themeKey), так что зацикливания нет.
    switch (event->type()) {
    case QEvent::ThemeChange:
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        if (ui)
            QMetaObject::invokeMethod(this, &MapWidget::applyMapTheme, Qt::QueuedConnection);
        break;
    default:
        break;
    }
    return res;
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

    updateCursor();

    // Как и раньше: сигнал и при отмене, и при завершении фигуры.
    // Плюс при переключении линия <-> полигон посреди рисования: начатая фигура пропадает,
    // и тот, кто ждал её (MapPicker), должен об этом узнать.
    if (drawingWasActive && mode != lastDrawingMode)
        emit drawingCanceled();

    if (active)
        setFocus();   // чтобы работали горячие клавиши
    drawingWasActive = active;
    lastDrawingMode = mode;
}

// ------------------- ПАНЕЛЬ УПРАВЛЕНИЯ КАРТОЙ ------------------- //

// Фильтр отображаемых объектов: каждый пункт - отдельный флаг видимости.
// Новый объект/слой = одна строка addFilter().
void MapWidget::setupFilterMenu()
{
    filterMenu = new QMenu(this);
    btnFilter->setMenu(filterMenu);

    addFilter("Активні загрози",   true, [this](bool on) { threatLayer->setVisible(on); });
    addFilter("Архівні дані",      true, [this](bool on) { archiveLayer->setVisible(on); });
    addFilter("Маркер дрона",      true, [this](bool on) { vehicleLayer->setVehicleVisible(on); });
    addFilter("Шлях дрона",        true, [this](bool on) { vehicleLayer->setPathVisible(on); });
    addFilter("Область пошуку",    true, [this](bool on) { searchLayer->setVisible(on); });

    filterMenu->addSeparator();
    addFilter("Точки вимірювань",     true, [this](bool on) { measurementLayer->setVisible(on); });
    addFilter("Значення біля точок",  true, [this](bool on) { measurementLayer->setLabelsVisible(on); });
    addFilter("Теплова карта",        true, [this](bool on) { doseGridLayer->setVisible(on); });

#ifndef QT_NO_DEBUG
    // Только в отладочной сборке: проверка слоёв дозы без ядра
    filterMenu->addSeparator();
    QMenu *test = filterMenu->addMenu("Тест: дані дози");
    test->addAction("Політ одразу",           this, [this] { loadTestDoseData(false); });
    test->addAction("Політ наживо (таймер)",  this, [this] { loadTestDoseData(true); });
    test->addAction("Очистити",               this, [this] { clearDoseData(); });
#endif
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
    geometryLayer->setReconCheck(-1);      // новая / изменённая область — ждём проверки ядра
    if (const GeometryItem *it = store->find(id)) {
        if (it->kind == GeometryKind::Polygon && it->points.size() > CoreConst::AreaMaxVertices)
            NotificationManager::showWarning(
                QString("В області %1 точок, логер приймає до %2. ПКМ по області → «Спростити до %2 точок».")
                    .arg(it->points.size()).arg(CoreConst::AreaMaxVertices),
                "Забагато точок");
        emit reconAssigned(it->points, it->kind == GeometryKind::Line);
    }
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

// ------------------------ ДОЗА: ТОЧКИ И ТЕПЛОВАЯ КАРТА ------------------------ //

void MapWidget::setReconArea(const QVector<smappoint> &points)
{
    if (points.isEmpty())
        return;
    const int id = store->reconId();
    const GeometryItem *it = store->find(id);
    if (it && it->kind == GeometryKind::Polygon) {
        // те же вершины (с точностью ~1 см) — ничего не делать
        bool same = it->points.size() == points.size();
        for (int i = 0; same && i < points.size(); ++i)
            same = std::fabs(it->points[i].latt - points[i].latt) < 1e-7
                && std::fabs(it->points[i].longt - points[i].longt) < 1e-7;
        if (!same)
            store->setPoints(id, points);
        return;
    }
    // разведкой была линия (маршрут) или ничего — новый полигон
    store->assignRecon(store->add(GeometryKind::Polygon, points));
}

void MapWidget::setReconCheck(bool ok, int vertex, const QString &error)
{
    geometryLayer->setReconCheck(ok ? 1 : 0, vertex, error);
}

void MapWidget::setActiveReconArea(const CommandRequest &id04, const QDateTime &acceptedAt)
{
    QVector<smappoint> pts;
    pts.reserve(id04.vertices.size());
    for (const GeoPoint &g : id04.vertices) {
        smappoint p;
        p.latt = g.lat;
        p.longt = g.lon;
        pts << p;
    }

    // Параметры, с которыми логер ведёт разведку, — для плашки активной области
    QStringList params;
    switch (id04.reconMode) {
    case ReconMode::Operator: params << QStringLiteral("Режим: за командою оператора"); break;
    case ReconMode::Time:     params << QStringLiteral("Режим: за часом (кожні %1 с)").arg(id04.intervalS); break;
    case ReconMode::Distance: params << QStringLiteral("Режим: за відстанню (кожні %1 м)").arg(id04.distanceM); break;
    default: break;
    }
    params << QStringLiteral("Інтервал: %1 с · відстань: %2 м").arg(id04.intervalS).arg(id04.distanceM)
           << (id04.spectrumS > 0 ? QStringLiteral("Набір спектра: %1 с").arg(id04.spectrumS)
                                  : QStringLiteral("Набір спектра: до команди ID12"))
           << QStringLiteral("Дублювання пакетів: ×%1").arg(id04.dupFactor);
    geometryLayer->setActiveRecon(pts, acceptedAt, params);
}

void MapWidget::clearActiveReconArea()
{
    geometryLayer->setActiveRecon({}, QDateTime());
}

void MapWidget::onMeasurement(const Measurement &m)                 { measurementLayer->addMeasurement(m); }
void MapWidget::setMeasurements(const QVector<Measurement> &list)   { measurementLayer->setMeasurements(list); }
void MapWidget::onGridUpdated(const GridData &grid)
{
    doseGridLayer->setGrid(grid);

    // подписи значений не ставим поверх мишеней горячих точек
    QVector<QPointF> hot;
    if (grid.cols > 0 && grid.rows > 0) {
        const double dLat = (grid.latMax - grid.latMin) / grid.rows;
        const double dLon = (grid.lonMax - grid.lonMin) / grid.cols;
        for (int idx : grid.hotspots)
            hot << dosemap::mercNorm(grid.latMin + (idx / grid.cols + 0.5) * dLat,
                                     grid.lonMin + (idx % grid.cols + 0.5) * dLon);
    }
    measurementLayer->setAvoidPoints(hot);
}
void MapWidget::selectMeasurement(qint64 id)                        { measurementLayer->selectAndCenter(id); }

void MapWidget::clearDoseData()
{
    if (testFlight)
        testFlight->stop();
    measurementLayer->clear();
    measurementLayer->setAvoidPoints({});
    doseGridLayer->clear();
}

void MapWidget::setDoseThresholds(float warn, float alarm)
{
    doseScale.setThresholds(warn, alarm);   // слои увидят новую версию шкалы и перестроят кэши
    update();
}

void MapWidget::loadTestDoseData(bool live)
{
    if (!testFlight) {
        testFlight = new DoseTestFlight(this);
        connect(testFlight, &DoseTestFlight::measurement, this, &MapWidget::onMeasurement);
        connect(testFlight, &DoseTestFlight::gridUpdated, this, &MapWidget::onGridUpdated);
        connect(testFlight, &DoseTestFlight::finished, this, [] {
            NotificationManager::showInfo("Тестовий політ завершено");
        });
    }
    clearDoseData();

    const smappoint c = m_core.getview();
    testFlight->setGridParams(10.0, 50.0, 2.0, doseScale.warn());
    testFlight->generate(c.latt, c.longt);

    disableTracking();
    setZoom(qMax(zoom(), 15));

    if (live) {
        testFlight->startLive(6, 100);      // ~60 точек/с: весь полёт примерно за 40 с
    } else {
        setMeasurements(testFlight->measurements());
        onGridUpdated(testFlight->buildGrid());
    }
    NotificationManager::showInfo(QString("Тестовий політ: %1 точок").arg(testFlight->measurements().size()));
}
