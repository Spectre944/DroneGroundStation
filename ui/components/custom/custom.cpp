#include "custom.h"

IndicatorFeature::IndicatorFeature(QWidget *parent)
{
    m_warningLabel = new QLabel(parent);
    m_okIcon = new QPixmap(":/icons/png/old/okCheck.png");
    m_warningIcon = new QPixmap(":/icons/png/old/warning.png");
    m_errorIcon = new QPixmap(":/icons/png/old/error.png");

    m_warningLabel->setScaledContents(true);
    m_warningLabel->setPixmap(*m_okIcon);
    m_warningLabel->setFixedSize(StyleConstants::indicatorIconSize,
                                 StyleConstants::indicatorIconSize);
    m_warningLabel->hide();
}

IndicatorFeature::~IndicatorFeature()
{
    delete m_okIcon;
    delete m_warningIcon;
    delete m_errorIcon;
    // m_warningLabel удалится автоматически как child виджет
}

void IndicatorFeature::setupLayout(QWidget *parent, int row, int col)
{
    auto layout = qobject_cast<QGridLayout*>(parent->layout());
    if (!layout) {
        layout = new QGridLayout(parent);
        layout->setContentsMargins(0, 0, 20, 0);
    }
    layout->addWidget(m_warningLabel, row, col, Qt::AlignRight|Qt::AlignCenter);
}

void IndicatorFeature::setIndicator(const IndicatorType &indicator, const QString &info)
{
    m_warningLabel->show();
    m_warningLabel->setToolTip(info);
    switch (indicator) {
        case IndicatorType::OK:         m_warningLabel->setPixmap(*m_okIcon);           break;
        case IndicatorType::WARNING:    m_warningLabel->setPixmap(*m_warningIcon);      break;
        case IndicatorType::ERROR:      m_warningLabel->setPixmap(*m_errorIcon);        break;
        default:    break;
    }
}

void IndicatorFeature::hide()
{
    m_warningLabel->hide();
}
