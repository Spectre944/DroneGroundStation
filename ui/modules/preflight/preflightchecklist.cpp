#include "preflightchecklist.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor, renderSvg

namespace {

const QString kIco = QStringLiteral(":/icons/svg/bs5/%1.svg");

constexpr LoggerDevice kOrder[] = {     // порядок проверки в логгере
    LoggerDevice::Gps, LoggerDevice::Lidar, LoggerDevice::Bdbg, LoggerDevice::Battery, LoggerDevice::Sd
};

QColor withAlpha(QColor c, int a) { c.setAlpha(a); return c; }

} // namespace


PreflightChecklist::PreflightChecklist(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(tr("Передпольотна перевірка"));
    resize(460, 640);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    // ---- шапка ----
    auto *head = new QHBoxLayout();
    auto *title = new QLabel(tr("Передпольотна перевірка"), this);
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() * 1.2);
    title->setFont(tf);
    m_counter = new QLabel(this);
    head->addWidget(title);
    head->addStretch(1);
    head->addWidget(m_counter);
    lay->addLayout(head);

    // ---- приборы ----
    const QString names[] = {tr("GPS"), tr("LIDAR"), tr("БДБГ-15"), tr("Батарея"), tr("SD-карта")};
    for (int i = 0; i < int(LoggerDevice::Count); ++i) {
        m_cards[int(kOrder[i])] = makeCard(names[i]);
        lay->addWidget(m_cards[int(kOrder[i])].frame);
    }

    // Выбор при ошибке SD — внутри карточки SD
    m_sdActions = new QFrame(this);
    auto *sdLay = new QHBoxLayout(m_sdActions);
    sdLay->setContentsMargins(0, 4, 0, 0);
    m_btnNoSd = new QPushButton(tr("Продовжити без SD"), m_sdActions);
    m_btnStop = new QPushButton(tr("Не продовжувати"), m_sdActions);
    m_btnNoSd->setToolTip(tr("ID02 — логер працюватиме без запису на SD"));
    m_btnStop->setToolTip(tr("ID03 — логер залишиться в режимі перевірки"));
    sdLay->addWidget(m_btnNoSd);
    sdLay->addWidget(m_btnStop);
    sdLay->addStretch(1);
    static_cast<QGridLayout *>(m_cards[int(LoggerDevice::Sd)].frame->layout())->addWidget(m_sdActions, 2, 1, 1, 2);

    lay->addStretch(1);

    // ---- запуск ----
    m_btnCheck = new QPushButton(this);
    m_btnCheck->setMinimumHeight(40);
    m_btnCheck->setIcon(QIcon(IndicatorItem::renderSvg(kIco.arg(QStringLiteral("arrow-repeat")),
                                                       palette().color(QPalette::ButtonText), 16, devicePixelRatioF())));
    lay->addWidget(m_btnCheck);

    auto *note = new QLabel(tr("Порядок як у логера: GPS → LIDAR → БДБГ → батарея → SD. "
                               "На першій помилці логер зупиняється й чекає повтору перевірки."), this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: %1;").arg(StatusColor::muted().name()));
    lay->addWidget(note);

    connect(m_btnCheck, &QPushButton::clicked, this, [this] { send(1); });
    connect(m_btnNoSd,  &QPushButton::clicked, this, [this] { send(2); });
    connect(m_btnStop,  &QPushButton::clicked, this, [this] { send(3); });

    refresh();
}


/////////////////////////////////// Слоты DroneCore ///////////////////////////////////

void PreflightChecklist::onLoggerStatus(const LoggerStatus &status)
{
    // Новая проверка — старая стартовая точка больше не актуальна
    if (status.checking && !m_status.checking)
        m_hasStart = false;
    m_status = status;
    refresh();
}

void PreflightChecklist::onRoutePoint(const RoutePoint &point)
{
    // Первая точка в режиме проверки — стартовая: оператор сам решает, нет ли спуфинга
    if (point.mode != LoggerMode::Test || m_hasStart)
        return;
    m_start = point;
    m_hasStart = true;
    refresh();
}

void PreflightChecklist::onAllowedCommands(const QList<quint8> &ids)
{
    m_allowed = ids;
    m_allowedKnown = true;
    refresh();
}

