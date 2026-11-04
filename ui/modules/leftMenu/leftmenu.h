#ifndef LEFTMENU_H
#define LEFTMENU_H

#include <QWidget>
#include <QPushButton>
#include <QFrame>
#include <QLayout>
#include <QSizePolicy>
#include <QButtonGroup>
#include <QPropertyAnimation>
#include <QShortcut>
#include <QKeySequence>

#include <backend/utilities/utilities.h>
#include <ui/components/styleconstants.h>

class LeftMenuButton : public QPushButton
{
    Q_OBJECT
public:
    explicit LeftMenuButton(QWidget *parent = nullptr);

    void updateIndicator(bool checked);
    void addSubPageMenu(const QList<QAction*> );

signals:
private:
    QFrame  *checkedFrame;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class LeftMenu : public QWidget
{
    Q_OBJECT
public:
    explicit LeftMenu(QWidget *parent = nullptr);
    ~LeftMenu();

public slots:
    void nextPage();
    void pageChanged(int);

signals:
    void changePage(int index);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void setupUI();
    void hideUnusedUI();
    void fillData();
    void connections();
    void setupShortcuts();

    void expandMenu();
    void collapseMenu();

    LeftMenuButton *deviceBtn;
    LeftMenuButton *databaseBtn;
    LeftMenuButton *cbrnBtn;
    LeftMenuButton *reconBtn;
    LeftMenuButton *settingsBtn;

    QVBoxLayout *layout;
    QButtonGroup *group;
    QSpacerItem *verticalSpacer;

    bool isExpandble = false;
    QPropertyAnimation *animation;
    int expandedWidth = 150;
    int collapsedWidth = 50;

    int currentPage = 0;

};

#endif // LEFTMENU_H
