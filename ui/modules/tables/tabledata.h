#ifndef TABLEDATA_H
#define TABLEDATA_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLayout>
#include <QSpacerItem>
#include <QTableWidget>

#include <backend/tables/tablemodel.h>
#include <backend/utilities/utilities.h>

namespace Ui {
class TableData;
}

class TableData : public QWidget
{
    Q_OBJECT

public:
    explicit TableData(QWidget *parent = nullptr);
    ~TableData();

private:
    Ui::TableData *ui;

};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class TableControl : public QWidget
{
    Q_OBJECT

public:
    explicit TableControl(QWidget *parent = nullptr);
    ~TableControl();

    enum class WidgetType {
        Clear,
        Adjust,
        Navigator
    };

    QWidget* getWidget(WidgetType type) const;
    void setButtonEnabled(WidgetType type, bool state);

signals:
    void clearRequested();
    void adjustRequested();
    void navigationPanelRequested();

public slots:
    void setTableName(const QString& name);
    void setIcon(const QPixmap& icon);
    void setNavigatorBtnVisible(bool);

private:

    QVBoxLayout *vLayoutMain;
    QHBoxLayout *hLayoutControl;
    QHBoxLayout *hLayoutContent;

    QFrame *frame;

    QLabel          *iconLabel;
    QLabel          *tableName;
    QStackedWidget  *stackWidget;
    QPushButton     *clearBtn;
    QPushButton     *adjustBtn;
    QPushButton     *navigatorBtn;

};

#endif // TABLEDATA_H
