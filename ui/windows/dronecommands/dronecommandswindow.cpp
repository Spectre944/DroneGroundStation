#include "dronecommandswindow.h"

#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <utility>

#include "ui/modules/coreTexts/coretexts.h"

namespace {

QString modeText(LoggerMode m)
{
    switch (m) {
    case LoggerMode::Test:   return QStringLiteral("перевірка");
    case LoggerMode::Expect: return QStringLiteral("очікування (політ до області)");
    case LoggerMode::Work:   return QStringLiteral("розвідка");
    case LoggerMode::Return: return QStringLiteral("повернення");
    case LoggerMode::Error:  return QStringLiteral("аварія");
    default:                 return QStringLiteral("невідомо");
    }
}

QSpinBox *spin(int min, int max, int value, const QString &suffix, QWidget *parent)
{
    auto *s = new QSpinBox(parent);
    s->setRange(min, max);
    s->setValue(value);
    s->setSuffix(suffix);
    s->setMinimumWidth(90);
    return s;
}

} // namespace

DroneCommandsWindow::DroneCommandsWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window);
    setWindowTitle(tr("Команди дрону"));
    buildUi();
    updateState();
}

void DroneCommandsWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);

    m_state = new QLabel(this);
    m_state->setWordWrap(true);
    root->addWidget(m_state);

    root->addWidget(addGroup(tr("Параметри розвідки"),       {5, 6, 7, 9, 11}));
    root->addWidget(addGroup(tr("Вимірювання і спектр"),     {10, 12}));
    root->addWidget(addGroup(tr("Радіомовчання"),            {13, 14}));
    root->addWidget(addGroup(tr("Завершення"),               {8}));
    root->addStretch();
}

QWidget *DroneCommandsWindow::addGroup(const QString &title, const QList<quint8> &ids)
{
    auto *box = new QGroupBox(title, this);
    auto *grid = new QGridLayout(box);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(3, 1);
    grid->setHorizontalSpacing(10);

    int row = 0;
    for (quint8 id : ids) {
        switch (id) {
        case 5:  addRow(grid, row++, id, tr("Коефіцієнт дублювання"), spin(2, 5, 2, QString(), box), tr("Задати")); break;
        case 6:  addRow(grid, row++, id, tr("Інтервал вимірювань"), spin(1, 65535, 60, tr(" с"), box), tr("Задати")); break;
        case 7:  addRow(grid, row++, id, tr("Відстань між вимірюваннями"), spin(1, 65535, 100, tr(" м"), box), tr("Задати")); break;
        case 9: {
            auto *c = new QComboBox(box);
            c->addItems({tr("За командою оператора"), tr("За часом"), tr("За відстанню"), tr("Набір спектра")});
            c->setCurrentIndex(2);
            addRow(grid, row++, id, tr("Режим розвідки"), c, tr("Задати"));
            break;
        }
        case 11: {
            QSpinBox *s = spin(0, 65535, 60, tr(" с"), box);
            s->setSpecialValueText(tr("до ID12"));
            addRow(grid, row++, id, tr("Час набору спектра"), s, tr("Задати"));
            break;
        }
        case 10: addRow(grid, row++, id, tr("Виміряти зараз"), nullptr, tr("Виміряти")); break;
        case 12: addRow(grid, row++, id, tr("Зупинити набір спектра"), nullptr, tr("Зупинити")); break;
        case 13: addRow(grid, row++, id, tr("Увімкнути радіомовчання"), nullptr, tr("Увімкнути")); break;
        case 14: addRow(grid, row++, id, tr("Вийти з радіомовчання"), nullptr, tr("Вийти")); break;
        case 8:  addRow(grid, row++, id, tr("Закінчити розвідку"), nullptr, tr("Закінчити")); break;
        default: break;
        }
    }
    return box;
}

void DroneCommandsWindow::addRow(QGridLayout *grid, int row, quint8 id, const QString &title,
                           QWidget *value, const QString &buttonText)
{
    Row r;
    r.id = id;
    r.value = value;

    auto *label = new QLabel(QStringLiteral("%1  <span style='color:#8A96A3'>ID%2</span>")
                                 .arg(title.toHtmlEscaped()).arg(id, 2, 10, QLatin1Char('0')),
                             grid->parentWidget());
    grid->addWidget(label, row * 2, 0);
    if (value)
        grid->addWidget(value, row * 2, 1);

    r.button = new QPushButton(buttonText, grid->parentWidget());
    r.button->setMinimumWidth(96);
    grid->addWidget(r.button, row * 2, 2);

    r.result = new QLabel(grid->parentWidget());
    r.result->setWordWrap(true);
    r.result->hide();
    grid->addWidget(r.result, row * 2 + 1, 0, 1, 3);

    connect(r.button, &QPushButton::clicked, this, [this, id] { send(id); });
    m_rows.insert(id, r);
}

int DroneCommandsWindow::valueOf(const Row &r) const
{
    if (auto *s = qobject_cast<QSpinBox *>(r.value))
        return s->value();
    if (auto *c = qobject_cast<QComboBox *>(r.value))
        return c->currentIndex();
    return 0;
}

