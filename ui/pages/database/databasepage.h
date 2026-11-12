#ifndef DATABASEPAGE_H
#define DATABASEPAGE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QJsonObject>

#include <ui/components/dialog/dialog.h>
#include <ui/modules/tables/tabledata.h>
#include <backend/tables/tablemodel.h>
#include <map/mapcore.h>

namespace Ui {
class DatabasePage;
}

class DatabasePage : public QWidget
{
    Q_OBJECT

public:
    explicit DatabasePage(QWidget *parent = nullptr);
    ~DatabasePage();

public slots:
    void updateTableData(const QVector<QVector<QVariant>> &data);
    void updateMapData(const QVector<QVector<QVariant>> &data);
    void onCenterMarkerChanged(smappoint centerPosition);
    void onArchiveThreatClicked(int index);
    void onArchiveDBOpened(QString);

signals:
    void tableQueryRequested(const QString&);
    void mapQueryRequested(const QString&);
    void querySearchAreaFormed(const QString&);
    void clearArchiveMapDataRequested();
    void archiveTableThreatClicked(int);
    void appendArchiveThread(tsmarker);
    void archiveDataLoaded(const QVector<smarker> &markers);

private:
    Ui::DatabasePage *ui;
    void setupUI();
    void fillData();
    void connections();

    void testData();

    class StickyTableModel *stickyModel = nullptr;
    class SimpleTableModel *simplelModel = nullptr;
    class TableNavigator *tableNavigator;
    QTableView *simpleTableView;
    class StickyTableView *stickyTableView;
    bool mapThreadRequest = false;
};



#endif // DATABASEPAGE_H
