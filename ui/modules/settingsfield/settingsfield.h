#ifndef SETTINGSFIELD_H
#define SETTINGSFIELD_H

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>

#include <ui/components/styleconstants.h>
#include <ui/components/combobox/combobox.h>
#include <ui/components/spinbox/spinbox.h>

namespace Ui {
class SettingsField;
}

class SettingsField : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsField(QWidget *parent = nullptr);
    ~SettingsField();

    void setTextData(const QString &text, const QString &subtext);
    void setTooltip(const QString &text);

private:
    Ui::SettingsField *ui;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SettingsFieldBase: public QWidget{
    Q_OBJECT

public:
    SettingsFieldBase(QWidget *parent = nullptr);
    ~SettingsFieldBase();

    void setTextData(const QString &text, const QString &subtext);
    void setTooltip(const QString &text);
    QGridLayout *getMainLayout();
    void addWidgetToControlSlot(QWidget*);

private:
    QLabel      *name;
    QLabel      *description;
    QGridLayout *gLayout;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SettingsFieldCombobox: public QWidget{
    Q_OBJECT

public:
    SettingsFieldCombobox(QWidget *parent = nullptr);
    ~SettingsFieldCombobox();

    void setTextData(const QString &text, const QString &subtext);
    void setTooltip(const QString &text);
    void setTextUnits(const QString &text);
    void setDataRange(int, int);
    void setComboboxData(const QStringList &list);

    QVariant getValue();
    int currentIndex();
    void setValue(const QVariant& value);
    void setCurrentIndex(int);

signals:
    void currentTextChanged(const QString&);
    void currentIndexChanged(int);
    void activated(int);

private:
    SettingsFieldBase *baseField;
    ComboBoxUnits *combobox;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SettingsFieldSpinBox: public QWidget{
    Q_OBJECT

public:
    SettingsFieldSpinBox(QWidget *parent = nullptr);
    ~SettingsFieldSpinBox();

    void setTextData(const QString &text, const QString &subtext);
    void setTooltip(const QString &text);
    void setTextUnits(const QString &text);
    void setDataRange(int, int);

    QVariant getValue();
    void setValue(const QVariant& value);
    void loadValue(const QVariant& value);

signals:
    void valueChanged(const QVariant&);

private:
    SettingsFieldBase *baseField;
    SpinBoxUnits *spinbox;
    QTimer *debounceTimer;

};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SettingsFieldDoubleSpinBox: public QWidget{
    Q_OBJECT

public:
    SettingsFieldDoubleSpinBox(QWidget *parent = nullptr);
    ~SettingsFieldDoubleSpinBox();

    void setTextData(const QString &text, const QString &subtext);
    void setTooltip(const QString &text);
    void setTextUnits(const QString &text);
    void setDataRange(double, double);

    QVariant getValue();
    void setValue(const QVariant& value);
    void loadValue(const QVariant &value);

signals:
    void valueChanged(const QVariant&);

private:
    SettingsFieldBase *baseField;
    DoubleSpinBoxUnits *doubleSpinbox;
    QTimer *debounceTimer;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SettingsFieldButton: public QWidget{
    Q_OBJECT

public:
    SettingsFieldButton(QWidget *parent = nullptr);
    ~SettingsFieldButton();

    void setTextData(const QString &text, const QString &subtext);
    void setTooltip(const QString &text);
    void setButtonText(const QString &text);

signals:
    void buttonClicked();

private:
    SettingsFieldBase *baseField;
    QPushButton *button;
};

#endif // SETTINGSFIELD_H
