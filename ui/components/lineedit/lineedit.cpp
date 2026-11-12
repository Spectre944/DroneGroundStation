#include "lineedit.h"

LineEditReport::LineEditReport(QWidget *parent)
    : QWidget(parent), lineEdit(new QLineEdit(this)), comboBox(new QComboBox(this)) {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(lineEdit);
    layout->addWidget(comboBox);
    setLayout(layout);

    connect(comboBox, &QComboBox::currentTextChanged, this, &LineEditReport::onComboBoxItemSelected);
}

void LineEditReport::loadComboBoxData(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open JSON file.");
        return;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(data));
    QJsonArray array = doc.array();

    for (const QJsonValue &value : array) {
        QJsonObject obj = value.toObject();
        QString abbreviation = obj["abbreviation"].toString();
        QString description = obj["description"].toString();
        abbreviations.insert(abbreviation, description);
        comboBox->addItem(abbreviation + " - " + description, abbreviation);
    }
}

void LineEditReport::onComboBoxItemSelected(const QString &item) {
    Q_UNUSED(item);
    QString abbreviation = comboBox->currentData().toString();
    int cursorPos = lineEdit->cursorPosition();
    QString text = lineEdit->text();
    text.insert(cursorPos, abbreviation + "/");
    lineEdit->setText(text);
    lineEdit->setCursorPosition(cursorPos + abbreviation.length() + 1);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
LineEditButton::LineEditButton(QWidget *parent)
    : QWidget(parent), lineEdit(new QLineEdit(this)), actionButton(new QPushButton("...", this)), menu(new QMenu(this)) {

    actionButton->setFixedSize(QSize(StyleConstants::lineEditControlHeight, StyleConstants::lineEditControlHeight));

    this->setMinimumHeight(StyleConstants::lineEditControlHeight);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); // Устанавливаем нулевые отступы
    layout->setSpacing(0); // Устанавливаем нулевое расстояние между виджетами
    layout->addWidget(lineEdit);
    layout->addWidget(actionButton);

    lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    actionButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    this->setLayout(layout);

    QAction *autoFillAction = new QAction("Автозаполнение", this);
    QAction *manualInputAction = new QAction("Ручной ввод", this);
    QAction *check = new QAction("Check", this);
    check->setCheckable(true);

    connect(autoFillAction, &QAction::triggered, this, &LineEditButton::onAutoFillSelected);
    connect(manualInputAction, &QAction::triggered, this, &LineEditButton::onManualInputSelected);

    menu->addSection("Автозаповнення");
    menu->addAction(autoFillAction);
    menu->addAction(manualInputAction);
    menu->addAction(check);

    connect(actionButton, &QPushButton::clicked, this, &LineEditButton::onActionButtonClicked);
}

void LineEditButton::onActionButtonClicked() {
    menu->exec(actionButton->mapToGlobal(QPoint(0, actionButton->height())));
}

void LineEditButton::onAutoFillSelected() {
    // Логика для автозаполнения
    lineEdit->setText("Автозаполнение выполнено");
}

void LineEditButton::onManualInputSelected() {
    // Логика для ручного ввода
    lineEdit->setText("Ручной ввод выбран");
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

LineEditMenu::LineEditMenu(QWidget *parent) : QLineEdit(parent)
{
    setMinimumHeight(StyleConstants::lineEditControlHeight);

    //m_Menu = createStandardContextMenu();
    //m_Menu->addAction("New action");


    m_autofillBtn = new QPushButton(this);
    m_autofillBtn->setText("...");
    m_autofillBtn->setFixedSize(QSize(StyleConstants::lineEditControlHeight, StyleConstants::lineEditControlHeight));

    QSpacerItem *horizontalSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

    hLayout = new QHBoxLayout(this);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addItem(horizontalSpacer);
    hLayout->addWidget(m_autofillBtn);

    setLayout(hLayout);
}

// void LineEditMenu::contextMenuEvent(QContextMenuEvent *event)
// {
//     //m_Menu->exec(event->globalPos());
// }

LineEditCoordinates::LineEditCoordinates(QWidget *parent)
: QWidget(parent)
{
    setupUI();
    fillData();
    connections();

}

QString LineEditCoordinates::text()
{
    QString result = m_comboBox->currentText() + ":" + m_lineEdit->text();
    return result;
}

QWidget *LineEditCoordinates::getWidget(WidgetType type) const
{
    switch (type) {
        case WidgetType::ComboBox:    return m_comboBox;
        case WidgetType::LineEdit:    return m_lineEdit;
    }
    return nullptr;
}

void LineEditCoordinates::showContextMenu(const QPoint &pos)
{
    m_Menu->exec(m_lineEdit->mapToGlobal(pos));
}

void LineEditCoordinates::setupUI()
{
    setMinimumHeight(StyleConstants::lineEditControlHeight);

    m_comboBox  = new QComboBox(this);
    m_lineEdit  = new QLineEdit(this);
    m_warningLabel = new QLabel(this);
    m_Menu      = new QMenu(m_lineEdit);
    hLayout     = new QHBoxLayout(this);
    m_lineEditLayout = new QGridLayout(m_lineEdit);

    m_lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_comboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_lineEdit->setMinimumSize(StyleConstants::cbrnElementMinW, StyleConstants::cbrnElementMinH);
    m_comboBox->setMinimumSize(StyleConstants::cbrnElementMinW, StyleConstants::cbrnElementMinH);

    m_Menu = m_lineEdit->createStandardContextMenu();
    m_lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);

    const QPixmap warningSign(":/icons/png/old/warning.png");
    m_warningLabel->setScaledContents(true);
    m_warningLabel->setPixmap(warningSign);
    m_warningLabel->setFixedSize(StyleConstants::textEditIconSize, StyleConstants::textEditIconSize);
    m_warningLabel->setToolTip("Помилка: Невалідний формат координат");
    m_warningLabel->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
    //m_warningLabel->hide(); // Скрываем по умолчанию

    m_lineEditLayout->addWidget(m_warningLabel, 0, 0, Qt::AlignRight|Qt::AlignTop);

    hLayout->addWidget(m_comboBox);
    hLayout->addWidget(m_lineEdit);

    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(10);

    setTabOrder({m_comboBox, m_lineEdit});

    setLayout(hLayout);
}

