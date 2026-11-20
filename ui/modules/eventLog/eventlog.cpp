#include "eventlog.h"

#include <QCheckBox>
#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor

EventLog::EventLog(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    auto *bar = new QHBoxLayout();
    bar->setContentsMargins(0, 0, 0, 0);
    m_debugBox = new QCheckBox(tr("Налагодження"), this);
    m_debugBox->setToolTip(tr("Показувати службові повідомлення логера ID70–ID84"));
    auto *clearBtn = new QPushButton(tr("Очистити"), this);
    clearBtn->setToolTip(tr("Очистити вікно. Події в базі залишаються."));
    bar->addWidget(m_debugBox);
    bar->addStretch(1);
    bar->addWidget(clearBtn);
    lay->addLayout(bar);

    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setUndoRedoEnabled(false);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setMaximumBlockCount(m_max);
    m_view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    lay->addWidget(m_view, 1);

    connect(m_debugBox, &QCheckBox::toggled, this, &EventLog::setShowDebug);
    connect(clearBtn, &QPushButton::clicked, this, &EventLog::clear);

    updateFormats();
}

void EventLog::setShowDebug(bool on)
{
    if (m_showDebug == on)
        return;
    m_showDebug = on;
    if (m_debugBox->isChecked() != on)
        m_debugBox->setChecked(on);
    rerender();
}

void EventLog::setUtc(bool on)
{
    if (m_utc == on)
        return;
    m_utc = on;
    rerender();
}

void EventLog::setMaxEntries(int n)
{
    m_max = qMax(100, n);
    m_view->setMaximumBlockCount(m_max);
    if (m_entries.size() > m_max)
        m_entries.remove(0, m_entries.size() - m_max);
}


/////////////////////////////////// Добавление строк ///////////////////////////////////

void EventLog::append(Level level, const QString &text, const QDateTime &time)
{
    Entry e{time.isValid() ? time : QDateTime::currentDateTimeUtc(), level, text};
    m_entries.append(e);
    if (m_entries.size() > m_max)
        m_entries.removeFirst();
    render(e);
}

void EventLog::onEvent(const CoreEvent &e)
{
    Level level = Level::Info;
    const QString text = eventText(e, &level);
    append(e.debug ? Level::Debug : level, text, e.time);
}

void EventLog::onCommandFinished(const CommandOutcome &o)
{
    Level level = Level::Cmd;
    const QString text = commandText(o, &level);
    append(level, text, o.finishTime.isValid() ? o.finishTime : o.sentTime);
}

void EventLog::onAlarm(const Alarm &a)
{
    append(a.level == AlarmLevel::Alarm ? Level::Alert : Level::Warn, alarmText(a), a.time);
}

void EventLog::onCoreError(const QString &source, const QString &text)
{
    const QString src = source == QLatin1String("port") ? tr("Порт")
                      : source == QLatin1String("db")   ? tr("База")
                      : source;
    append(Level::Error, QStringLiteral("%1: %2").arg(src, text));
}

void EventLog::loadSession(const SessionData &data)
{
    // Прошлый полёт: собираем всё в одну ленту и сортируем по времени
    m_entries.clear();
    m_view->clear();

    QVector<Entry> all;
    all.reserve(data.events.size() + data.commands.size() + data.alarms.size());
    for (const CoreEvent &e : data.events) {
        Level l = Level::Info;
        const QString t = eventText(e, &l);
        all.append({e.time, e.debug ? Level::Debug : l, t});
    }
    for (const CommandOutcome &o : data.commands) {
        Level l = Level::Cmd;
        const QString t = commandText(o, &l);
        all.append({o.finishTime.isValid() ? o.finishTime : o.sentTime, l, t});
    }
    for (const Alarm &a : data.alarms)
        all.append({a.time, a.level == AlarmLevel::Alarm ? Level::Alert : Level::Warn, alarmText(a)});

    std::stable_sort(all.begin(), all.end(),
                     [](const Entry &a, const Entry &b) { return a.time < b.time; });
    if (all.size() > m_max)
        all.remove(0, all.size() - m_max);
    m_entries = all;
    rerender();
}

void EventLog::clear()
{
    m_entries.clear();
    m_view->clear();
}

void EventLog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::PaletteChange) {
        updateFormats();
        rerender();
    }
    QWidget::changeEvent(e);
}


/////////////////////////////////// Отрисовка ///////////////////////////////////

void EventLog::updateFormats()
{
    const QPalette pal = m_view->palette();
    const QColor text = pal.color(QPalette::Text);
    const QColor base = pal.color(QPalette::Base);
    const QColor muted((text.red() + base.red()) / 2, (text.green() + base.green()) / 2,
                       (text.blue() + base.blue()) / 2);

    m_fmtText = QTextCharFormat();
    m_fmtText.setForeground(text);

    m_fmtTime = QTextCharFormat();
    m_fmtTime.setForeground(muted);

    const QColor tagColor[6] = {
        StatusColor::ok(),      // Info
        StatusColor::info(),    // Cmd
        StatusColor::warning(), // Warn
        StatusColor::alarm(),   // Alert
        StatusColor::alarm(),   // Error
        muted                   // Debug
    };
    for (int i = 0; i < 6; ++i) {
        m_fmtTag[i] = QTextCharFormat();
        m_fmtTag[i].setForeground(tagColor[i]);
        m_fmtTag[i].setFontWeight(i == int(Level::Debug) ? QFont::Normal : QFont::Bold);
    }
    // ALERT дополнительно на подложке — заметен при быстром просмотре
    QColor bg = StatusColor::alarm();
    bg.setAlpha(40);
    m_fmtTag[int(Level::Alert)].setBackground(bg);
}

