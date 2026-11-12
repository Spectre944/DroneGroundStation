#ifndef RECONPAGE_H
#define RECONPAGE_H

#include <QWidget>
#include <QJsonObject>
#include <QAction>
#include <QComboBox>
#include <QStandardItemModel>

#include <backend/data_types/data_types.h>


namespace Ui {
class ReconPage;
}

class ReconPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReconPage(QWidget *parent = nullptr);
    ~ReconPage();

signals:
    void reconDataGenerated(startReconParams);
    void reconAreaSelected(const QString&);

private:
    Ui::ReconPage *ui;

    void collectReconDataFromPage();
    PROGRAMM_SETTINGS::ReconMode getReconModeFromComboBox(QComboBox*);
    reqDevMode getDeviceModeFromComboBox(QComboBox*);
    attackStatus getAtackFromComboBox(QComboBox*);

    void fillData();
    void setupUI();
    void connections();
    void hideUnusedUI();
};

#endif // RECONPAGE_H
