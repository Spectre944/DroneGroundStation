#include "button.h"

Button::Button(QWidget *parent)
    : QWidget{parent}
{

}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

LeftMenuButton::LeftMenuButton(QWidget *parent)
{
    Q_UNUSED(parent);
    QSizePolicy policy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);

    subMenu = new Menu(this);
    subMenu->setMinimumWidth(StyleConstants::menuMinimimWidht);

    setSizePolicy(policy);
    setIcon(QIcon(":/icons/svg/bs5/geo.svg"));
    setIconSize(QSize(StyleConstants::leftMenuIconSize,StyleConstants::leftMenuIconSize));
    setText("");
    setFlat(true);
    setToolTip("Home");
    setMinimumWidth(StyleConstants::leftMenuMinimumBtnHeight);
    setMinimumHeight(StyleConstants::leftMenuMinimumBtnWidth);
    setCheckable(true);
    setFocusPolicy(Qt::TabFocus);

    // Создаем и настраиваем индикаторный фрейм
    checkedFrame = new QFrame(this);
    checkedFrame->setFixedWidth(5);
    checkedFrame->setStyleSheet("background-color: #0078d7; border-radius: 2px;");
    checkedFrame->hide();

    // Располагаем индикаторный фрейм справа
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->addWidget(checkedFrame, 0, Qt::AlignLeft);
    setLayout(layout);

    // Связываем изменение состояния кнопки с отображением индикатора
    connect(this, &QPushButton::toggled, this, &LeftMenuButton::updateIndicator);

}

void LeftMenuButton::updateIndicator(bool checked)
{
    checkedFrame->setVisible(checked);
}

void LeftMenuButton::addSubPageMenu(const QList<QAction*> actions)
{
    subMenu->addActions(actions);

    // Open SubMenu by click
    connect(this, &QPushButton::clicked, subMenu, [this]() {
        subMenu->showAtButton(this, ShowPosition::RIGHT);
    });
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

MaterialButton::MaterialButton(QWidget *parent) : QPushButton(parent) {
    setMinimumHeight(36); // Высота кнопки Material Design
    setCursor(Qt::PointingHandCursor);
}

void MaterialButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);

    // Цвета для разных состояний кнопки
    QColor backgroundColor;
    QColor textColor = palette().buttonText().color();

    if (isDown()) {
        backgroundColor = palette().dark().color(); // Цвет при нажатии
    } else if (option.state & QStyle::State_MouseOver) {
        backgroundColor = palette().mid().color(); // Цвет при наведении
    } else {
        backgroundColor = palette().button().color(); // Основной цвет
    }

    // Отрисовка фона кнопки
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(backgroundColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), height() / 2, height() / 2); // Радиус скругления

    // Отрисовка текста
    painter.setPen(textColor);
    painter.drawText(rect(), Qt::AlignCenter, text());
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

DropdownButton::DropdownButton(QWidget *parent)
{
    Q_UNUSED(parent);
    buttonText = this->text();
    setPixmap(QPixmap(":/icons/svg/bs5/caret-down-fill.svg"));

    //setStyleSheet("text-align:left; padding-left: 10px;");

    dropMenu = new Menu(this);
    connect(this, &QPushButton::clicked, this, &DropdownButton::buttonClicked);
    connect(dropMenu, &Menu::menuClosed, this, &DropdownButton::updateButtonText);

}

void DropdownButton::setPixmap(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
}

void DropdownButton::updateButtonText()
{
    int checkedActions = 0;

    // Подсчитываем количество выбранных действий
    for (const QAction *action : dropMenu->actions()) {
        if (action->isChecked()) {
            ++checkedActions;
        }
    }

    // Устанавливаем текст в зависимости от типа кнопки
    if (buttonType == DropdownButtonType::SINGLE) {
        setText(buttonText); // Оставляем стандартный текст для одиночного выбора
    }
    else if (buttonType == DropdownButtonType::MULTIBLE) {
        setText(buttonText + QString(" [%1]").arg(checkedActions)); // Добавляем количество выбранных действий
    }
}

QSize DropdownButton::sizeHint() const
{
    const auto parentHint = QPushButton::sizeHint();
    // add margins here if needed
    return QSize(parentHint.width() + m_pixmap.width(), std::max(parentHint.height(), m_pixmap.height()));
}

void DropdownButton::addActions(const QList<QAction*> actions)
{
    dropMenu->addActions(actions);

    for (QAction *action : dropMenu->actions()) {
        connect(action, &QAction::toggled, this, &DropdownButton::updateButtonText);
        connect(action, &QAction::triggered, this, [this, action]() {
            actionSelected(action);
        });
    }
}

void DropdownButton::buttonClicked()
{
    dropMenu->showAtButton(this, ShowPosition::BOTTOM);
}

void DropdownButton::setInitialText(const QString &text)
{
    buttonText = text;
    setText(text);
}

void DropdownButton::setButtonType(const DropdownButtonType type)
{
    buttonType = type;
}

void DropdownButton::paintEvent(QPaintEvent *event)
{
    QPushButton::paintEvent(event);

    if (!m_pixmap.isNull()) {
        const int y = (height() - m_pixmap.height()) / 2; // add margin if needed
        const int x = (width() - m_pixmap.width()); // add margin if needed
        QPainter painter(this);
        painter.drawPixmap(x-10, y, m_pixmap); // hardcoded horizontal margin
    }
}

void DropdownButton::actionSelected(QAction *action)
{
    // Устанавливаем текст на кнопке в зависимости от типа выбора
    if (buttonType == DropdownButtonType::SINGLE) {
        setText(action->text());
        dropMenu->close(); // Закрываем меню для одиночного выбора
    } else if (buttonType == DropdownButtonType::MULTIBLE) {
        updateButtonText();
        if (!action->isCheckable()) {
            dropMenu->close(); // Закрываем меню, если элемент не checkable
        }
    }
}

QStringList DropdownButton::getSelectedActions() const
{
    QStringList selected;
    for (const QAction *action : dropMenu->actions()) {
        if (action->isChecked()) {
            selected << action->text();
        }
    }
    return selected;
}

QAction *DropdownButton::activeActon() const
{
    return dropMenu->activeAction();
}

QMenu *DropdownButton::getMenuPtr() const
{
    return dropMenu;
}

QList<QAction *> DropdownButton::checkedActions() const
{
    QList<QAction*> checked;
    foreach(QAction* action, dropMenu->actions())
    {
        if(action->isCheckable() && action->isChecked())
            checked.append(action);
    }
    return checked;
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
