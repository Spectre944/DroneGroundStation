#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QWidget>
#include <QStandardItemModel>
#include <QJsonObject>
#include <QActionGroup>

#include <ui/components/dialog/dialog.h>
#include <ui/components/menu/menu.h>
#include <ui/components/button/button.h>
#include <map/mapcore.h>


namespace Ui {
class SearchBar;
}

class SearchBar : public QWidget
{
    Q_OBJECT

public:
    explicit SearchBar(QWidget *parent = nullptr);
    ~SearchBar();

    enum class WidgetType {
        Type,
        Flag,
        Date,
        Square,
        Options,
        Search
    };

    QWidget* getWidget(WidgetType type) const;

signals:
    void tableQueryRequested(const QString&);
    void mapQueryRequested(const QString&);
    void querySearchAreaFormed(const QString&);

    /**
        @brief Поиск через ядро (страница «База даних»).
        @param points   искать точки (DroneCore::queryArchive)
        @param flights  показать полёты (DroneCore::listSessions)
        @param fromSec, toSec  период, секунды Unix; 0 — без ограничения
        @param area     область «WGS84:ш,д/…» (круг радиуса или заданная); пусто — вся карта
        @param sort     0 — нові спочатку, 1 — старі спочатку, 2 — за ПАЕД
    */
    void archiveSearchRequested(bool points, bool flights, qint64 fromSec, qint64 toSec,
                                const QString &area, int sort);

public slots:
    // void onTypeActionTriggered(QAction* action);
    // void onTimeActionTriggered(QAction* action);
    // void onAreaActionTriggered(QAction* action);
    void createQuery();
    void onCenterMarkerChanged(smappoint centerPosition);


private:
    void setupUI();
    void fillData();
    void connections();

    void getSelectedFilter();
    void getDangerTypeFilter();
    void getTimeFilter(QAction*);
    void getDistanceFilter(QAction*);
    QString getSortFilter();

    Ui::SearchBar *ui;
    DateDateDialog *ddDialog;
    AreaSearchDialog *asDialog;

    int dangerType = -1;           // -1 = все типы
    qint64 timeFrom = 0;           // 0 = без ограничения
    qint64 timeTo = 0;             // 0 = без ограничения
    bool searchInArea = false;
    QString areaCoordinates;
    double centerLat = 50.4234;     // центр поиска (по умолчанию Киев)
    double centerLon = 30.5842;
    double radiusMeters = -1;       // -1 = без ограничения

    QAction *radType, *bioType, *chemType;
    QAction *allTime, *lastHour, *lastDay, *lastWeek, *lastMonth, *lastYear, *lastOptional;
    QAction *areaAll, *area1km, *area10km, *area50km, *area100km, *area500km, *areaOptional;
    QAction *pointFlag = nullptr;
    QAction *flightFlag = nullptr;      ///< «Польоти» — список полётов (Р2-2)
    QAction *layerOverlaping, *displayOnMap, *displayInNewTable;
    QAction *sortByTimeNewFirst, *sortByTimeOldFirst, *sortByDistance, *sortByRadiation;
};

#endif // SEARCHBAR_H
