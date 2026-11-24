#include "devicedetailswindow.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

DeviceDetailsWindow::DeviceDetailsWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window);
    setWindowTitle(tr("Прилад"));
    setMinimumWidth(420);
    buildUi();

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &DeviceDetailsWindow::refresh);
}

void DeviceDetailsWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);

    m_title = new QLabel(this);
    m_title->setProperty("textSize", "lg");
    QFont f = m_title->font();
    f.setBold(true);
    m_title->setFont(f);
    root->addWidget(m_title);

    // --- сведения о приборе ---
    auto *info = new QGridLayout();
    info->setHorizontalSpacing(12);
    info->setVerticalSpacing(6);
    auto addRow = [&](int row, const QString &name, QLabel *&value) {
        auto *l = new QLabel(name, this);
        l->setProperty("textRole", "secondary");
        value = new QLabel(this);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        info->addWidget(l, row, 0, Qt::AlignLeft | Qt::AlignTop);
        info->addWidget(value, row, 1);
    };

    auto *statusRow = new QHBoxLayout();
    m_statusIcon = new QLabel(this);
    m_statusIcon->setFixedSize(18, 18);
    m_statusIcon->setScaledContents(true);
    m_status = new QLabel(this);
    statusRow->addWidget(m_statusIcon);
    statusRow->addWidget(m_status, 1);
    auto *statusName = new QLabel(tr("Статус"), this);
    statusName->setProperty("textRole", "secondary");
    info->addWidget(statusName, 0, 0);
    info->addLayout(statusRow, 0, 1);

    addRow(1, tr("Підключення"),        m_source);
    addRow(2, tr("Останнє оновлення"),  m_lastUpdate);
    addRow(3, tr("Помилок"),            m_errors);
    addRow(4, tr("Серійний номер"),     m_serial);
    info->setColumnStretch(1, 1);
    root->addLayout(info);

    // --- подключение (только радиомодем) ---
    m_connBox = new QGroupBox(tr("Підключення до радіомодема"), this);
    auto *conn = new QVBoxLayout(m_connBox);

    auto *portRow = new QHBoxLayout();
    portRow->addWidget(new QLabel(tr("Порт"), m_connBox));
    m_ports = new QComboBox(m_connBox);
    m_ports->setMinimumWidth(180);
    m_ports->setToolTip(tr("«Тест (заглушка)» — тестовий політ без радіомодема"));
    portRow->addWidget(m_ports, 1);
    m_refresh = new QPushButton(tr("Оновити"), m_connBox);
    m_refresh->setToolTip(tr("Перечитати список COM-портів"));
    portRow->addWidget(m_refresh);
    conn->addLayout(portRow);

    auto *btnRow = new QHBoxLayout();
    m_connect = new QPushButton(tr("Підключити"), m_connBox);
    m_disconnect = new QPushButton(tr("Відключити"), m_connBox);
    btnRow->addWidget(m_connect);
    btnRow->addWidget(m_disconnect);
    btnRow->addStretch();
    conn->addLayout(btnRow);

    m_linkText = new QLabel(m_connBox);
    m_linkText->setWordWrap(true);
    conn->addWidget(m_linkText);
    root->addWidget(m_connBox);

    // --- действия ---
    auto *actions = new QHBoxLayout();
    m_resetErrors = new QPushButton(tr("Скинути лічильник помилок"), this);
    auto *close = new QPushButton(tr("Закрити"), this);
    actions->addWidget(m_resetErrors);
    actions->addStretch();
    actions->addWidget(close);
    root->addStretch();
    root->addLayout(actions);

    connect(m_refresh, &QPushButton::clicked, this, &DeviceDetailsWindow::refreshPorts);
    connect(m_connect, &QPushButton::clicked, this, [this] {
        const QString port = selectedPort();
        if (!port.isEmpty())
            emit connectRequested(port);
    });
    connect(m_disconnect, &QPushButton::clicked, this, &DeviceDetailsWindow::disconnectRequested);
    connect(m_ports, &QComboBox::currentIndexChanged, this, &DeviceDetailsWindow::updateConnectionUi);
    connect(m_resetErrors, &QPushButton::clicked, this, [this] {
        if (m_item)
            m_item->setErrorCount(0);
        refresh();
    });
    connect(close, &QPushButton::clicked, this, &QWidget::close);
}

void DeviceDetailsWindow::openFor(SettingsDeviceItem *item)
{
    m_item = item;
    const bool radio = item && item->data().devType == TypeDev::RADIO;
    m_connBox->setVisible(radio);
    if (radio)
        refreshPorts();

    refresh();
    adjustSize();
    show();
    raise();
    activateWindow();
    m_timer->start();
}