void PreflightChecklist::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::PaletteChange)
        refresh();
    QWidget::changeEvent(e);
}


/////////////////////////////////// Карточки ///////////////////////////////////

PreflightChecklist::Card PreflightChecklist::makeCard(const QString &name)
{
    Card c;
    c.frame = new QFrame(this);
    c.frame->setObjectName(QStringLiteral("PreflightCard"));

    auto *g = new QGridLayout(c.frame);
    g->setContentsMargins(12, 10, 12, 10);
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(4);

    c.icon = new QLabel(c.frame);
    c.icon->setFixedSize(24, 24);
    c.name = new QLabel(name, c.frame);
    QFont nf = c.name->font();
    nf.setBold(true);
    c.name->setFont(nf);
    c.state = new QLabel(c.frame);
    c.state->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    c.detail = new QLabel(c.frame);
    c.detail->setWordWrap(true);
    c.detail->setTextInteractionFlags(Qt::TextSelectableByMouse);   // координаты можно скопировать

    g->addWidget(c.icon, 0, 0, 2, 1, Qt::AlignTop);
    g->addWidget(c.name, 0, 1);
    g->addWidget(c.state, 0, 2);
    g->addWidget(c.detail, 1, 1, 1, 2);
    g->setColumnStretch(1, 1);
    return c;
}

void PreflightChecklist::setCard(LoggerDevice dev, CardState st, const QString &stateText, const QString &detail)
{
    Card &c = m_cards[int(dev)];

    QString icon;
    QColor color;
    switch (st) {
    case CardState::NotChecked: icon = QStringLiteral("dash-circle");       color = StatusColor::muted();   break;
    case CardState::Running:    icon = QStringLiteral("hourglass-split");   color = StatusColor::info();    break;
    case CardState::Ok:         icon = QStringLiteral("check-circle-fill"); color = StatusColor::ok();      break;
    case CardState::Warn:       icon = QStringLiteral("exclamation-triangle-fill"); color = StatusColor::warning(); break;
    case CardState::Fail:       icon = QStringLiteral("x-circle-fill");     color = StatusColor::alarm();   break;
    }
    c.icon->setPixmap(IndicatorItem::renderSvg(kIco.arg(icon), color, 22, devicePixelRatioF()));

    c.state->setText(stateText);
    // Цвет — стилем надписи: у карточки свой styleSheet, и палитру дочерних виджетов Qt не учитывает
    c.state->setStyleSheet(QStringLiteral("color: %1;")
                               .arg((st == CardState::NotChecked ? StatusColor::muted() : color).name()));

    c.detail->setText(detail);
    c.detail->setVisible(!detail.isEmpty());

    // Рамка: ошибка — красная, предупреждение — жёлтая; фон — AlternateBase темы
    const QColor base = palette().color(QPalette::AlternateBase);
    QString border = QStringLiteral("transparent");
    QColor bg = base;
    if (st == CardState::Fail) {
        border = StatusColor::alarm().name();
        const QColor tint = withAlpha(StatusColor::alarm(), 28);
        bg = QColor::fromRgbF(base.redF() * (1 - tint.alphaF()) + tint.redF() * tint.alphaF(),
                              base.greenF() * (1 - tint.alphaF()) + tint.greenF() * tint.alphaF(),
                              base.blueF() * (1 - tint.alphaF()) + tint.blueF() * tint.alphaF());
    } else if (st == CardState::Warn) {
        border = StatusColor::warning().name();
    }
    c.frame->setStyleSheet(QStringLiteral("QFrame#PreflightCard { background: %1; border: 1px solid %2; border-radius: 8px; }")
                               .arg(bg.name(), border));
}

