#ifndef BUTTON_H
#define BUTTON_H

#include <QWidget>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QStyleOptionButton>
#include <QPalette>
#include <QStringList>

#include <ui/components/menu/menu.h>
#include <ui/components/styleconstants.h>

class Button : public QWidget
{
    Q_OBJECT
public:
    explicit Button(QWidget *parent = nullptr);

signals:
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

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
    Menu    *subMenu;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class MaterialButton : public QPushButton {
    Q_OBJECT

public:
    MaterialButton(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

enum DropdownButtonType {
    SINGLE = 0,
    MULTIBLE = 1
};

class DropdownButton : public QPushButton
{
    Q_OBJECT
public:
    explicit DropdownButton(QWidget *parent = nullptr);
    void setPixmap(const QPixmap& pixmap);
    QSize sizeHint() const override;
    void addActions(const QList<QAction*> actions);
    void buttonClicked();
    void setInitialText(const QString &text);
    void setButtonType(const DropdownButtonType type);
    QStringList getSelectedActions() const;
    QAction* activeActon() const;
    QMenu *getMenuPtr() const;
    QList<QAction*> checkedActions() const;
public slots:
    void actionSelected(QAction *action);

protected:
    void paintEvent(QPaintEvent *event) override;
    void updateButtonText();

private:
    Menu    *dropMenu;
    QPixmap m_pixmap;
    QString buttonText;
    DropdownButtonType buttonType = DropdownButtonType::MULTIBLE;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//


#endif // BUTTON_H
