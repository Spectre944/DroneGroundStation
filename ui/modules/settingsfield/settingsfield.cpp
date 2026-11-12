#include "settingsfield.h"
#include "qtimer.h"
#include "ui_settingsfield.h"

SettingsField::SettingsField(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsField)
{
    ui->setupUi(this);
}

SettingsField::~SettingsField()
{
    delete ui;
}

void SettingsField::setTextData(const QString &text, const QString &subtext)
{
    ui->labelText->setText(text);
    ui->labelSubText->setText(subtext);
}

void SettingsField::setTooltip(const QString &text)
{
    Q_UNUSED(text);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SettingsFieldBase::SettingsFieldBase(QWidget *parent)
{
    Q_UNUSED(parent);

    name        = new QLabel(this);
    description = new QLabel(this);
    gLayout     = new QGridLayout(this);

    name->setText("Main text");
    description->setText("Desription");

    name->setProperty("textSize", "md");
    description->setProperty("textSize", "xs");

    name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    description->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    gLayout->addWidget(name,        0, 0);
    gLayout->addWidget(description, 1, 0);

    gLayout->setContentsMargins(0, 0, 0, 0);
    gLayout->setSpacing(5);

}

SettingsFieldBase::~SettingsFieldBase()
{

}

void SettingsFieldBase::setTextData(const QString &text, const QString &subtext)
{
    name->setText(text);
    description->setText(subtext);
}

void SettingsFieldBase::setTooltip(const QString &text)
{
    setToolTip(text);
}

void SettingsFieldBase::addWidgetToControlSlot(QWidget *widget)
{
    gLayout->addWidget(widget, 0, 1, 2, 2);
}

QGridLayout *SettingsFieldBase::getMainLayout()
{
    return gLayout;
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SettingsFieldCombobox::SettingsFieldCombobox(QWidget *parent)
{
    Q_UNUSED(parent);
    baseField = new SettingsFieldBase(this);
    combobox =  new ComboBoxUnits(this);

    combobox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    combobox->setFixedSize(StyleConstants::settingsElementMinW, StyleConstants::settingsElementMinH);
    baseField->addWidgetToControlSlot(combobox);
    setLayout(baseField->getMainLayout());

    connect(combobox, &QComboBox::currentTextChanged, this, [this](){
        emit currentTextChanged(combobox->currentText());
    });

    connect(combobox, &QComboBox::currentIndexChanged, this, [this](int index){
        emit currentIndexChanged(index);
    });

    connect(combobox, &QComboBox::activated, this, [this](int index){
        emit activated(index);
    });

}

SettingsFieldCombobox::~SettingsFieldCombobox()
{

}

void SettingsFieldCombobox::setTextData(const QString &text, const QString &subtext)
{
    baseField->setTextData(text, subtext);
}

void SettingsFieldCombobox::setTooltip(const QString &text)
{
    baseField->setToolTip(text);
}

void SettingsFieldCombobox::setTextUnits(const QString &text)
{
    combobox->setUnitsText(text);
}

void SettingsFieldCombobox::setDataRange(int first, int last)
{
    for(int i = first; i <= last; i++){
        combobox->addItem(QString::number(i), i);
    }
}

void SettingsFieldCombobox::setComboboxData(const QStringList &list)
{
    combobox->addItems(list);
}

QVariant SettingsFieldCombobox::getValue()
{
    return combobox->currentText();
}

int SettingsFieldCombobox::currentIndex()
{
    return combobox->currentIndex();
}

void SettingsFieldCombobox::setValue(const QVariant &value)
{
    combobox->setCurrentIndex(combobox->findText(value.toString()));
}

void SettingsFieldCombobox::setCurrentIndex(int value)
{
    combobox->setCurrentIndex(value);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SettingsFieldSpinBox::SettingsFieldSpinBox(QWidget *parent)
{
    Q_UNUSED(parent);
    baseField = new SettingsFieldBase(this);
    spinbox =  new SpinBoxUnits(this);

    spinbox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    spinbox->setFixedSize(StyleConstants::settingsElementMinW, StyleConstants::settingsElementMinH);
    baseField->addWidgetToControlSlot(spinbox);
    setLayout(baseField->getMainLayout());

    debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(1000);

    connect(debounceTimer, &QTimer::timeout, this, [this](){
        qDebug() << "DebounceTimer valueChanged:" << (spinbox->value() == spinbox->minimum() ? QVariant() : QVariant(spinbox->value()));
        emit valueChanged(spinbox->value() == spinbox->minimum() ? QVariant() : QVariant(spinbox->value()));
    });
    connect(spinbox, &QSpinBox::valueChanged, this, [this](int value){
        qDebug() << "QSpinBox valueChanged:" << value; // Добавьте отладку!
        debounceTimer->stop();
        debounceTimer->start();
    });
}

SettingsFieldSpinBox::~SettingsFieldSpinBox()
{

}

void SettingsFieldSpinBox::setTextData(const QString &text, const QString &subtext)
{
    baseField->setTextData(text, subtext);
}

void SettingsFieldSpinBox::setTooltip(const QString &text)
{
    baseField->setToolTip(text);
}

void SettingsFieldSpinBox::setTextUnits(const QString &text)
{
    spinbox->setUnitsText(text);
}

void SettingsFieldSpinBox::setDataRange(int min, int max)
{
    spinbox->setRange(min, max);
    spinbox->setToolTip(QString("Мінімальне значення: %1\nМаксимальне значення: %2").arg(min).arg(max));
}

QVariant SettingsFieldSpinBox::getValue()
{
    return spinbox->value();
}

void SettingsFieldSpinBox::setValue(const QVariant &value)
{
    debounceTimer->stop();
    spinbox->setValue(value.toInt());
}

void SettingsFieldSpinBox::loadValue(const QVariant &value)
{
    setValue(value);

    // Вызывается явно когда все connections уже готовы
    QTimer::singleShot(0, this, [this]() {
        const int v = spinbox->value();
        emit valueChanged(v == spinbox->minimum() ? QVariant() : QVariant(v));
    });
}


//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SettingsFieldDoubleSpinBox::SettingsFieldDoubleSpinBox(QWidget *parent)
{
    Q_UNUSED(parent);
    baseField = new SettingsFieldBase(this);
    doubleSpinbox =  new DoubleSpinBoxUnits(this);

    doubleSpinbox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    doubleSpinbox->setFixedSize(StyleConstants::settingsElementMinW, StyleConstants::settingsElementMinH);
    baseField->addWidgetToControlSlot(doubleSpinbox);
    setLayout(baseField->getMainLayout());

    debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(1000);

    connect(debounceTimer, &QTimer::timeout, this, [this](){
        qDebug() << "DebounceTimer valueChanged:" << (doubleSpinbox->value() == doubleSpinbox->minimum() ? QVariant() : QVariant(doubleSpinbox->value()));
        emit valueChanged(doubleSpinbox->value() == doubleSpinbox->minimum() ? QVariant() : QVariant(doubleSpinbox->value()));
    });
    connect(doubleSpinbox, &QDoubleSpinBox::valueChanged, this, [this](double value){
        qDebug() << "QDoubleSpinBox valueChanged:" << value; // Добавьте отладку!
        debounceTimer->stop();
        debounceTimer->start();
    });
}

SettingsFieldDoubleSpinBox::~SettingsFieldDoubleSpinBox()
{

}

void SettingsFieldDoubleSpinBox::setTextData(const QString &text, const QString &subtext)
{
    baseField->setTextData(text, subtext);
}

void SettingsFieldDoubleSpinBox::setTooltip(const QString &text)
{
    baseField->setToolTip(text);
}

void SettingsFieldDoubleSpinBox::setTextUnits(const QString &text)
{
    doubleSpinbox->setUnitsText(text);
}

void SettingsFieldDoubleSpinBox::setDataRange(double min, double max)
{
    doubleSpinbox->setRange(min, max);
    doubleSpinbox->setToolTip(QString("Мінімальне значення: %1\nМаксимальне значення: %2").arg(min).arg(max));
}

QVariant SettingsFieldDoubleSpinBox::getValue()
{
    return doubleSpinbox->value();
}

void SettingsFieldDoubleSpinBox::setValue(const QVariant &value)
{
    debounceTimer->stop();
    doubleSpinbox->setValue(value.toDouble());
}

void SettingsFieldDoubleSpinBox::loadValue(const QVariant &value)
{
    setValue(value);

    // Вызывается явно когда все connections уже готовы
    QTimer::singleShot(0, this, [this]() {
        const double v = doubleSpinbox->value();
        emit valueChanged(v == doubleSpinbox->minimum() ? QVariant() : QVariant(v));
    });
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

SettingsFieldButton::SettingsFieldButton(QWidget *parent)
{
    Q_UNUSED(parent);
    baseField = new SettingsFieldBase(this);
    button =  new QPushButton(this);

    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    button->setFixedSize(StyleConstants::settingsElementMinW, StyleConstants::settingsElementMinH);
    baseField->addWidgetToControlSlot(button);
    setLayout(baseField->getMainLayout());

    connect(button, &QPushButton::clicked, this, &SettingsFieldButton::buttonClicked);
}

SettingsFieldButton::~SettingsFieldButton()
{

}

void SettingsFieldButton::setTextData(const QString &text, const QString &subtext)
{
    baseField->setTextData(text, subtext);
}

void SettingsFieldButton::setTooltip(const QString &text)
{
    baseField->setToolTip(text);
}

void SettingsFieldButton::setButtonText(const QString &text)
{
    button->setText(text);
}


