#include "instrumentspage.h"
#include "ui_instrumentspage.h"

InstrumentsPage::InstrumentsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InstrumentsPage)
{
    ui->setupUi(this);

    m_spectrum = new SpectrumView(this);
    m_eventLog = new EventLog(this);
    m_timeSeries = new TimeSeriesView(this);

    addInstrumentWidget(m_spectrum);
    addInstrumentWidget(m_timeSeries);
    addInstrumentWidget(m_eventLog);

}

InstrumentsPage::~InstrumentsPage()
{
    delete ui;
}

void InstrumentsPage::addInstrumentWidget(QWidget *widget, int stretch)
{
    // Последний элемент contentLayout - verticalSpacer, вставляем перед ним
    const int index = qMax(0, ui->contentLayout->count() - 1);
    ui->contentLayout->insertWidget(index, widget, stretch);
}