bool DroneCommandsWindow::confirm(quint8 id, int value)
{
    QString text;
    if (id == 8)
        text = tr("Логер закриє файл поточної області, зупинить вимірювання і перейде в «Повернення».\n\nЗакінчити розвідку?");
    else if (id == 13)
        text = tr("Дрон перестане передавати дані й відповідати на команди до команди «Вийти з радіомовчання» (ID14).\n\nУвімкнути радіомовчання?");
    else if (id == 9 && value == 3)
        text = tr("Для набору спектра дрон має зависнути над точкою.\n\nПочати набір спектра?");
    else if (id == 12 && m_logger.spectrumStage == SpectrumStage::Collecting)
        text = tr("Набір спектра буде зупинено. Продовжити?");
    if (text.isEmpty())
        return true;
    return QMessageBox::question(this, windowTitle(), text) == QMessageBox::Yes;
}

void DroneCommandsWindow::send(quint8 id)
{
    auto it = m_rows.find(id);
    if (it == m_rows.end() || !it->button->isEnabled())
        return;

    const int value = valueOf(*it);
    if (!confirm(id, value))
        return;
    updateState();                       // за время подтверждения могло всё поменяться
    if (!it->button->isEnabled())
        return;

    CommandRequest r;
    r.id = id;
    r.value = value;

    m_pendingId = id;
    m_pendingSeq = 0;
    m_sending = true;
    m_early.clear();
    it->result->setText(QStringLiteral("<span style='color:#8A96A3'>%1</span>").arg(tr("Відправлено, очікується відповідь…")));
    it->result->show();
    updateState();
    emit sendRequested(r);               // MainWindow: seq = core->sendCommand(r); onCommandSent(seq)
}

void DroneCommandsWindow::onCommandSent(quint32 seq)
{
    m_sending = false;
    m_pendingSeq = seq;
    // итог мог прийти раньше номера (заглушка ядра — в потоке интерфейса)
    const QVector<CommandOutcome> early = std::exchange(m_early, {});
    for (const CommandOutcome &o : early)
        if (o.request.seq == seq) {
            onCommandFinished(o);
            return;
        }
    updateState();
}

void DroneCommandsWindow::onCommandFinished(const CommandOutcome &o)
{
    if (m_pendingId == 0 || o.request.id != m_pendingId)
        return;
    if (m_sending) {
        m_early.append(o);
        return;
    }
    if (o.request.seq != m_pendingSeq)
        return;

    auto it = m_rows.find(m_pendingId);
    m_pendingId = 0;
    m_pendingSeq = 0;
    if (it != m_rows.end())
        showResult(*it, o);
    updateState();
}

void DroneCommandsWindow::showResult(Row &r, const CommandOutcome &o)
{
    const QString time = (o.finishTime.isValid() ? o.finishTime : QDateTime::currentDateTime())
                             .toLocalTime().toString("HH:mm:ss");
    QString text = CoreTexts::outcome(o);
    if (o.result == CommandResult::NoAnswer)
        text += tr(" — можна повторити через 20 с");
    const char *color = CoreTexts::isSuccess(o) ? "#2E9E6A" : "#E5484D";
    r.result->setText(QStringLiteral("<span style='color:%1'>%2 · %3</span>")
                          .arg(QLatin1String(color), time, text.toHtmlEscaped()));
    r.result->show();
}

void DroneCommandsWindow::onAllowedCommands(const QList<quint8> &ids)
{
    m_allowed = ids;
    updateState();
}

void DroneCommandsWindow::onLinkStatus(const LinkStatus &status)
{
    const bool changed = status.state != m_link.state || status.commandPending != m_link.commandPending;
    m_link = status;
    if (changed)
        updateState();
}

void DroneCommandsWindow::onLoggerStatus(const LoggerStatus &status)
{
    const bool changed = status.mode != m_logger.mode || status.radioSilence != m_logger.radioSilence;
    m_logger = status;
    if (changed)
        updateState();
}

void DroneCommandsWindow::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    updateState();
}

void DroneCommandsWindow::updateState()
{
    const bool busy = m_link.commandPending || m_pendingId != 0;
    for (Row &r : m_rows) {
        const bool allowed = m_allowed.contains(r.id);
        const bool on = allowed && !busy && !m_readOnly && m_link.state != LinkState::Closed;
        r.button->setEnabled(on);
        if (r.value)
            r.value->setEnabled(on);
        r.button->setToolTip(on ? QString()
                             : m_readOnly ? tr("Відкрито минулий політ")
                             : m_link.state == LinkState::Closed ? tr("Радіомодем не підключено")
                             : busy ? tr("Очікується відповідь на попередню команду")
                                    : tr("Недоступна в поточному режимі логера"));
    }

    // Строка состояния сверху
    QString text;
    if (m_readOnly)
        text = tr("Відкрито минулий політ — команди недоступні");
    else if (m_link.state == LinkState::Closed)
        text = tr("Радіомодем не підключено (Налаштування → Радіомодем)");
    else {
        text = tr("Режим логера: <b>%1</b>").arg(modeText(m_logger.mode));
        if (m_logger.radioSilence)
            text += tr(" · радіомовчання");
        if (busy)
            text += tr(" · очікується відповідь на команду");
    }
    m_state->setText(text);
}
