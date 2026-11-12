#ifndef MENU_H
#define MENU_H

#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QPushButton>

enum ShowPosition {
    RIGHT,
    BOTTOM
};

class Menu : public QMenu
{
    Q_OBJECT

public:
    explicit Menu(QWidget *parent = nullptr);
    void showAtButton(const QWidget *button);
    void showAtButton(const QPushButton *button, ShowPosition position);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;

private:
    bool m_mousePressedInside;

signals:
    void menuClosed();

};
#endif // MENU_H