void DeviceDetailsWindow::refresh()
{
    if (!m_item || !isVisible()) {
        m_timer->stop();
        return;
    }
    const SettingsDeviceItem::DeviceData &d = m_item->data();
    setWindowTitle(d.name);
    m_title->setText(d.name);

    using DS = SettingsDeviceItem::DeviceStatus;
    QString text, icon;
    switch (d.status) {
    case DS::Active:       text = tr("Активний");     icon = ":/icons/png/old/okCheck.png"; break;
    case DS::Warning:      text = tr("Попередження"); icon = ":/icons/png/old/warning.png"; break;
    case DS::Inactive:     text = tr("Помилка");      icon = ":/icons/png/old/warning.png"; break;
    case DS::Disconnected: text = tr("Від'єднаний");  icon = ":/icons/png/old/error.png";   break;
    }
    m_status->setText(text);
    m_statusIcon->setPixmap(QPixmap(icon));
    m_source->setText(d.port);
    m_lastUpdate->setText(SettingsDeviceItem::lastUpdateText(d.lastUpdate));
    m_errors->setText(QString::number(d.errorCount));
    m_serial->setText(d.serial.isEmpty() || d.serial == "-" ? tr("невідомий") : d.serial);
    m_resetErrors->setEnabled(d.errorCount > 0);
}

// ------------------------------------------------------------------
//  Порт радиомодема
// ------------------------------------------------------------------

void DeviceDetailsWindow::refreshPorts()
{
    const QString keep = selectedPort().isEmpty() ? (m_link.portName.isEmpty() ? m_savedPort : m_link.portName)
                                                  : selectedPort();
    const QStringList ports = m_portProvider ? m_portProvider() : QStringList();

    m_ports->blockSignals(true);
    m_ports->clear();
    m_ports->addItem(tr("Тест (заглушка)"), QString::fromLatin1(STUB_PORT));
    for (const QString &p : ports)
        if (p != QLatin1String(STUB_PORT))
            m_ports->addItem(p, p);

    // Сохранённый / открытый порт, которого сейчас нет в системе (вынут USB) — показать, но пометить
    if (!keep.isEmpty() && m_ports->findData(keep) < 0)
        m_ports->addItem(tr("%1 (не знайдено)").arg(keep), keep);

    const int idx = m_ports->findData(keep);
    m_ports->setCurrentIndex(idx >= 0 ? idx : (ports.isEmpty() ? 0 : 1));
    m_ports->blockSignals(false);
    updateConnectionUi();
}

QString DeviceDetailsWindow::selectedPort() const
{
    return m_ports->currentData().toString();
}

void DeviceDetailsWindow::onLinkStatus(const LinkStatus &status)
{
    m_link = status;
    updateConnectionUi();
}

void DeviceDetailsWindow::updateConnectionUi()
{
    const bool open = m_link.state != LinkState::Closed;

    // Подключено — порт не меняем, пока не отключимся
    m_ports->setEnabled(!open);
    m_refresh->setEnabled(!open);
    m_connect->setEnabled(!open && !selectedPort().isEmpty());
    m_disconnect->setEnabled(open);

    QString state;
    QString color;
    switch (m_link.state) {
    case LinkState::Closed:  state = tr("порт закритий");        color = "#8A96A3"; break;
    case LinkState::Waiting: state = tr("очікування пакетів");   color = "#3D8BFD"; break;
    case LinkState::Ok:      state = tr("на зв'язку");           color = "#2E9E6A"; break;
    case LinkState::Silent:  state = tr("очікувана тиша (радіомовчання)"); color = "#3D8BFD"; break;
    case LinkState::Lost:    state = tr("зв'язок втрачено");     color = "#E5484D"; break;
    }
    QString text = tr("Стан: <b><span style='color:%1'>%2</span></b>").arg(color, state);
    if (open) {
        const QString port = m_link.portName == QLatin1String(STUB_PORT) ? tr("заглушка") : m_link.portName;
        text += QStringLiteral(" · ") + port;
        if (m_link.secondsSinceLastPacket >= 0)
            text += QStringLiteral(" · ") + tr("останній пакет %1").arg(
                        SettingsDeviceItem::lastUpdateText(m_link.secondsSinceLastPacket));
        if (m_link.commandPending)
            text += QStringLiteral(" · ") + tr("очікується відповідь на команду");
    }
    m_linkText->setText(text);
}
