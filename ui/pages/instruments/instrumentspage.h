#ifndef INSTRUMENTSPAGE_H
#define INSTRUMENTSPAGE_H

#include <QWidget>

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

private:
    Ui::InstrumentsPage *ui;
};

#endif // INSTRUMENTSPAGE_H
