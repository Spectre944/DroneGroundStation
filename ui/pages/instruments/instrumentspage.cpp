#include "instrumentspage.h"
#include "ui_instrumentspage.h"

InstrumentsPage::InstrumentsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InstrumentsPage)
{
    ui->setupUi(this);

    auto spectrumView = new SpectrumView(this);
    auto eventLog = new EventLog(this);

    addInstrumentWidget(spectrumView);
    addInstrumentWidget(eventLog);

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
