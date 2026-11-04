#pragma once

#include <QWidget>
#include <QGridLayout>

class OverlayLayer : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayLayer(QWidget *parent = nullptr);

    void addOverlay(
        QWidget *widget,
        int row,
        int column,
        Qt::Alignment alignment = Qt::Alignment()
        );

private:
    QGridLayout *m_layout;
};