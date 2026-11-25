#ifndef DATABASEPAGE_H
#define DATABASEPAGE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QJsonObject>

#include <ui/components/dialog/dialog.h>
#include <ui/modules/tables/tabledata.h>
#include <backend/tables/tablemodel.h>
#include <map/mapcore.h>
#include "backend/classes/core_types.h"

namespace Ui {
class DatabasePage;
}

/**
    @brief Страница «База даних».
    @note  Поиск — через ядро (без SQL на стороне интерфейса):
             «Тип даних» → Точки   → DroneCore::queryArchive  → onArchiveResult;
             «Тип даних» → Польоти → DroneCore::listSessions  → onSessionsListed.
           Результаты — в одной таблице StickyTableModel разделами «Польоти» и «Точки»
           (заголовки столбцов меняются по разделу, кнопка навигатора — переход к разделу).
           Полёт: двойной клик или ПКМ → «Переглянути політ» → openFlightRequested(id)
           (просмотр на карте — Р2-2). Точки — на карту архивным слоем.
*/
class DatabasePage : public QWidget
{
    Q_OBJECT

public:
    explicit DatabasePage(QWidget *parent = nullptr);
    ~DatabasePage();

public slots:
    void onCenterMarkerChanged(smappoint centerPosition);
    void onArchiveThreatClicked(int index);          ///< клик по точке на карте — выделить строку
    void onArchiveDBOpened(QString);
    void showFlights();                              ///< «Бази даних → Відкрити політ…»: фильтр «Польоти» и поиск

    // --- от DroneCore ---
    void onSessionsListed(const QVector<SessionInfo> &list);
    void onArchiveResult(const ArchiveResult &result);
    void setCurrentSession(qint64 id);               ///< текущий полёт — помечается в списке

signals:
    void querySearchAreaFormed(const QString&);
    void clearArchiveMapDataRequested();
    void archiveTableThreatClicked(int);
    void archivePointsLoaded(const QVector<Measurement> &points);   ///< найденные точки → на карту

    void flightsRequested();                          ///< → DroneCore::listSessions
    void pointsRequested(const SessionFilter &filter);///< → DroneCore::queryArchive
    void openFlightRequested(qint64 sessionId);       ///< просмотр полёта на карте

private:
    Ui::DatabasePage *ui;
    void setupUI();
    void connections();

    void onSearch(bool points, bool flights, qint64 fromSec, qint64 toSec, const QString &area, int sort);
    void rebuildTable();
    void showContextMenu(const QPoint &pos);

    /// Строка таблицы → (раздел, номер в разделе); раздел: 0 — нет, 1 — полёты, 2 — точки
    QPair<int, int> locate(int row) const;
    static QVector<GeoPoint> parseArea(const QString &area);

    StickyTableModel *model = nullptr;
    StickyTableView  *view = nullptr;

    // Результаты последнего поиска
    QVector<SessionInfo>  m_flights;
    QVector<ArchivePoint> m_points;
    bool    m_showFlights = false;
    bool    m_showPoints = false;
    bool    m_waitFlights = false;        ///< ждём sessionsListed на свой запрос
    qint64  m_fromSec = 0, m_toSec = 0;
    int     m_sort = 0;
    qint64  m_currentSession = -1;
    int     m_flightSection = -1;         ///< индексы разделов в модели
    int     m_pointSection = -1;
    bool    mapThreadRequest = false;
};

#endif // DATABASEPAGE_H
