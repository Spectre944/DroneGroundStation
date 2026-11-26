#ifndef INSTRUMENTSPAGE_H
#define INSTRUMENTSPAGE_H

#include <QWidget>

#include "ui/modules/spectrumView/spectrumview.h"
#include "ui/modules/eventLog/eventlog.h"
#include "ui/modules/timeSeries/timeseriesview.h"

namespace Ui {
class InstrumentsPage;
}

// Страница "Прилади" - пустой шаблон.
// Содержимое прокручивается; виджеты приборов добавляются в contentLayout
// (в дизайнере - перед verticalSpacer, или из кода через addInstrumentWidget()).
class InstrumentsPage : public QWidget
{
    Q_OBJECT

public:
    explicit InstrumentsPage(QWidget *parent = nullptr);
    ~InstrumentsPage();

    // Добавить виджет прибора в конец списка (над растяжкой)
    void addInstrumentWidget(QWidget *widget, int stretch = 0);

    /// «Графіки польоту» (Р2-9) — данные подаёт MainWindow::setupCore
    TimeSeriesView *timeSeriesView() const { return m_timeSeries; }

private:
    Ui::InstrumentsPage *ui;
    TimeSeriesView *m_timeSeries = nullptr;
};

#endif // INSTRUMENTSPAGE_H
