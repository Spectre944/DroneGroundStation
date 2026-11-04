#include "leftmenu.h"

LeftMenuButton::LeftMenuButton(QWidget *parent)
{
    Q_UNUSED(parent);
    QSizePolicy policy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);

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

}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

LeftMenu::LeftMenu(QWidget *parent)
    : QWidget{parent}
{
    setupUI();
    fillData();
    connections();
    hideUnusedUI();
    setupShortcuts();
}

void LeftMenu::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    if(isExpandble)
        expandMenu();
}

void LeftMenu::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if(isExpandble)
        collapseMenu();
}

void LeftMenu::setupUI()
{
    setObjectName("LeftMenuWidget");

    deviceBtn       = new LeftMenuButton(this);
    databaseBtn     = new LeftMenuButton(this);
    cbrnBtn         = new LeftMenuButton(this);
    reconBtn        = new LeftMenuButton(this);
    settingsBtn     = new LeftMenuButton(this);

    deviceBtn->setChecked(true);

    group = new QButtonGroup(this);

    group->addButton(deviceBtn);
    group->addButton(databaseBtn);
    group->addButton(cbrnBtn);
    group->addButton(reconBtn);
    group->addButton(settingsBtn);

    verticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

    layout = new QVBoxLayout(this);

    layout->addWidget(deviceBtn);
    layout->addWidget(databaseBtn);
    layout->addWidget(cbrnBtn);
    layout->addWidget(reconBtn);
    layout->addItem(verticalSpacer);
    layout->addWidget(settingsBtn);

    layout->setSpacing(10);
    layout->setContentsMargins(0, 0, 0, 0);

    setLayout(layout);

    setTabOrder(deviceBtn, databaseBtn);
    setTabOrder(databaseBtn, cbrnBtn);
    setTabOrder(cbrnBtn, reconBtn);
    setTabOrder(reconBtn, deviceBtn);

    //setFixedWidth(collapsedWidth);

    animation = new QPropertyAnimation(this, "minimumWidth");
    animation->setDuration(300);

}

void LeftMenu::hideUnusedUI()
{
    cbrnBtn->setVisible(false);
    cbrnBtn->setEnabled(false);
}

void LeftMenu::fillData()
{
    deviceBtn       ->setToolTip("Прилади");
    databaseBtn     ->setToolTip("База даних");
    cbrnBtn         ->setToolTip("Донесення");
    reconBtn        ->setToolTip("Розвідка");
    settingsBtn     ->setToolTip("Налаштування");

    deviceBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/house.svg"));
    databaseBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/database.svg"));
    cbrnBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/journal-bookmark.svg"));
    reconBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/geo-alt.svg"));
    settingsBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/gear.svg"));
}

void LeftMenu::connections()
{
    connect(deviceBtn,      &QPushButton::clicked, this, [=]() { emit changePage(0); });
    connect(databaseBtn,    &QPushButton::clicked, this, [=]() { emit changePage(1); });
    connect(cbrnBtn,        &QPushButton::clicked, this, [=]() { emit changePage(2); });
    connect(reconBtn,       &QPushButton::clicked, this, [=]() { emit changePage(3); });
    connect(settingsBtn,    &QPushButton::clicked, this, [=]() { emit changePage(4); });


    connect(qApp, &QApplication::paletteChanged, [=]() {
        deviceBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/house.svg"));
        databaseBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/database.svg"));
        cbrnBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/journal-bookmark.svg"));
        reconBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/geo-alt.svg"));
        settingsBtn->setIcon(IconHelper::createThemedIcon(":/icons/svg/bs5/gear.svg"));
    });
}

void LeftMenu::setupShortcuts()
{
    int shortcutIndex = 1; // Начинаем с Ctrl+1

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);

        if (item && item->widget()) {
            QPushButton* button = qobject_cast<QPushButton*>(item->widget());

            if (button && button->isEnabled()) {
                QString shortcut = QString("Ctrl+%1").arg(shortcutIndex);
                button->setShortcut(QKeySequence(shortcut));
                shortcutIndex++;
            } else if (button) {
                button->setShortcut(QKeySequence());
            }
        }
    }
}

void LeftMenu::expandMenu()
{
    animation->stop();
    animation->setStartValue(width());
    animation->setEndValue(expandedWidth);
    animation->start();
}

void LeftMenu::collapseMenu()
{
    animation->stop();
    animation->setStartValue(width());
    animation->setEndValue(collapsedWidth);
    animation->start();
}

LeftMenu::~LeftMenu()
{
    delete layout;
    delete group;
}

void LeftMenu::nextPage()
{
    // Увеличиваем `currentPage`, но не выходим за границы
    currentPage = (currentPage + 1) % 5;  // Если страниц 5, то вернётся на первую после последней

    // Имитируем нажатие кнопки, соответствующей `currentPage`
    switch (currentPage) {
        case 0: deviceBtn->click(); break;
        case 1: databaseBtn->click(); break;
        case 2: cbrnBtn->click(); break;
        case 3: reconBtn->click(); break;
        case 4: settingsBtn->click(); break;
    }
}


void LeftMenu::pageChanged(int pageIndex)
{
    currentPage = pageIndex;
}