QString EventLog::tagFor(Level l) const
{
    static const char *tags[] = {"INFO", "CMD", "WARN", "ALERT", "ERROR", "DEBUG"};
    return QStringLiteral("[%1]").arg(QLatin1String(tags[int(l)]));
}

void EventLog::render(const Entry &e)
{
    if (e.level == Level::Debug && !m_showDebug)
        return;

    QScrollBar *sb = m_view->verticalScrollBar();
    const bool atBottom = sb->value() >= sb->maximum() - 2;

    QTextCursor c(m_view->document());
    c.movePosition(QTextCursor::End);
    if (!m_view->document()->isEmpty())
        c.insertBlock();

    const QDateTime t = m_utc ? e.time.toUTC() : e.time.toLocalTime();
    c.insertText(t.toString(QStringLiteral("HH:mm:ss")) + QLatin1Char(' '), m_fmtTime);
    const QString tag = tagFor(e.level);
    c.insertText(tag, m_fmtTag[int(e.level)]);
    c.insertText(QString(8 - tag.size(), QLatin1Char(' ')) + e.text, m_fmtText);

    if (atBottom)
        sb->setValue(sb->maximum());
}

void EventLog::rerender()
{
    m_view->setUpdatesEnabled(false);
    m_view->clear();
    for (const Entry &e : std::as_const(m_entries))
        render(e);
    m_view->setUpdatesEnabled(true);
    m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
}


/////////////////////////////////// Тексты ///////////////////////////////////

QString EventLog::commandName(quint8 id) const
{
    switch (id) {
    case 1:  return tr("Передпольотна перевірка");
    case 2:  return tr("Продовжити без SD");
    case 3:  return tr("Не продовжувати");
    case 4:  return tr("Область і параметри");
    case 5:  return tr("Коефіцієнт дублювання");
    case 6:  return tr("Інтервал");
    case 7:  return tr("Відстань");
    case 8:  return tr("Закінчити розвідку");
    case 9:  return tr("Режим розвідки");
    case 10: return tr("Виміряти");
    case 11: return tr("Час спектра");
    case 12: return tr("Зупинити спектр");
    case 13: return tr("Радіомовчання: увімк.");
    case 14: return tr("Радіомовчання: вимк.");
    default: return tr("Невідома");
    }
}

QString EventLog::commandText(const CommandOutcome &o, Level *level) const
{
    const QString cmd = QStringLiteral("ID%1 «%2»%3")
                            .arg(o.request.id, 2, 10, QLatin1Char('0'))
                            .arg(commandName(o.request.id), commandValue(o.request));
    *level = Level::Cmd;
    QString res;
    switch (o.result) {
    case CommandResult::Done:      res = tr("виконано"); break;
    case CommandResult::BadPacket: res = tr("помилка пакета (ID02)"); *level = Level::Warn; break;
    case CommandResult::Forbidden: res = tr("не виконується в цьому режимі (ID03)"); *level = Level::Warn; break;
    case CommandResult::NoAnswer:  res = tr("немає відповіді, спроб: %1").arg(o.attempts); *level = Level::Warn; break;
    case CommandResult::Rejected:  res = tr("не надіслано: %1").arg(rejectText(o.reason)); *level = Level::Warn; break;
    }
    return QStringLiteral("%1 → %2").arg(cmd, res);
}

QString EventLog::commandValue(const CommandRequest &r) const
{
    switch (r.id) {
    case 4:  return tr(" · %1 вершин").arg(r.vertices.size());
    case 5:  return QStringLiteral(" = %1").arg(r.value);
    case 6:  return tr(" = %1 с").arg(r.value);
    case 7:  return tr(" = %1 м").arg(r.value);
    case 9: {
        static const char *modes[] = {QT_TR_NOOP("оператор"), QT_TR_NOOP("час"),
                                      QT_TR_NOOP("відстань"), QT_TR_NOOP("спектр")};
        return r.value >= 0 && r.value <= 3 ? QStringLiteral(" = ") + tr(modes[r.value]) : QString();
    }
    case 11: return r.value == 0 ? tr(" = до ID12") : tr(" = %1 с").arg(r.value);
    default: return QString();
    }
}

