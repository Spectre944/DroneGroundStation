#include "databasepage.h"
#include "ui_databasepage.h"

#include <QMenu>
#include <algorithm>

#include "backend/coords/coordhandler.h"

namespace {

QString pointTypeText(quint32 t)
{
    if (t & ApAlarm)    return QStringLiteral("тривога");
    if (t & ApWarn)     return QStringLiteral("попередження");
    if (t & ApSpectrum) return QStringLiteral("спектр");
    return QStringLiteral("вимірювання");
}

QString durationText(const QDateTime &a, const QDateTime &b)
{
    if (!a.isValid() || !b.isValid())
        return QStringLiteral("триває");
    const qint64 s = a.secsTo(b);
    return QStringLiteral("%1:%2").arg(s / 3600).arg((s / 60) % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

DatabasePage::DatabasePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DatabasePage)
{
    ui->setupUi(this);
    setupUI();
    connections();
}

DatabasePage::~DatabasePage()
{
    delete ui;
}

void DatabasePage::setupUI()
{
    ui->tableControl->setTableName("База даних");
    ui->tableControl->setIcon(QPixmap(":/icons/png/old/databaseColor.png"));
    ui->tableControl->setNavigatorBtnVisible(true);    // переход к разделу «Польоти» / «Точки»

    // Одна таблица на все типы данных: разделы со своими заголовками
    model = new StickyTableModel(this);
    view = new StickyTableView(this);
    view->setModel(model);
    view->setAlternatingRowColors(true);
    view->setFont(QFont("Consolas"));                  // точки-заполнители «ПАЕД: ....» выравниваются
    view->setTabKeyNavigation(false);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view->setToolTip(QStringLiteral("Політ: подвійний клік або ПКМ → «Переглянути політ»"));
    ui->tableLayout->addWidget(view, 1);

    // Панель поиска - по высоте содержимого, всё остальное место - таблице
    ui->widgetSerach->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->verticalLayout->setStretchFactor(ui->tableLayout, 1);
    ui->tableControl->setFocus();
}

void DatabasePage::connections()
{
    connect(ui->widgetSerach, &SearchBar::querySearchAreaFormed, this, &DatabasePage::querySearchAreaFormed);
    connect(ui->widgetSerach, &SearchBar::archiveSearchRequested, this, &DatabasePage::onSearch);

    connect(ui->tableControl, &TableControl::clearRequested, this, [this] {
        m_flights.clear();
        m_points.clear();
        m_showFlights = m_showPoints = false;
        rebuildTable();
        emit clearArchiveMapDataRequested();
    });
    connect(ui->tableControl, &TableControl::adjustRequested, this, [this] {
        view->resizeColumnsToContents();
        view->resizeRowsToContents();
    });
    // Переход к разделу — выпадающим меню под кнопкой (а не отдельным окном)
    connect(ui->tableControl, &TableControl::navigationPanelRequested, this, [this] {
        QMenu menu(this);
        const QVector<TableSection> &secs = model->getSections();
        for (const TableSection &sec : secs) {
            const int row = sec.startRow;
            menu.addAction(QStringLiteral("%1 (%2)").arg(sec.name).arg(sec.rowCount), this, [this, row] {
                view->scrollTo(model->index(row, 0), QAbstractItemView::PositionAtTop);
                view->selectRow(row);
            });
        }
        if (menu.isEmpty())
            menu.addAction(QStringLiteral("Немає результатів"))->setEnabled(false);
        QWidget *btn = ui->tableControl->getWidget(TableControl::WidgetType::Navigator);
        menu.exec(btn ? btn->mapToGlobal(QPoint(0, btn->height())) : QCursor::pos());
    });

    connect(view, &QWidget::customContextMenuRequested, this, &DatabasePage::showContextMenu);
    connect(view, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex &idx) {
        const QPair<int, int> at = locate(idx.row());
        if (at.first == 1)
            emit openFlightRequested(m_flights[at.second].id);
    });
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
        const QPair<int, int> at = locate(current.row());
        if (at.first == 2 && !mapThreadRequest)
            emit archiveTableThreatClicked(at.second);       // точка → выделить на карте
        mapThreadRequest = false;
    });
}

// ------------------------------------------------------------------
//  Поиск
// ------------------------------------------------------------------

/**
    @brief «Бази даних → Відкрити політ…»: в фильтре «Тип даних» — только «Польоти», сразу поиск.
    @note  Остальные условия (время, область, сортировка) — те, что выбраны в строке поиска.
*/
void DatabasePage::setSearchArea(const QString &area)
{
    ui->widgetSerach->setSearchArea(area);
}

void DatabasePage::showFlights()
{
    ui->widgetSerach->searchFlights();
}

