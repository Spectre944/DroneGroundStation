#include "dronecommandswindow.h"
#include <QCoreApplication>

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

#include "ui/components/button/toggleswitch.h"
#include "ui/modules/coreTexts/coretexts.h"

namespace {

QString modeText(LoggerMode m)
{
    switch (m) {
    case LoggerMode::Test:   return QCoreApplication::translate("DroneCommandsWindow", "перевірка");
    case LoggerMode::Expect: return QCoreApplication::translate("DroneCommandsWindow", "очікування (політ до області)");
    case LoggerMode::Work:   return QCoreApplication::translate("DroneCommandsWindow", "розвідка");
    case LoggerMode::Return: return QCoreApplication::translate("DroneCommandsWindow", "повернення");
    case LoggerMode::Error:  return QCoreApplication::translate("DroneCommandsWindow", "аварія");
    default:                 return QCoreApplication::translate("DroneCommandsWindow", "невідомо");
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

QLabel *titleLabel(const QString &title, const QString &ids, QWidget *parent)
{
    return new QLabel(QStringLiteral("%1  <span style='color:#8A96A3'>%2</span>").arg(title.toHtmlEscaped(), ids), parent);
}

QLabel *resultLabel(QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setWordWrap(true);
    l->hide();
    return l;
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

    auto makeGroup = [this](const QString &title) {
        auto *box = new QGroupBox(title, this);
        auto *grid = new QGridLayout(box);
        grid->setColumnStretch(0, 1);
        grid->setHorizontalSpacing(10);
        return std::make_pair(box, grid);
    };

    // --- Спектр і радіомовчання: переключатели сверху ---
    {
        auto [box, grid] = makeGroup(tr("Спектр і радіомовчання"));
        m_spectrum.onId = 9;
        m_spectrum.onValue = 3;
        m_spectrum.offId = 12;
        addToggleRow(grid, 0, m_spectrum, tr("Набір спектра"), QStringLiteral("ID09=3 / ID12"));

        QSpinBox *s = spin(0, 65535, 60, tr(" с"), box);
        s->setSpecialValueText(tr("до ID12"));
        addButtonRow(grid, 1, 11, tr("Час набору спектра"), s, tr("Задати"));

        m_silence.onId = 13;
        m_silence.offId = 14;
        addToggleRow(grid, 2, m_silence, tr("Радіомовчання"), QStringLiteral("ID13 / ID14"));
        root->addWidget(box);
    }

    // --- Параметри розвідки ---
    {
        auto [box, grid] = makeGroup(tr("Параметри розвідки"));
        addButtonRow(grid, 0, 5, tr("Коефіцієнт дублювання"), spin(2, 5, 2, QString(), box), tr("Задати"));
        addButtonRow(grid, 1, 6, tr("Інтервал вимірювань"), spin(1, 65535, 60, tr(" с"), box), tr("Задати"));
        addButtonRow(grid, 2, 7, tr("Відстань між вимірюваннями"), spin(1, 65535, 100, tr(" м"), box), tr("Задати"));
        auto *c = new QComboBox(box);
        c->addItems({tr("За командою оператора"), tr("За часом"), tr("За відстанню")});   // 3 — переключатель спектра
        c->setCurrentIndex(2);
        addButtonRow(grid, 3, 9, tr("Режим розвідки"), c, tr("Задати"));
        root->addWidget(box);
    }

    // --- Вимірювання ---
    {
        auto [box, grid] = makeGroup(tr("Вимірювання"));
        addButtonRow(grid, 0, 10, tr("Виміряти зараз"), nullptr, tr("Виміряти"));
        root->addWidget(box);
    }

    // --- Завершення ---
    {
        auto [box, grid] = makeGroup(tr("Завершення"));
        addButtonRow(grid, 0, 8, tr("Закінчити розвідку"), nullptr, tr("Закінчити"));
        root->addWidget(box);
    }
    root->addStretch();
}

void DroneCommandsWindow::addButtonRow(QGridLayout *grid, int row, quint8 id, const QString &title,
                                       QWidget *value, const QString &buttonText)
{
    Row r;
    r.id = id;
    r.value = value;

    QWidget *parent = grid->parentWidget();
    grid->addWidget(titleLabel(title, QStringLiteral("ID%1").arg(id, 2, 10, QLatin1Char('0')), parent), row * 2, 0);
    if (value)
        grid->addWidget(value, row * 2, 1);

    r.button = new QPushButton(buttonText, parent);
    r.button->setMinimumWidth(96);
    grid->addWidget(r.button, row * 2, 2);

    r.result = resultLabel(parent);
    grid->addWidget(r.result, row * 2 + 1, 0, 1, 3);

    connect(r.button, &QPushButton::clicked, this, [this, id] {
        const Row &rr = m_rows[id];
        send(id, valueOf(rr), rr.result);
    });
    m_rows.insert(id, r);
}

void DroneCommandsWindow::addToggleRow(QGridLayout *grid, int row, Toggle &t, const QString &title, const QString &ids)
{
    QWidget *parent = grid->parentWidget();
    grid->addWidget(titleLabel(title, ids, parent), row * 2, 0);

    t.sw = new ToggleSwitch(parent);
    grid->addWidget(t.sw, row * 2, 1, 1, 2, Qt::AlignRight);
    t.result = resultLabel(parent);
    grid->addWidget(t.result, row * 2 + 1, 0, 1, 3);

    Toggle *tp = &t;
    connect(t.sw, &ToggleSwitch::toggleRequested, this, [this, tp](bool wantOn) {
        if (wantOn)
            send(tp->onId, tp->onValue, tp->result, tp->sw);
        else
            send(tp->offId, 0, tp->result, tp->sw);
    });
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
        text = tr("Дрон перестане передавати дані й відповідати на команди до виходу з радіомовчання (ID14).\n\nУвімкнути радіомовчання?");
    else if (id == 9 && value == 3)
        text = tr("Для набору спектра дрон має зависнути над точкою.\n\nПочати набір спектра?");
    else if (id == 12 && m_logger.spectrumStage == SpectrumStage::Collecting)
        text = tr("Набір спектра буде зупинено. Продовжити?");
    if (text.isEmpty())
        return true;
    return QMessageBox::question(this, windowTitle(), text) == QMessageBox::Yes;
}

QString DroneCommandsWindow::blockReason(quint8 id, bool busy) const
{
    if (m_readOnly)
        return tr("Відкрито минулий політ");
    if (m_link.state == LinkState::Closed)
        return tr("Радіомодем не підключено");
    if (busy)
        return tr("Очікується відповідь на попередню команду");
    if (!m_allowed.contains(id))
        return tr("Недоступна в поточному режимі логера");
    return QString();
}

void DroneCommandsWindow::send(quint8 id, int value, QLabel *result, ToggleSwitch *sw)
{
    if (!blockReason(id, m_link.commandPending || m_pendingId != 0).isEmpty())
        return;
    if (!confirm(id, value))
        return;
    // за время подтверждения всё могло поменяться
    if (!blockReason(id, m_link.commandPending || m_pendingId != 0).isEmpty())
        return;

    CommandRequest r;
    r.id = id;
    r.value = value;

    m_pendingId = id;
    m_pendingSeq = 0;
    m_pendingResult = result;
    m_pendingSwitch = sw;
    m_sending = true;
    m_early.clear();
    if (sw)
        sw->setPending(true);
    result->setText(QStringLiteral("<span style='color:#8A96A3'>%1</span>").arg(tr("Відправлено, очікується відповідь…")));
    result->show();
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

    QLabel *result = m_pendingResult;
    ToggleSwitch *sw = m_pendingSwitch;
    m_pendingId = 0;
    m_pendingSeq = 0;
    m_pendingResult = nullptr;
    m_pendingSwitch = nullptr;
    if (sw)
        sw->setPending(false);           // положение переключателя придёт с LoggerStatus
    if (result)
        showResult(result, o);
    updateState();
}

void DroneCommandsWindow::showResult(QLabel *result, const CommandOutcome &o)
{
    const QString time = (o.finishTime.isValid() ? o.finishTime : QDateTime::currentDateTime())
                             .toLocalTime().toString("HH:mm:ss");
    QString text = CoreTexts::outcome(o);
    if (o.result == CommandResult::NoAnswer)
        text += tr(" — можна повторити через 20 с");
    const char *color = CoreTexts::isSuccess(o) ? "#2E9E6A" : "#E5484D";
    result->setText(QStringLiteral("<span style='color:%1'>%2 · ID%3 · %4</span>")
                        .arg(QLatin1String(color), time)
                        .arg(o.request.id, 2, 10, QLatin1Char('0'))
                        .arg(text.toHtmlEscaped()));
    result->show();
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
    m_logger = status;
    // Переключатели показывают фактическое состояние логгера
    m_spectrum.sw->setOn(status.spectrumStage == SpectrumStage::Preparing
                         || status.spectrumStage == SpectrumStage::Collecting);
    m_silence.sw->setOn(status.radioSilence);
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
        const QString reason = blockReason(r.id, busy);
        r.button->setEnabled(reason.isEmpty());
        if (r.value)
            r.value->setEnabled(reason.isEmpty());
        r.button->setToolTip(reason);
    }
    for (Toggle *t : {&m_spectrum, &m_silence}) {
        // нужна та команда, что переведёт в другое положение
        const quint8 next = t->sw->isOn() ? t->offId : t->onId;
        const QString reason = blockReason(next, busy && t->sw != m_pendingSwitch);
        t->sw->setEnabled(reason.isEmpty() && !t->sw->isPending());
        t->sw->setToolTip(reason.isEmpty() ? QStringLiteral("ID%1").arg(next, 2, 10, QLatin1Char('0')) : reason);
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
