#include "textedit.h"
#include <map/coordinatesmanager.h>

TextEditCoordinates::TextEditCoordinates(QWidget *parent)
    : QWidget{parent}
{
    setupUI();
    fillData();
    connections();
}

void TextEditCoordinates::setWindowDesription(const QString &title)
{
    m_label->setText(title);
}

void TextEditCoordinates::setPlaceholderText(const QString &text)
{
    m_textEdit->setPlaceholderText(text);
}

void TextEditCoordinates::setCoordinatesMode(CoordinatesMode mode)
{
    _mode = mode;
    QString iconPath;
    switch (mode) {
        case CoordinatesMode::Area:     m_label->setText("Вершини області");            iconPath = ":/icons/svg/custom/ReconAreaMode.svg";    break;
        case CoordinatesMode::Route:    m_label->setText("Точки маршруту");             iconPath = ":/icons/svg/custom/ReconWayMode_2.svg";   break;
        case CoordinatesMode::Other:    m_label->setText("Вершини області зараження");  iconPath = ":/icons/svg/custom/ReconAreaMode.svg";    break;
        default: break;
    }

    m_pushButton->setIcon(IconHelper::createThemedIcon(iconPath));
}

QString TextEditCoordinates::getCoordinateString()
{
    // Add validation 15/07/2025
   return m_textEdit->toPlainText();
}

bool TextEditCoordinates::isCoordinatesValid()
{
    return m_coordinatesValid;
}

int TextEditCoordinates::getCoordinatesCount()
{
    return m_coordinatesCount;
}

QWidget *TextEditCoordinates::getWidget(WidgetType type) const
{
    switch (type) {
        case WidgetType::ComboBox:      return m_comboBox;
        case WidgetType::Button:        return m_pushButton;
        case WidgetType::TextEdit:      return m_textEdit;
    }
    return nullptr;
}

TextEditCoordinates::CoordinatesMode TextEditCoordinates::getMode()
{
    return _mode;
}

void TextEditCoordinates::requestCoordinates()
{
    CoordinatesManager::instance()->requestCoordinatesFromMap(this);
}

void TextEditCoordinates::setCoordinatesFromMap(QVector<smappoint> coordinates)
{
    m_textEdit->clear();
    QString result;
    result.clear();
    foreach (auto point, coordinates) {
        result += QString("WGS84:%1, %2/").arg(point.latt).arg(point.longt);
    }
    result += "/";
    m_textEdit->append(result);
    emit editingFinished();
}

void TextEditCoordinates::setupUI()
{
    CoordinatesManager::instance()->registerCoordinateField(this);

    m_label          = new QLabel(this);
    m_comboBox       = new QComboBox(this);
    m_textEdit       = new QTextEdit(this);
    m_textEditLayout = new QGridLayout(m_textEdit);
    m_warningLabel   = new QLabel(this);
    m_pushButton     = new QPushButton(this);
    hLayout1         = new QHBoxLayout();
    hLayout2         = new QHBoxLayout();
    vLayout          = new QVBoxLayout(this);

    m_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_comboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_pushButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_textEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_comboBox->setFixedHeight(StyleConstants::textEditControlHeight);

    m_textEdit->setMinimumHeight(StyleConstants::textEditMinimumHeight);
    m_textEdit->setTabChangesFocus(true);
    m_textEdit->installEventFilter(this);

    //m_pushButton->setText("...");
    m_pushButton->setIcon(IconHelper::createThemedIcon(":/icons/svg/custom/ReconAreaMode.svg"));
    m_pushButton->setIconSize(QSize(25, 25));
    m_pushButton->setFixedSize(QSize(StyleConstants::textEditControlHeight, StyleConstants::textEditControlHeight));
    //m_pushButton->setFixedWidth(40);

    const QPixmap warningSign(":/icons/png/old/warning.png");
    m_warningLabel->setScaledContents(true);
    m_warningLabel->setPixmap(warningSign);
    m_warningLabel->setFixedSize(StyleConstants::textEditIconSize, StyleConstants::textEditIconSize);
    m_warningLabel->setToolTip("Помилка: Невалідний формат координат");
    m_warningLabel->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
    //m_warningLabel->hide(); // Скрываем по умолчанию

    m_textEditLayout->addWidget(m_warningLabel, 0, 0, Qt::AlignRight|Qt::AlignTop);

    hLayout1->addWidget(m_label);
    hLayout1->addWidget(m_comboBox);
    hLayout1->addWidget(m_pushButton);

    hLayout2->addWidget(m_textEdit);

    vLayout->addLayout(hLayout1);
    vLayout->addLayout(hLayout2);

    vLayout->setContentsMargins(0, 0, 0, 0);
    hLayout1->setContentsMargins(0, 0, 0, 0);
    hLayout2->setContentsMargins(0, 0, 0, 0);

    vLayout->setSpacing(10);
    hLayout1->setSpacing(10);
    hLayout2->setSpacing(10);

    setTabOrder({m_comboBox, m_pushButton, m_textEdit});

    setLayout(vLayout);

}

