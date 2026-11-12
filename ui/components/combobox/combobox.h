#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QLineEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>

#include <ui/components/styleconstants.h>
#include <ui/components/custom/custom.h>

class ComboBoxIndicator : public QComboBox
{
    Q_OBJECT

public:
    ComboBoxIndicator(QWidget* parent = nullptr);
    void setIndicator(const IndicatorType &indicator, const QString &info);

private:
    IndicatorFeature    m_indicator;
    int                 m_lastProgrammaticIndex;
};

class ComboBoxUnits : public QComboBox
{
    Q_OBJECT

public:
    ComboBoxUnits(QWidget* parent = nullptr);
    void setUnitsText(const QString &text);

private:
    QLabel *units;
    QHBoxLayout *hLayout;
    QSpacerItem *hSpacer;
};


#endif // COMBOBOX_H
