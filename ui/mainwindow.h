#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedLayout>

#include "map/mapwidget.h"
#include <backend/settings/settings.h>
#include <ui/modules/leftMenu/leftmenu.h>
#include <ui/modules/coordinatesPanel/coordinatespanel.h>

#include "ui/windows/overlay/overlaycontainer.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    MapWidget *mapWidget = nullptr;

    LeftMenu *leftMenu = nullptr;
    OverlayContainer * leftOverlay = nullptr;

    OverlayContainer * coordOverlay = nullptr;
    CoordinatesPanel *coordinatesPanel = nullptr;

    // QWidget interface
protected:
    void resizeEvent(QResizeEvent *event);
};
#endif // MAINWINDOW_H
