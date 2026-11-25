#ifndef LEFTMENU_H
#define LEFTMENU_H

/**
    @file  leftmenu.h
    @brief Левое меню страниц: узкая колонка кнопок-значков у левого края окна, без подписей.
    @note  Кнопки — LeftMenuButton (50×50, значок 20 px, полоска-индикатор у открытой страницы),
           название страницы — в подсказке.
           Сами действия (QAction, checkable) создаёт и обрабатывает MainWindow: меню только
           показывает их и держит кнопки в том же состоянии, что действия. Поэтому
           переключение из меню «Вигляд» и горячими клавишами тоже отражается на кнопках,
           а повторное нажатие на открытую страницу сворачивает её (MainWindow::togglePage).
           Рабочие страницы — сверху (addPage), «Налаштування» — прижата вниз (addBottomPage).
*/

#include <QHash>
#include <QWidget>

class QAction;
class QVBoxLayout;
class LeftMenuButton;

class LeftMenu : public QWidget
{
    Q_OBJECT
public:
    explicit LeftMenu(QWidget *parent = nullptr);

    void addPage(QAction *action, const QString &svgPath);         ///< сверху, по порядку
    void addBottomPage(QAction *action, const QString &svgPath);   ///< снизу, под растяжкой

protected:
    void changeEvent(QEvent *event) override;

private:
    LeftMenuButton *makeButton(QAction *action, const QString &svgPath);
    void updateIcons();                     ///< значки под текущую тему

    QVBoxLayout *m_layout = nullptr;
    int m_topCount = 0;                     ///< кнопок над растяжкой
    QHash<LeftMenuButton *, QString> m_icons;
};

#endif // LEFTMENU_H
