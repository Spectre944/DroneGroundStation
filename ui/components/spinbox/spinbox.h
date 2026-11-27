#ifndef SPINBOXINDICATOR_H
#define SPINBOXINDICATOR_H

#include <QObject>
#include <QWidget>
#include <QSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QRegExp>
#include <QToolButton>
#include <QMenu>

#include <ui/components/styleconstants.h>
#include <ui/components/custom/custom.h>
#include <ui/components/menu/menu.h>

class SpinBoxIndicator : public QSpinBox
{
    Q_OBJECT
public:
    explicit SpinBoxIndicator(QWidget* parent = nullptr);
    void setIndicator(const IndicatorType &indicator, const QString &info);

private:
    IndicatorFeature    m_indicator;
    int                 m_lastProgrammaticValue;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SpinBoxUnits : public QSpinBox
{
    Q_OBJECT

public:
    SpinBoxUnits(QWidget* parent = nullptr);
    void setUnitsText(const QString &text);
    void updateUnitsGeometry();

private:
    QLabel *units;
    QHBoxLayout *hLayout;
    QSpacerItem *hSpacer;

private slots:
    void updateUnit(int);

protected:
    void resizeEvent(QResizeEvent *event) override;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class DoubleSpinBoxUnits : public QDoubleSpinBox
{
    Q_OBJECT

public:
    DoubleSpinBoxUnits(QWidget* parent = nullptr);
    void setUnitsText(const QString &text);
    void updateUnitsGeometry();
private:
    QLabel *units;
    QHBoxLayout *hLayout;
    QSpacerItem *hSpacer;
private slots:
    void updateUnit(double);

protected:
    void resizeEvent(QResizeEvent *event) override;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SpinBoxDirection : public QWidget
{
    Q_OBJECT

public:
    SpinBoxDirection(QWidget* parent = nullptr);
    QString text();

    //void setComboBoxVisible(bool);
    //void setSuffix(QString);

private:
    QSpinBox    *spinBox;
    QComboBox   *comboBox;
    QHBoxLayout *hLayout;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SpinBoxDistance : public QWidget
{
    Q_OBJECT

public:
    SpinBoxDistance(QWidget* parent = nullptr);
    QString text();

private:
    QSpinBox    *spinBox;
    QComboBox   *comboBox;
    QHBoxLayout *hLayout;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class SpinBoxText : public QSpinBox {
    Q_OBJECT
public:
    enum DisplayFormat {
        TextFormat,      // Только текст (С, Ю, З, В...)
        NumericFormat,   // Только градусы (0-360)
        CombinedFormat  // Текст + градусы
    };

    SpinBoxText(QWidget *parent = nullptr) : QSpinBox(parent), m_format(CombinedFormat) {
        setRange(0, 360);
        setWrapping(true);
        setAccelerated(true);

        dataInput = {
            tr("Північний"), tr("Північно-Східний"), tr("Східний"), tr("Південно-Східний"),
            tr("Південний"), tr("Південно-Західний"), tr("Західний"), tr("Північно-Західний"),
            tr("Пн"), tr("ПнСх"), tr("Сх"), tr("ПнСх"),
            tr("Пд"), tr("ПдЗх"), tr("Зх"), tr("ПнЗх")
        };

        // Создаем кнопку со стрелкой
        m_button = new QToolButton(this);
        m_button->setArrowType(Qt::DownArrow);
        m_button->setCursor(Qt::ArrowCursor);
        //m_button->setStyleSheet("QToolButton { border: none; }");

        // Создаем меню
        m_menu = new QMenu(this);
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-up.svg"),         tr("Північний"), [this]()          { setValue(0); });
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-up-right.svg"),   tr("Північно-Східний"), [this]()   { setValue(45); });
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-right.svg"),      tr("Східний"), [this]()            { setValue(90); });
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-down-right.svg"), tr("Південно-Східний"), [this]()   { setValue(135); });
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-down.svg"),       tr("Південний"), [this]()          { setValue(180); });
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-down-left.svg"),  tr("Південно-Західний"), [this]()  { setValue(225); });
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-left.svg"),       tr("Західний"), [this]()           { setValue(270); });
        m_menu->addAction(QIcon(":/icons/svg/bs5/arrow-up-left.svg"),    tr("Північно-Західний"), [this]()  { setValue(315); });

        // Привязываем меню к кнопке
        connect(m_button, &QToolButton::clicked, [this]() {
            QPoint p = mapToGlobal(QPoint(0, height()));
            m_menu->popup(p);
        });

    }

    void setDisplayFormat(DisplayFormat format) {
        m_format = format;
        update();
    }

    DisplayFormat displayFormat() const {
        return m_format;
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QSpinBox::resizeEvent(event);
        int buttonWidth = height();
        m_button->setGeometry(width() - buttonWidth, 0, buttonWidth, height());
    }

    QString textFromValue(int value) const override {
        QString text;

        // Получаем текстовое направление
        QString direction = getDirectionText(value);

        // Формируем вывод в зависимости от формата
        switch (m_format) {
        case TextFormat:
            return direction;
        case NumericFormat:
            return QString::number(value) + "°";
        case CombinedFormat:
            return QString("(%1°) %2").arg(value).arg(direction);
        default:
            return QString::number(value);
        }
    }

    int valueFromText(const QString &text) const override {

        QString comp = text.toLower();

        // Пробуем извлечь числовое значение
        QRegExp rxNum("(\\d+)");
        if (rxNum.indexIn(text) != -1) {
            return rxNum.cap(1).toInt();
        }

        // Если это текстовое направление, конвертируем его в градусы
        if (comp == tr("північний") || comp == tr("пн")) return 0;
        if (comp == tr("північно-східний")) return 45;
        if (comp == tr("східний") || comp == tr("сх")) return 90;
        if (comp == tr("південно-східний")) return 135;
        if (comp == tr("південний") || comp == tr("пд")) return 180;
        if (comp == tr("південно-західний")) return 225;
        if (comp == tr("західний") || comp == tr("зх")) return 270;
        if (comp == tr("північно-західний")) return 315;

        return value();
    }

    QValidator::State validate(QString &text, int &pos) const override {

        Q_UNUSED(pos);
        //text = text.toLower();

        // Разрешаем ввод чисел и текстовых направлений
        QRegExp rxNum("\\d+");
        if (rxNum.exactMatch(text)) {
            int val = text.toInt();
            if (val >= 0 && val <= 360) {
                return QValidator::Acceptable;
            }
        }

        // Проверяем на соответствие текстовым направлениям
        QString comp = text.toLower();
        if (dataInput.contains(text)) {
            return QValidator::Acceptable;
        }

        return QValidator::Invalid;
    }

private:
    DisplayFormat   m_format;
    QToolButton     *m_button;
    QMenu           *m_menu;
    QStringList     dataInput;


    QString getDirectionText(int value) const {
        if ((value >= 337.5 && value <= 360) || (value >= 0 && value < 22.5))
            return tr("Північний");
        if (value >= 22.5 && value < 67.5)
            return tr("Північно-Східний");
        if (value >= 67.5 && value < 112.5)
            return tr("Східний");
        if (value >= 112.5 && value < 157.5)
            return tr("Південно-Східний");
        if (value >= 157.5 && value < 202.5)
            return tr("Південний");
        if (value >= 202.5 && value < 247.5)
            return tr("Південно-Західний");
        if (value >= 247.5 && value < 292.5)
            return tr("Західний");
        if (value >= 292.5 && value < 337.5)
            return tr("Північно-Західний");
        return tr("Північний");  // По умолчанию
    }
};


#endif // SPINBOXINDICATOR_H
