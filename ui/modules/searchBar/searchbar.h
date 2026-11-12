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
    QAction *pointFlag, *flagFlag, *routeFlag, *objectFlag, *sampleFlag, *calcZoneFlag, *factZoneFlag;
    QAction *layerOverlaping, *displayOnMap, *displayInNewTable;
    QAction *sortByTimeNewFirst, *sortByTimeOldFirst, *sortByDistance, *sortByConcetration, *sortByRadiation;
};

#endif // SEARCHBAR_H