QString EventLog::rejectText(RejectReason r) const
{
    switch (r) {
    case RejectReason::None:         return QString();
    case RejectReason::PortClosed:   return tr("порт закритий");
    case RejectReason::Busy:         return tr("очікуємо відповідь на попередню команду");
    case RejectReason::NotAllowed:   return tr("заборонена в поточному режимі");
    case RejectReason::ReadOnly:     return tr("відкрито минулий політ");
    case RejectReason::BadId:        return tr("невірний ID");
    case RejectReason::AreaNotValid: return tr("область не пройшла перевірку");
    case RejectReason::BadReconMode: return tr("невірний режим");
    case RejectReason::BadInterval:  return tr("інтервал має бути більше 0");
    case RejectReason::BadDistance:  return tr("відстань має бути більше 0");
    case RejectReason::BadDupFactor: return tr("коефіцієнт має бути 2–5");
    case RejectReason::BadValue:     return tr("недопустиме значення");
    }
    return QString();
}

QString EventLog::eventText(const CoreEvent &e, Level *level) const
{
    const QVariantMap &p = e.params;
    const auto volts = [&p] { return QLocale().toString(p.value(QStringLiteral("voltage")).toDouble(), 'f', 1); };
    const auto devName = [this](int d) {
        switch (d) {
        case 0:  return tr("БДБГ");
        case 1:  return tr("LIDAR");
        case 2:  return tr("GPS");
        case 3:  return tr("акумулятор");
        default: return tr("прилад %1").arg(d);
        }
    };

    *level = Level::Info;
    switch (e.code) {
    case EventCode::BatteryLow:     *level = Level::Warn;  return tr("Акумулятор розряджений: %1 В").arg(volts());
    case EventCode::BatteryOk:                             return tr("Акумулятор у нормі: %1 В").arg(volts());
    case EventCode::BatteryFault:   *level = Level::Warn;  return tr("Акумулятор несправний");
    case EventCode::BdbgBadAnswer:  *level = Level::Warn;  return tr("БДБГ відповів некоректно");
    case EventCode::BdbgNoAnswer:   *level = Level::Warn;  return tr("БДБГ не відповів");
    case EventCode::BdbgOk:                                return tr("БДБГ перевірено");
    case EventCode::LidarOk:                               return tr("LIDAR перевірено");
    case EventCode::LidarFault:     *level = Level::Warn;  return tr("LIDAR несправний");
    case EventCode::GpsFault:       *level = Level::Warn;  return tr("GPS несправний");
    case EventCode::SdFault:        *level = Level::Warn;  return tr("SD-карта несправна — продовжити без SD або зупинити");
    case EventCode::CheckPassed:                           return tr("Перевірку пройдено — режим «Очікування»");
    case EventCode::PointAlreadyMeasured:                  return tr("У цій точці вже виміряно — вимір пропущено");
    case EventCode::Spoofing:       *level = Level::Alert; return tr("Спуфінг GPS — аварія");
    case EventCode::DeviceFailed:   *level = Level::Alert;
        return tr("Відмова приладу: %1 — аварія").arg(devName(p.value(QStringLiteral("device")).toInt()));
    case EventCode::ClockError:     *level = Level::Alert; return tr("Помилка годинника — аварія");
    case EventCode::SpectrumStartFailed: *level = Level::Alert; return tr("Не вдалося запустити набір спектра — аварія");
    case EventCode::SelectReconMode: *level = Level::Warn; return tr("Логер просить обрати режим розвідки (ID09)");
    case EventCode::DebugState:     *level = Level::Debug;
        return QStringLiteral("ID%1").arg(p.value(QStringLiteral("msgId")).toInt());
    case EventCode::UnknownId:      *level = Level::Warn;
        return tr("Невідомий ID повідомлення: %1").arg(p.value(QStringLiteral("msgId")).toInt());
    case EventCode::BadLength:      *level = Level::Warn;
        return tr("Невірна довжина ID%1: %2 байт").arg(p.value(QStringLiteral("msgId")).toInt())
                                                  .arg(p.value(QStringLiteral("length")).toInt());
    case EventCode::PortOpened:     return tr("Порт %1 відкрито").arg(p.value(QStringLiteral("port")).toString());
    case EventCode::PortClosed:     return tr("Порт закрито");
    case EventCode::PortError:      *level = Level::Error;
        return tr("Помилка порту: %1").arg(p.value(QStringLiteral("text")).toString());
    case EventCode::LinkLost:       *level = Level::Alert; return tr("Зв'язок втрачено");
    case EventCode::LinkRestored:                          return tr("Зв'язок відновлено");
    }
    *level = Level::Warn;
    return tr("Подія з кодом 0x%1").arg(int(e.code), 0, 16);
}

QString EventLog::alarmText(const Alarm &a) const
{
    switch (a.type) {
    case AlarmType::DoseRate:
        return (a.level == AlarmLevel::Alarm ? tr("Тривога: ПАЕД %1 мкЗв/год вище порогу")
                                             : tr("Попередження: ПАЕД %1 мкЗв/год вище порогу"))
            .arg(QLocale().toString(a.value, 'f', 2));
    case AlarmType::LoggerError:  return tr("Аварія дрона (ID%1)").arg(a.msgId);
    case AlarmType::SdError:      return tr("Помилка SD-карти");
    case AlarmType::LinkLost:     return tr("Тривога: зв'язок втрачено");
    case AlarmType::ModeMismatch: return tr("Режим логера не збігається з обчисленим");
    }
    return QString();
}
