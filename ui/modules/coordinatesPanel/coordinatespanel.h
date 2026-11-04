#ifndef COORDINATESPANEL_H
#define COORDINATESPANEL_H

#include <QWidget>

namespace Ui {
class CoordinatesPanel;
}

class CoordinatesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CoordinatesPanel(QWidget *parent = nullptr);
    ~CoordinatesPanel();

private:
    Ui::CoordinatesPanel *ui;
};

#endif // COORDINATESPANEL_H