void PreflightChecklist::refresh()
{
    const LoggerStatus &s = m_status;
    const QLocale loc;
    bool runningShown = false;
    bool anyResult = false;
    int okCount = 0;

    for (LoggerDevice dev : kOrder) {
        const DeviceState ds = s.devices.value(int(dev));
        CardState st = CardState::NotChecked;
        QString stateText = tr("не перевірявся");
        QString detail;

        if (ds.check == DeviceCheck::Ok) {
            st = CardState::Ok;
            stateText = tr("справний");
            ++okCount;
            anyResult = true;
        } else if (ds.check == DeviceCheck::Fail) {
            st = CardState::Fail;
            stateText = failText(dev, ds.msgId);
            anyResult = true;
        } else if (s.checking && !runningShown) {
            st = CardState::Running;
            stateText = tr("перевіряється…");
            runningShown = true;
        }

        switch (dev) {
        case LoggerDevice::Gps:
            if (m_hasStart) {
                const qint64 drift = m_start.time.secsTo(m_start.rxTime);
                detail = QStringLiteral("%1 %2 · %3 UTC")
                             .arg(m_start.lat, 0, 'f', 5).arg(m_start.lon, 0, 'f', 5)
                             .arg(m_start.time.toUTC().toString(QStringLiteral("HH:mm:ss")));
                if (qAbs(drift) >= 2)
                    detail += QLatin1Char('\n') + (drift > 0 ? tr("Годинник логера відстає від ПК на %1 с")
                                                             : tr("Годинник логера випереджає ПК на %1 с")).arg(qAbs(drift));
                detail += QLatin1Char('\n') + tr("Перевірте, що точка поруч із вами — інакше можливий спуфінг.");
            }
            break;
        case LoggerDevice::Battery:
            if (s.hasBattV && ds.check != DeviceCheck::NotChecked)
                stateText += QStringLiteral(" · ") + tr("%1 В").arg(loc.toString(s.battV, 'f', 1));
            break;
        case LoggerDevice::Sd:
            if (s.noSd) {
                st = CardState::Warn;
                stateText = tr("робота без SD");
                detail = tr("Звірка після польоту неможлива, дані під час радіомовчання втрачаються.");
            } else if (ds.check == DeviceCheck::Fail) {
                detail = tr("Без SD дані будуть лише в ефірі: підсумкові спектри й пакети під час "
                            "радіомовчання буде втрачено.");
            }
            break;
        default:
            break;
        }
        setCard(dev, st, stateText, detail);
    }

    m_counter->setText(tr("%1 з %2").arg(okCount).arg(int(LoggerDevice::Count)));

    // ---- кнопки ----
    const auto allowed = [this](quint8 id, bool byMode) {
        return m_allowedKnown ? m_allowed.contains(id) : byMode;
    };
    const bool inTest = s.mode == LoggerMode::Test || s.mode == LoggerMode::Unknown;

    const bool sdChoice = s.devices.value(int(LoggerDevice::Sd)).check == DeviceCheck::Fail && !s.noSd && inTest;
    m_sdActions->setVisible(sdChoice);
    m_btnNoSd->setEnabled(allowed(2, sdChoice));
    m_btnStop->setEnabled(allowed(3, sdChoice));

    if (s.checking) {
        m_btnCheck->setText(tr("Іде перевірка…"));
        m_btnCheck->setEnabled(false);
    } else {
        m_btnCheck->setText(anyResult ? tr("Повторити перевірку") : tr("Почати перевірку"));
        m_btnCheck->setEnabled(allowed(1, inTest));
    }
    m_btnCheck->setToolTip(s.mode == LoggerMode::Error
                               ? tr("Аварія: спочатку вимкніть і ввімкніть живлення дрона")
                               : tr("ID01 — передпольотна перевірка приладів"));
}

QString PreflightChecklist::failText(LoggerDevice dev, quint8 msgId) const
{
    switch (msgId) {
    case 0x04: return tr("розряджений (ID04)");
    case 0x06: return tr("несправний (ID06)");
    case 0x07: return tr("некоректна відповідь (ID07)");
    case 0x08: return tr("не відповідає (ID08)");
    case 0x0C: return tr("несправний (ID12)");
    case 0x0F: return tr("несправний (ID15)");
    case 0x10: return tr("несправна (ID16)");
    case 0x19: return tr("відмова в польоті (ID25)");
    case 0x1A: return tr("помилка годинника (ID26)");
    default:
        Q_UNUSED(dev)
        return msgId ? tr("помилка (ID%1)").arg(msgId) : tr("помилка");
    }
}

void PreflightChecklist::send(quint8 id)
{
    CommandRequest r;
    r.id = id;
    emit commandRequested(r);
}