void DatabasePage::onSearch(bool points, bool flights, qint64 fromSec, qint64 toSec,
                            const QString &area, int sort)
{
    m_fromSec = fromSec;
    m_toSec = toSec;
    m_sort = sort;
    m_showFlights = flights;
    m_showPoints = points;
    m_flights.clear();
    m_points.clear();
    rebuildTable();

    if (flights) {
        m_waitFlights = true;
        emit flightsRequested();
    }
    if (points) {
        SessionFilter f;
        if (fromSec > 0)
            f.from = QDateTime::fromSecsSinceEpoch(fromSec);
        if (toSec > 0)
            f.to = QDateTime::fromSecsSinceEpoch(toSec);
        f.area = parseArea(area);
        emit pointsRequested(f);
    } else {
        emit archivePointsLoaded({});        // точки не искали — убрать старые с карты
    }
}

void DatabasePage::onSessionsListed(const QVector<SessionInfo> &list)
{
    if (!m_waitFlights)
        return;                              // список запросил кто-то другой (импорт SD)
    m_waitFlights = false;

    // Период фильтра — по началу полёта
    m_flights.clear();
    for (const SessionInfo &s : list) {
        const qint64 t = s.startTime.toSecsSinceEpoch();
        if (m_fromSec > 0 && t < m_fromSec)
            continue;
        if (m_toSec > 0 && t > m_toSec)
            continue;
        m_flights << s;
    }
    if (m_sort == 1)
        std::reverse(m_flights.begin(), m_flights.end());    // ядро даёт новые сверху
    rebuildTable();
}

void DatabasePage::onArchiveResult(const ArchiveResult &result)
{
    if (!m_showPoints)
        return;
    // Область поиска — по точному многоугольнику (чётно-нечётное правило), а не по рамке:
    // ядро может отдать лишнее (заглушка ищет по рамке, ядро — только по выпуклой области)
    m_points.clear();
    m_points.reserve(result.points.size());
    for (const ArchivePoint &p : result.points)
        if (insideArea(result.filter.area, p.lat, p.lon))
            m_points << p;
    std::stable_sort(m_points.begin(), m_points.end(), [this](const ArchivePoint &a, const ArchivePoint &b) {
        if (m_sort == 2) return a.paed > b.paed;
        if (m_sort == 1) return a.time < b.time;
        return a.time > b.time;
    });

    // На карту — как точки разведки: цвет по шкале ПАЕД и значения рядом
    QVector<Measurement> pts;
    pts.reserve(m_points.size());
    for (const ArchivePoint &p : std::as_const(m_points)) {
        Measurement m;
        m.id = p.id;
        m.time = p.time;
        m.lat = p.lat;
        m.lon = p.lon;
        m.paed = p.paed;              // в базе — уже то значение, что показывалось (DoseCorrection)
        pts << m;
    }
    emit archivePointsLoaded(pts);
    rebuildTable();
}

void DatabasePage::setCurrentSession(qint64 id)
{
    m_currentSession = id;
    if (!m_flights.isEmpty())
        rebuildTable();
}

void DatabasePage::rebuildTable()
{
    model->clearData();
    m_flightSection = m_pointSection = -1;

    // Фон разделов — лёгкий оттенок от палитры (читается и в тёмной теме)
    const QColor base = palette().color(QPalette::Base);
    const QColor accent = palette().color(QPalette::Highlight);
    auto tint = [&](double k) {
        return QColor::fromRgbF(base.redF() * (1 - k) + accent.redF() * k,
                                base.greenF() * (1 - k) + accent.greenF() * k,
                                base.blueF() * (1 - k) + accent.blueF() * k);
    };

    // Формат как в РХБ: многострочные ячейки, одинаковые по смыслу строки — в одном столбце:
    //   столбец 1 «Параметри» — где и когда, столбец 2 — данные
    if (m_showFlights) {
        QVector<QVector<QVariant>> rows;
        for (const SessionInfo &s : std::as_const(m_flights)) {
            const QDateTime st = s.startTime.toLocalTime();
            const QString params = QStringLiteral("Назва\n%1\n\nЧас:  %2\nДата: %3")
                                       .arg(s.name, st.toString("HH:mm:ss"), st.toString("dd.MM.yyyy"));
            QString data = QStringLiteral("Тривалість: .. %1\nВимірювань: .. %2\nSD-карта: .... %3\nСтан: ........ %4")
                               .arg(durationText(s.startTime, s.endTime))
                               .arg(s.measurementCount)
                               .arg(s.noSd ? QStringLiteral("без SD") : QStringLiteral("так"),
                                    s.id == m_currentSession ? QStringLiteral("поточний")
                                    : s.endTime.isValid()    ? QStringLiteral("завершено")
                                                             : QStringLiteral("не завершено"));
            rows.append({s.id, params, data});
        }
        if (rows.isEmpty())
            rows.append({QString(), m_waitFlights ? QStringLiteral("Завантаження…") : QStringLiteral("Польотів не знайдено")});
        m_flightSection = model->getSections().size();
        model->addSection("Польоти", {"ID", "Параметри", "Дані польоту"}, rows, tint(0.06));
    }

    if (m_showPoints) {
        QVector<QVector<QVariant>> rows;
        rows.reserve(m_points.size());
        for (const ArchivePoint &p : std::as_const(m_points)) {
            const QDateTime t = p.time.toLocalTime();
            const QString params = QStringLiteral("Координати\nш: %1\nд: %2\n\nЧас:  %3\nДата: %4")
                                       .arg(p.lat, 0, 'f', 4).arg(p.lon, 0, 'f', 4)
                                       .arg(t.toString("HH:mm:ss"), t.toString("dd.MM.yyyy"));
            const QString data = QStringLiteral("ПАЕД: .... %1 мкЗв/г\nТип: ..... %2\nПоліт: ... №%3")
                                     .arg(QString::number(double(p.paed), 'f', p.paed < 10 ? 2 : 1),
                                          pointTypeText(p.type))
                                     .arg(p.sessionId);
            rows.append({p.id, params, data});
        }
        if (rows.isEmpty())
            rows.append({QString(), QStringLiteral("Точок не знайдено")});
        m_pointSection = model->getSections().size();
        model->addSection("Точки", {"ID", "Параметри", "Радіація"}, rows, tint(0.12));
    }

    view->resizeColumnsToContents();
    view->resizeRowsToContents();
}

