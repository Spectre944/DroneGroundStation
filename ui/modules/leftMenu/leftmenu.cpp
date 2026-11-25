#include "leftmenu.h"

#include <QAction>
#include <QEvent>
#include <QVBoxLayout>

#include <backend/utilities/utilities.h>
#include <ui/components/button/button.h>

LeftMenu::LeftMenu(QWidget *parent)
    : QWidget{parent}
{
    setObjectName("LeftMenuWidget");

    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addStretch();                 // всё, что после растяжки, прижато к низу

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
}

void LeftMenu::addPage(QAction *action, const QString &svgPath)
{
    m_layout->insertWidget(m_topCount++, makeButton(action, svgPath));
}

void LeftMenu::addBottomPage(QAction *action, const QString &svgPath)
{
    m_layout->addWidget(makeButton(action, svgPath));
}

/**
    @brief Кнопка для действия страницы.
    @note  Нажатие кнопки = срабатывание действия; положение кнопки всегда берётся
           из действия (его меняет MainWindow::togglePage / showPage / hidePages).
*/
LeftMenuButton *LeftMenu::makeButton(QAction *action, const QString &svgPath)
{
    auto *btn = new LeftMenuButton(this);
    btn->setIcon(IconHelper::createThemedIcon(svgPath));
    btn->setChecked(action->isChecked());

    const QString keys = action->shortcut().toString(QKeySequence::NativeText);
    btn->setToolTip(keys.isEmpty() ? action->text() : QStringLiteral("%1 (%2)").arg(action->text(), keys));

    connect(btn, &QPushButton::clicked, this, [btn, action] {
        action->trigger();
        btn->setChecked(action->isChecked());   // кнопка сама переключилась — вернуть как у действия
    });
    connect(action, &QAction::toggled, btn, &QPushButton::setChecked);

    m_icons.insert(btn, svgPath);
    return btn;
}

/**
    @brief Тема Windows сменилась — перекрасить значки (цвет — из палитры приложения).
*/
void LeftMenu::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)
        updateIcons();
}

void LeftMenu::updateIcons()
{
    for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it)
        it.key()->setIcon(IconHelper::createThemedIcon(it.value()));
}