void TextEditCoordinates::fillData()
{

    m_comboBox->addItem("WGS84");
    m_comboBox->addItem("MGRS");
    m_comboBox->addItem("UTM");
    m_comboBox->addItem("DMS");
    // m_comboBox->addItem("CK42"); // Disable, has problem with converation

    m_label->setText("Вершини області зараження");

}

void TextEditCoordinates::connections()
{
    connect(m_comboBox, &QComboBox::activated, this, &TextEditCoordinates::convertCoordinates);
    connect(this, &TextEditCoordinates::editingFinished, this, &TextEditCoordinates::convertCoordinates);
    connect(m_pushButton, &QPushButton::clicked, this, &TextEditCoordinates::requestCoordinates);
}

void TextEditCoordinates::setValidationError(const QString &errorText)
{
    if (errorText.isEmpty()) {
        m_warningLabel->hide();
    } else {
        m_warningLabel->setToolTip(errorText);
        m_warningLabel->show();
    }
}

bool TextEditCoordinates::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_textEdit) {
        if (event->type() == QEvent::FocusOut) {
            emit editingFinished();
        } else if (event->type() == QEvent::KeyPress) {

            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (keyEvent->modifiers() & Qt::ControlModifier) {
                    emit editingFinished(); // Ctrl+Enter → подтверждение
                    return true; // блокируем стандартный перенос строки
                } else {
                    return false; // обычный Enter или Shift+Enter → разрешаем перенос
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TextEditCoordinates::convertCoordinates()
{
    // QPair<bool, QString> result = CoordHandler::convertPackageCoords(
    //     m_textEdit->toPlainText(),
    //     static_cast<CoordType>(m_comboBox->currentIndex() + 1)
    //     );

    // enum class CoordType { WGS84_Pair,  WGS84_String,  MGRS,  UTM,  DMS,  CK42,  NotValid};



    CoordType type;
    int index = m_comboBox->currentIndex() + 1;

    switch(index) {

        case 1:
            type = CoordType::WGS84_String;
            break;

        case 2:
            type = CoordType::MGRS;
            break;

        case 3:
            type = CoordType::UTM;
            break;

        case 4:
            type = CoordType::DMS;
            break;

        case 5:
            type = CoordType::CK42;
            break;

        default:
            return;
    }

    QString _text = m_textEdit->toPlainText();
    _text.remove(QRegularExpression("[\\s\\n\\r\\t]"));
    QPair<bool, QString> result = CoordHandler::convertPackageCoords(_text , type);
    m_coordinatesCount = CoordHandler::countCoordsInText(_text);


    if (result.first) {
        // Временно блокируем сигналы
        m_textEdit->blockSignals(true);
        m_textEdit->setPlainText(result.second);
        m_textEdit->blockSignals(false);
        m_coordinatesValid = true;
        setValidationError("");
    } else {
        setValidationError("Невалідний формат координат");
        m_coordinatesValid = false;
        m_coordinatesCount = 0;
    }

}

void TextEditCoordinates::clearTextEditContent()
{
    m_textEdit->clear();
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

TextEditAutofill::TextEditAutofill(QWidget *parent)
{
    Q_UNUSED(parent);
    setupUI();
    fillData();
}

void TextEditAutofill::setPlaceholderText(const QString &text)
{
    m_textEdit->setPlaceholderText(text);
}

QWidget *TextEditAutofill::getWidget(WidgetType type) const
{
    switch (type) {
        case WidgetType::TextEdit:    return m_textEdit;
        case WidgetType::PushButton:    return m_pushButtonAutofill;
    }
    return nullptr;
}

void TextEditAutofill::setupUI()
{
    m_textEdit = new QTextEdit(this);
    m_pushButtonAutofill = new QPushButton(this);
    m_pushButtonAutofill->setIcon(QIcon(":/icons/svg/bs5/pencil-square.svg"));
    m_pushButtonAutofill->setFixedSize(StyleConstants::textEditControlHeight, StyleConstants::textEditControlHeight);

    m_textEdit->setTabChangesFocus(true);
    m_pushButtonAutofill->setFlat(true);
    m_pushButtonAutofill->setFocusPolicy(Qt::FocusPolicy::TabFocus);
    m_pushButtonAutofill->setToolTip("Заповнити автоматично");

    // Пример layout — простой, только для размещения QTextEdit
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_textEdit);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    setTabOrder({m_textEdit, m_pushButtonAutofill});

    // Кнопка поверх
    m_pushButtonAutofill->raise();

}

void TextEditAutofill::fillData()
{

}

void TextEditAutofill::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // Позиционируем кнопку в правый верхний угол текстового поля
    QPoint topRight = m_textEdit->geometry().topRight();
    m_pushButtonAutofill->move(topRight - QPoint(m_pushButtonAutofill->width(), 0));
}
