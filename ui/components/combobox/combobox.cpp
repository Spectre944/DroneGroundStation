#include "combobox.h"

ComboBoxIndicator::ComboBoxIndicator(QWidget *parent)
    : QComboBox(parent),
    m_indicator(this)
{
    m_indicator.setupLayout(this);

    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int newIndex) {
                if (newIndex != m_lastProgrammaticIndex) {
                    m_indicator.hide();
                }
            });

}

void ComboBoxIndicator::setIndicator(const IndicatorType &indicator, const QString &info)
{
    m_lastProgrammaticIndex = currentIndex();
    m_indicator.setIndicator(indicator, info);
}

ComboBoxUnits::ComboBoxUnits(QWidget *parent)
{
    Q_UNUSED(parent);
    hLayout = new QHBoxLayout(this);
    units = new QLabel();

    units->setText("");
    units->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    hSpacer = new QSpacerItem(15, 15, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
    units->setAlignment(Qt::AlignRight);

    hLayout->addWidget(units);
    hLayout->addItem(hSpacer);

}

void ComboBoxUnits::setUnitsText(const QString &text)
{
    units->setText(text);
}