void LineEditCoordinates::fillData()
{
    m_Menu->addAction("Автоаповнити координати");

    m_comboBox->addItem("WGS84");
    m_comboBox->addItem("MGRS");
    m_comboBox->addItem("UTM");
    m_comboBox->addItem("DMS");
    m_comboBox->addItem("CK42");

}

void LineEditCoordinates::connections()
{
    connect(m_lineEdit, &QLineEdit::customContextMenuRequested, this, &LineEditCoordinates::showContextMenu);
    connect(m_lineEdit, &QLineEdit::editingFinished, this, &LineEditCoordinates::convertCoordinates);
    connect(m_comboBox, &QComboBox::activated, this, &LineEditCoordinates::convertCoordinates);
}

void LineEditCoordinates::convertCoordinates()
{
    QPair<bool, QString> result = CoordHandler::convertSingleCoord(
        m_lineEdit->text(),
        static_cast<CoordType>(m_comboBox->currentIndex() + 1)
        );

    if (result.first) {
        m_lineEdit->setText(result.second);
        setValidationError("");
    } else {
        setValidationError("Невалідний формат координат");
    }
}

void LineEditCoordinates::setValidationError(const QString &errorText)
{
    if (errorText.isEmpty()) {
        m_warningLabel->hide();
    } else {
        m_warningLabel->setToolTip(errorText);
        m_warningLabel->show();
    }
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

LineEditAutofill::LineEditAutofill(QWidget *parent)
{
    Q_UNUSED(parent);
    setupUI();
    fillData();
}

void LineEditAutofill::setupUI()
{
    m_lineEdit = new QLineEdit(this);
    m_pushButtonAutofill = new QPushButton(this);
    m_pushButtonAutofill->setIcon(QIcon(":/icons/svg/bs5/pencil-square.svg"));
    m_pushButtonAutofill->setFixedSize(StyleConstants::textEditControlHeight, StyleConstants::textEditControlHeight);

    m_lineEdit->setMinimumHeight(StyleConstants::lineEditControlHeight);
    m_pushButtonAutofill->setFlat(true);
    m_pushButtonAutofill->setFocusPolicy(Qt::FocusPolicy::TabFocus);
    m_pushButtonAutofill->setToolTip("Заповнити автоматично");

    // Пример layout — простой, только для размещения QTextEdit
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_lineEdit);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    setTabOrder({m_lineEdit, m_pushButtonAutofill});

    // Кнопка поверх
    m_pushButtonAutofill->raise();

}

void LineEditAutofill::fillData()
{

}

void LineEditAutofill::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // Позиционируем кнопку в правый верхний угол текстового поля
    QPoint topRight = m_lineEdit->geometry().topRight();
    m_pushButtonAutofill->move(topRight - QPoint(m_pushButtonAutofill->width(), 0));
}

QWidget *LineEditAutofill::getWidget(WidgetType type) const
{
    switch (type) {
        case WidgetType::LineEdit:    return m_lineEdit;
        case WidgetType::PushButton:    return m_pushButtonAutofill;
    }
    return nullptr;
}
