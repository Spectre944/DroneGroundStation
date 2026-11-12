#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include "QFileDialog"
#include <QWidget>
#include <QLine>
#include <QtMinMax>
#include <QJsonArray>


#include <ui/modules/settingsfield/settingsfield.h>
#include <backend/settings/settings.h>
#include <backend/data_types/data_types.h>
#include <ui/modules/notifier/notifier.h>

namespace Ui {
class SettingsPage;
}

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage();

    void saveSettings();
    void updateSettings();

public slots:
    void onAirmarTestDataGenerated(const airmarTest& testData);
    void onGpsTestDataGenerated(const gpsTest& testData);
    void onLcdTestDataGenerated(const lcdTest& testData);
    void onBdbgTestDataGenerated(const bdbgTest& testData);
    void onSvngTestDataGenerated(const svngTest& testData);
    void onSrhbTestDataGenerated(const srhrTest& testData);
    void onMksuTestDataGenerated(const mksuTest& testData);
    void onDeviceRemovedFromPoll(TypeDev);
    void onUpdateDiagnosticInfoDzura(const lcdTest&, const mksuTest&, const bdbgTest&, const airmarTest&, const gpsTest&);
    void onUpdateDiagnosticInfoBrdm(const lcdTest&, const svngTest&, const srhrTest&, const airmarTest&);
    void onCommanderKeyInserted(bool, QString);

signals:
    void openDiagnosticWindow();
    void startDiagnosicRequsted();
    void settingsSaved();
    void styleChangeRequested(int);

    void settingsWorkModeChanged(PROGRAMM_SETTINGS::ReconMode mode);
    void settingsAudioModeChanged(PROGRAMM_SETTINGS::SoundMode mode);
    void settingsSaveDistanceChanged(const QVariant&);
    void settingsSavePeriodChanged(const QVariant&);
    void settingsSVDERThresholdChanged(const QVariant&);
    void settingsSRDERThresholdChanged(const QVariant&);
    void settingsBDBGDERThresholdChanged(const QVariant&);
    void settingsMKSUDERThresholdChanged(const QVariant&);
    void settingsMKSUDoseThresholdChanged(const QVariant&);
    void settingsHNOThresholdChanged(const QVariant&);
    void settingsAsThresholdChanged(const QVariant&);
    void settingsSThresholdChanged(const QVariant&);
    void settingsPThresholdChanged(const QVariant&);
    void settingsCHThresholdChanged(const QVariant&);

    void removeDeviceFromSystem(TypeDev);
    void reconnectDevice(TypeDev);

    void cryptographyAccessChanged(bool);
    void mapFileSelected(QString);

private:
    Ui::SettingsPage *ui;

    void setupUI();
    void fillData();
    void connections();
    void lockAdminFunctions();
    void updateAdminFunctions();
    void forceUpdateComboBoxSettings();

    void setupInterfaceFields();
    void setupGeneralFields();
    void setupDevices();
    void setupSerialNumberFromFile();
    void buildLayout();
    void hideUnusedUI();
    void hideHeadquartersUI();
    QFrame* createHorizontalLine();


    bool adminAccess = false;

    QFileDialog mapSelectDialog;
    QString mapFilePath;

    static const int MSV_TO_MKSV = 1000;

    struct DeviceThresholds {
        QList<QWidget*> thresholds;
        bool isVisible = false;
    };

};

#endif // SETTINGSPAGE_H
