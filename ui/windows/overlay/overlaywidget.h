#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

class OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayWidget(QWidget *parent = nullptr);

    void setPosition(int x, int y);
    void setOverlaySize(int width, int height);

private:
    QPushButton *m_testButton;
};