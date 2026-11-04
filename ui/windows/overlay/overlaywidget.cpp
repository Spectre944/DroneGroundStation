#include "overlaywidget.h"

#include <QDebug>

OverlayWidget::OverlayWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);

    // Appearance
    setStyleSheet(R"(
        OverlayWidget {
            background-color: rgba(30, 35, 45, 220);
            border: 1px solid rgba(255, 255, 255, 35);
            border-radius: 12px;
        }

        QPushButton {
            background-color: #3478F6;
            color: white;
            border: none;
            border-radius: 7px;
            padding: 8px 16px;
        }

        QPushButton:hover {
            background-color: #4B8BFF;
        }

        QPushButton:pressed {
            background-color: #245CC0;
        }
    )");

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);

    // Test button
    m_testButton = new QPushButton("Test click", this);

    layout->addWidget(m_testButton);

    connect(m_testButton, &QPushButton::clicked, this, []() {
        qDebug() << "Overlay button clicked!";
    });

    resize(160, 60);
}

void OverlayWidget::setPosition(int x, int y)
{
    move(x, y);
}

void OverlayWidget::setOverlaySize(int width, int height)
{
    resize(width, height);
}