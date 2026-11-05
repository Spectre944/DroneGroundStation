#ifndef CUSTOM_H
#define CUSTOM_H

#include <QWidget>
#include <QLabel>
#include <QGridLayout>

#include <ui/components/styleconstants.h>

enum IndicatorType{
    OK,
    WARNING,
    ERROR
};

class IndicatorFeature{
public:



    explicit IndicatorFeature(QWidget* parent);
    ~IndicatorFeature();

    void setupLayout(QWidget* parent, int row = 0, int col = 0);
    void setIndicator(const IndicatorType &indicator, const QString &info);
    void hide();

private:
    QLabel* m_warningLabel;
    QPixmap* m_okIcon;
    QPixmap* m_warningIcon;
    QPixmap* m_errorIcon;
};

#endif // CUSTOM_H
