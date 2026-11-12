#include "menu.h"
#include <QMouseEvent>

Menu::Menu(QWidget *parent) : QMenu(parent), m_mousePressedInside(false)
{
}

void Menu::mouseReleaseEvent(QMouseEvent *event)
{
    QAction *action = actionAt(event->pos());
    if (action) {
        action->trigger();
    }
    // Не вызываем QMenu::mouseReleaseEvent, чтобы предотвратить закрытие меню
}

void Menu::mousePressEvent(QMouseEvent *event)
{
    m_mousePressedInside = true;
    QMenu::mousePressEvent(event);
}

void Menu::focusOutEvent(QFocusEvent *event)
{
    emit menuClosed();
    QMenu::focusOutEvent(event);
}

bool Menu::event(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress && !rect().contains(static_cast<QMouseEvent*>(event)->pos())) {
        // Клик был вне границ меню
        close();
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        if (!m_mousePressedInside) {
            // Если кнопка мыши была нажата вне меню, а отпущена внутри, игнорируем это событие
            return true;
        }
        m_mousePressedInside = false;
    }

    return QMenu::event(event);
}

void Menu::showAtButton(const QWidget *button)
{
    if (button) {
        QPoint pos = button->mapToGlobal(button->rect().bottomLeft());
        this->popup(pos);
    }
}

void Menu::showAtButton(const QPushButton *button, ShowPosition position)
{
    if (button) {

        QPoint pos;

        switch (position) {
            case ShowPosition::RIGHT:       pos = button->mapToGlobal(button->rect().topRight());     break;
            case ShowPosition::BOTTOM:      pos = button->mapToGlobal(button->rect().bottomLeft());   break;
            default:                        pos = button->mapToGlobal(button->rect().bottomLeft());   break;
        }

        this->popup(pos);
    }
}
