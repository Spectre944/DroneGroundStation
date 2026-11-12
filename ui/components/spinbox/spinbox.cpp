#include "spinbox.h"

SpinBoxIndicator::SpinBoxIndicator(QWidget *parent)
    : QSpinBox{parent},
    m_indicator(this)
{
    m_indicator.setupLayout(this);

    connect(this, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int newIndex) {
                if (newIndex != m_lastProgrammaticValue) {
                    m_indicator.hide();
                }
            });

}

void SpinBoxIndicator::setIndicator(const IndicatorType &indicator, const QString &info)
{
    m_lastProgrammaticValue = value();
    m_indicator.setIndicator(indicator, info);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SpinBoxUnits::SpinBoxUnits(QWidget *parent)
{
    Q_UNUSED(parent);
    setRange(0, 1000000);
    setSpecialValueText("Вимкнено");
    setAccelerated(true);
    setGroupSeparatorShown(true);

    hLayout = new QHBoxLayout(this);
    units = new QLabel();
    units->setVisible(false);
    units->setAttribute(Qt::WA_TransparentForMouseEvents);

    units->setText("");
    units->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    hSpacer = new QSpacerItem(30, 15, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
    units->setAlignment(Qt::AlignVCenter);
    // hSpacer->changeSize(15, 15, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

    hLayout->addWidget(units);
    // hLayout->addItem(hSpacer);

    connect(this, &SpinBoxUnits::valueChanged, this, &SpinBoxUnits::updateUnit);
}

void SpinBoxUnits::setUnitsText(const QString &text)
{
    units->setText(text);
}

void SpinBoxUnits::updateUnitsGeometry()
{
    QStyleOptionSpinBox opt;
    initStyleOption(&opt);
    QRect editRect = style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                             QStyle::SC_SpinBoxEditField, this);

    int w = units->sizeHint().width();
    units->setGeometry(editRect.right() - w, editRect.top(),
                       w, editRect.height());
}

void SpinBoxUnits::updateUnit(int value)
{
    bool visible = (value != minimum());
    units->setVisible(visible);
    if (visible) {
        updateUnitsGeometry(); // пересчёт позиции сразу после показа
    }
}

void SpinBoxUnits::resizeEvent(QResizeEvent *event)
{
    updateUnitsGeometry();
    QSpinBox::resizeEvent(event);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

DoubleSpinBoxUnits::DoubleSpinBoxUnits(QWidget *parent)
{
    Q_UNUSED(parent);
    setRange(0, 1000000);
    setSpecialValueText("Вимкнено");
    setAccelerated(true);
    setGroupSeparatorShown(true);

    hLayout = new QHBoxLayout(this);
    units = new QLabel();
    units->setVisible(false);
    units->setAttribute(Qt::WA_TransparentForMouseEvents);

    units->setText("");
    units->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // hSpacer = new QSpacerItem(15, 15, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
    units->setAlignment(Qt::AlignVCenter);

    hLayout->addWidget(units);
    // hLayout->addItem(hSpacer);

    connect(this, &DoubleSpinBoxUnits::valueChanged, this, &DoubleSpinBoxUnits::updateUnit);
}

void DoubleSpinBoxUnits::setUnitsText(const QString &text)
{
    units->setText(text);
}

void DoubleSpinBoxUnits::updateUnitsGeometry()
{
    QStyleOptionSpinBox opt;
    initStyleOption(&opt);
    QRect editRect = style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                             QStyle::SC_SpinBoxEditField, this);

    int w = units->sizeHint().width();
    units->setGeometry(editRect.right() - w, editRect.top(),
                       w, editRect.height());
}

void DoubleSpinBoxUnits::updateUnit(double value)
{
    bool visible = (value != minimum());
    units->setVisible(visible);
    if (visible) {
        updateUnitsGeometry(); // пересчёт позиции сразу после показа
    }
}

void DoubleSpinBoxUnits::resizeEvent(QResizeEvent *event)
{
    updateUnitsGeometry();
    QDoubleSpinBox::resizeEvent(event);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SpinBoxDirection::SpinBoxDirection(QWidget *parent)
{
    Q_UNUSED(parent);
    hLayout = new QHBoxLayout(this);

    spinBox = new QSpinBox(this);
    comboBox = new QComboBox(this);

    spinBox->setRange(-1, 360);
    spinBox->setSpecialValueText("-");
    spinBox->setAccelerated(true);
    spinBox->setWrapping(true);
    spinBox->setMinimumSize(StyleConstants::cbrnElementMinW, StyleConstants::cbrnElementMinH);

    comboBox->addItem("DGM (магнітний північний поліс)", "DGM");
    comboBox->addItem("DGG (географічний північний поліс)", "DGG");
    comboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    comboBox->setMinimumSize(StyleConstants::cbrnElementMinW, StyleConstants::cbrnElementMinH);

    hLayout->addWidget(spinBox);
    hLayout->addWidget(comboBox);
    hLayout->setContentsMargins(QMargins(0, 0, 0, 0));
    hLayout->setSpacing(10);
}

QString SpinBoxDirection::text()
{
    QString result = QString("%1%2")
                         .arg(spinBox->text())
                         .arg(comboBox->currentData().toString());

    return result;
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SpinBoxDistance::SpinBoxDistance(QWidget *parent)
{
    Q_UNUSED(parent);
    hLayout = new QHBoxLayout(this);

    spinBox = new QSpinBox(this);
    comboBox = new QComboBox(this);

    spinBox->setRange(0, 9999);
    spinBox->setSpecialValueText("-");
    spinBox->setAccelerated(true);
    spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spinBox->setMinimumSize(StyleConstants::cbrnElementMinW, StyleConstants::cbrnElementMinH);

    comboBox->addItem("M : метрів", "M");
    comboBox->addItem("KM : кілометрів", "KM");
    comboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    comboBox->setMinimumSize(StyleConstants::cbrnElementMinW, StyleConstants::cbrnElementMinH);

    hLayout->addWidget(spinBox);
    hLayout->addWidget(comboBox);
    hLayout->setContentsMargins(QMargins(0, 0, 0, 0));
    hLayout->setSpacing(10);
}

QString SpinBoxDistance::text()
{
    QString result = QString("%1%2")
                            .arg(spinBox->text())
                            .arg(comboBox->currentData().toString());

    return result;
}