QPair<int, int> DatabasePage::locate(int row) const
{
    const QVector<TableSection> &secs = model->getSections();
    auto check = [&](int sec, int kind, int count) -> QPair<int, int> {
        if (sec < 0 || sec >= secs.size())
            return {0, -1};
        const int i = row - secs[sec].startRow;
        if (i >= 0 && i < count)
            return {kind, i};
        return {0, -1};
    };
    QPair<int, int> r = check(m_flightSection, 1, int(m_flights.size()));
    if (r.first)
        return r;
    return check(m_pointSection, 2, int(m_points.size()));
}

void DatabasePage::showContextMenu(const QPoint &pos)
{
    const QModelIndex idx = view->indexAt(pos);
    if (!idx.isValid())
        return;
    const QPair<int, int> at = locate(idx.row());
    QMenu menu(this);
    if (at.first == 1) {
        const SessionInfo s = m_flights[at.second];
        menu.addAction(QStringLiteral("Переглянути політ «%1»").arg(s.name), this,
                       [this, s] { emit openFlightRequested(s.id); });
    } else if (at.first == 2) {
        const int i = at.second;
        menu.addAction(QStringLiteral("Показати на карті"), this, [this, i] { emit archiveTableThreatClicked(i); });
        const qint64 sid = m_points[i].sessionId;
        menu.addAction(QStringLiteral("Переглянути політ №%1").arg(sid), this, [this, sid] { emit openFlightRequested(sid); });
    }
    if (!menu.isEmpty())
        menu.exec(view->viewport()->mapToGlobal(pos));
}

/**
    @brief Точка внутри многоугольника — чётно-нечётное правило (луч вдоль долготы).
    @note  Подходит и для невыпуклых областей. Меньше 3 вершин — ограничения нет.
*/
bool DatabasePage::insideArea(const QVector<GeoPoint> &area, double lat, double lon)
{
    const int n = area.size();
    if (n < 3)
        return true;
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const GeoPoint &a = area[i];
        const GeoPoint &b = area[j];
        if ((a.lat > lat) != (b.lat > lat)) {
            const double x = a.lon + (lat - a.lat) * (b.lon - a.lon) / (b.lat - a.lat);
            if (lon < x)
                inside = !inside;
        }
    }
    return inside;
}

QVector<GeoPoint> DatabasePage::parseArea(const QString &area)
{
    // «WGS84:ш,д/WGS84:ш,д/…» — от кругового поиска и окна «В заданій області»
    QVector<GeoPoint> out;
    for (const QString &part : area.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        QString p = part.trimmed();
        const int colon = p.indexOf(QLatin1Char(':'));
        if (colon >= 0)
            p = p.mid(colon + 1);
        const QStringList ll = p.split(QLatin1Char(','));
        if (ll.size() != 2)
            continue;
        bool ok1 = false, ok2 = false;
        GeoPoint g;
        g.lat = ll[0].trimmed().toDouble(&ok1);
        g.lon = ll[1].trimmed().toDouble(&ok2);
        if (ok1 && ok2)
            out << g;
    }
    // Замкнутый круг повторяет первую вершину в конце — ядру повтор не нужен
    if (out.size() > 3 && std::fabs(out.first().lat - out.last().lat) < 1e-9
                      && std::fabs(out.first().lon - out.last().lon) < 1e-9)
        out.removeLast();
    return out;
}

// ------------------------------------------------------------------

void DatabasePage::onCenterMarkerChanged(smappoint centerPosition)
{
    ui->widgetSerach->onCenterMarkerChanged(centerPosition);
}

void DatabasePage::onArchiveThreatClicked(int index)
{
    if (m_pointSection < 0 || index < 0 || index >= m_points.size())
        return;
    mapThreadRequest = true;
    view->selectRow(model->getSections()[m_pointSection].startRow + index);
}

void DatabasePage::onArchiveDBOpened(QString name)
{
    if (!name.isEmpty())
        ui->tableControl->setTableName(QString("База даних %1").arg(name));
}
