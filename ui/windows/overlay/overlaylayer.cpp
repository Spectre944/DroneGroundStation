#include "overlaylayer.h"

OverlayLayer::OverlayLayer(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QGridLayout(this);

    // Отступы от краёв карты
    m_layout->setContentsMargins(15, 15, 15, 15);

    // Расстояние между элементами
    m_layout->setSpacing(10);

    // Прозрачный фон
    setAttribute(Qt::WA_StyledBackground, false);
    setStyleSheet("background: transparent;");
}

void OverlayLayer::addOverlay(
    QWidget *widget,
    int row,
    int column,
    Qt::Alignment alignment)
{
    m_layout->addWidget(widget, row, column, alignment);
}