#include "tabledata.h"
#include "ui_tabledata.h"

TableData::TableData(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TableData)
{
    ui->setupUi(this);

}

TableData::~TableData()
{
    delete ui;
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

TableControl::TableControl(QWidget *parent)
{
    Q_UNUSED(parent);
    setFixedHeight(40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    hLayoutControl = new QHBoxLayout(this);
    hLayoutControl->addWidget(frame);
    hLayoutControl->setContentsMargins(QMargins(0, 0, 0, 0));

    QHBoxLayout *frameLayout = new QHBoxLayout(frame);

    iconLabel = new QLabel(this);
    tableName = new QLabel(this);
    stackWidget = new QStackedWidget(this);
    clearBtn    = new QPushButton(this);
    adjustBtn   = new QPushButton(this);
    navigatorBtn = new QPushButton(this);

    iconLabel->setPixmap(QPixmap());
    iconLabel->setScaledContents(true);
    iconLabel->setMaximumSize(QSize(30, 30));

    tableName->setText("");

    clearBtn->setFlat(true);
    QIcon clearIcon = IconHelper::createThemedIcon(":/icons/svg/bs5/eraser.svg");
    clearBtn->setIcon(clearIcon);
    clearBtn->setFixedSize(QSize(40, 40));
    clearBtn->setIconSize(QSize(25, 30));
    clearBtn->setToolTip("Очистити");

    adjustBtn->setFlat(true);
    QIcon adjustIcon = IconHelper::createThemedIcon(":/icons/svg/custom/enlarge.svg");
    adjustBtn->setIcon(adjustIcon);
    adjustBtn->setFixedSize(QSize(40, 40));
    adjustBtn->setIconSize(QSize(25, 30));
    adjustBtn->setToolTip("Автопідбір ширини стовпців");

    navigatorBtn->setFlat(true);
    QIcon navigatorIcon = IconHelper::createThemedIcon(":/icons/svg/bs5/list-ul.svg");
    navigatorBtn->setIcon(navigatorIcon);
    navigatorBtn->setFixedSize(QSize(40, 40));
    navigatorBtn->setIconSize(QSize(30, 30));

    frameLayout->addWidget(iconLabel);
    frameLayout->addWidget(tableName);
    QSpacerItem *spacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    frameLayout->addItem(spacer);
    frameLayout->addWidget(clearBtn);
    frameLayout->addWidget(navigatorBtn);
    frameLayout->addWidget(adjustBtn);
    frameLayout->setSpacing(10);
    frameLayout->setContentsMargins(QMargins(10, 0, 10, 0));

    navigatorBtn->pos();

    connect(clearBtn, &QPushButton::clicked, this, [=](){ emit clearRequested(); });
    connect(adjustBtn, &QPushButton::clicked, this, [=](){ emit adjustRequested(); });
    connect(navigatorBtn, &QPushButton::clicked, this, [=](){ emit navigationPanelRequested(); });

}

TableControl::~TableControl()
{

}

QWidget *TableControl::getWidget(WidgetType type) const
{
    switch (type) {
        case WidgetType::Clear:    return clearBtn;
        case WidgetType::Adjust:    return adjustBtn;
        case WidgetType::Navigator:    return navigatorBtn;
    }
    return nullptr;
}

void TableControl::setButtonEnabled(WidgetType type, bool enabled)
{
    switch (type) {
        case WidgetType::Clear: clearBtn->setEnabled(enabled); break;
        case WidgetType::Adjust: adjustBtn->setEnabled(enabled); break;
        case WidgetType::Navigator: navigatorBtn->setEnabled(enabled); break;
        default: qWarning() << "Unknown WidgetType:" << static_cast<int>(type); break;
    }
}


void TableControl::setTableName(const QString &name)
{
    tableName->setText(name);
}

void TableControl::setIcon(const QPixmap &icon)
{
    iconLabel->setPixmap(icon);
}

void TableControl::setNavigatorBtnVisible(bool state)
{
    navigatorBtn->setVisible(state);
}
