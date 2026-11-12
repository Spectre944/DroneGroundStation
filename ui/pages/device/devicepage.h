#ifndef DEVICEPAGE_H
#define DEVICEPAGE_H

#include "data_types/data_types.h"
#include <QWidget>
#include <QDateTime>

#include <ui/modules/tables/tabledata.h>
#include <backend/tables/tablemodel.h>


namespace Ui {
class DevicePage;
}

class DevicePage : public QWidget
{
    Q_OBJECT

public:
    explicit DevicePage(QWidget *parent = nullptr);
    ~DevicePage();

signals:
    void tableThreadClicked(int);

public slots:


    void onBdbgDataGenerated(const bdbgData& data);
    void onBdbgDerThresholdUpdated(QVariant);


    void onReconStarted();
    void onReconEnded(startReconParams);
    void onReconToggled(bool);
    void onReconModeChanged(PROGRAMM_SETTINGS::ReconMode);
    void onLastSavePointRecived(CoordPairD);
    void onDatabaseDataAddedDzura(gpsData, airmarData, bdbgData, mksuData, lcdData);
    void onThreatClicked(int index);
    void onReconDurationExceeded();


private:
    Ui::DevicePage *ui;

    void testData();

    class SimpleTableModel *сheckpointModel;
    bool mapThreadRequest = false;
};


#endif // DEVICEPAGE_H
