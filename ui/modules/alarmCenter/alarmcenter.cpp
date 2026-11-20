#include "alarmcenter.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor, renderSvg

namespace {

const QString kIco = QStringLiteral(":/icons/svg/bs5/%1.svg");
constexpr int kMaxAlarms = 500;

QColor mix(const QColor &a, const QColor &b, double k)
{
    return QColor::fromRgbF(a.redF() * (1 - k) + b.redF() * k,
                            a.greenF() * (1 - k) + b.greenF() * k,
                            a.blueF() * (1 - k) + b.blueF() * k);
}

} // namespace


AlarmCenter::AlarmCenter(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("AlarmCenter"));
    setFocusPolicy(Qt::StrongFocus);    // Esc закрывает
    setStyleSheet(QStringLiteral("QFrame#AlarmCenter { background: palette(window); border: 1px solid palette(mid); border-radius: 10px; }"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(8);

    // ---- шапка ----
    auto *head = new QHBoxLayout();
    head->setSpacing(8);
    auto *title = new QLabel(tr("Тривоги"), this);
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() * 1.15);
    title->setFont(tf);

    m_badge = new QLabel(this);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setStyleSheet(QStringLiteral("QLabel { background: %1; color: white; border-radius: 9px; padding: 1px 8px; font-weight: bold; }")
                               .arg(StatusColor::alarm().name()));

    m_ackAll = new QPushButton(tr("Бачу всі"), this);
    m_ackAll->setToolTip(tr("Відмітити всі тривоги як переглянуті"));

    m_close = new QToolButton(this);
    m_close->setAutoRaise(true);
    m_close->setToolTip(tr("Закрити (Esc)"));
    m_close->setAccessibleName(tr("Закрити"));

    head->addWidget(title);
    head->addWidget(m_badge);
    head->addStretch(1);
    head->addWidget(m_ackAll);
    head->addWidget(m_close);
    lay->addLayout(head);

    // ---- список ----
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list = new QWidget(m_scroll);
    m_listLay = new QVBoxLayout(m_list);
    m_listLay->setContentsMargins(0, 0, 0, 0);
    m_listLay->setSpacing(6);
    m_scroll->setWidget(m_list);
    lay->addWidget(m_scroll, 1);

    m_empty = new QLabel(tr("Тривог немає"), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setStyleSheet(QStringLiteral("color: %1;").arg(StatusColor::muted().name()));
    lay->addWidget(m_empty, 1);

    connect(m_ackAll, &QPushButton::clicked, this, &AlarmCenter::acknowledgeAll);
    connect(m_close, &QToolButton::clicked, this, &AlarmCenter::closeRequested);

    rebuild();
}

int AlarmCenter::unacknowledgedCount() const
{
    return int(std::count_if(m_alarms.cbegin(), m_alarms.cend(), [](const Alarm &a) { return !a.acknowledged; }));
}


/////////////////////////////////// Слоты ///////////////////////////////////

void AlarmCenter::onAlarm(const Alarm &a)
{
    Alarm x = a;
    if (x.id == 0)
        x.id = m_localId--;     // ещё не записана в базу — свой отрицательный номер
    m_alarms.append(x);
    if (m_alarms.size() > kMaxAlarms)
        m_alarms.removeFirst();
    rebuild();
    emit alarmAdded(x);
}

void AlarmCenter::onAlarmAcknowledged(qint64 alarmId)
{
    for (Alarm &a : m_alarms)
        if (a.id == alarmId)
            a.acknowledged = true;
    rebuild();
}

void AlarmCenter::setAlarms(const QVector<Alarm> &alarms)
{
    m_alarms = alarms;
    for (Alarm &a : m_alarms)
        if (a.id == 0)
            a.id = m_localId--;
    rebuild();
}

void AlarmCenter::acknowledgeAll()
{
    for (const Alarm &a : std::as_const(m_alarms))
        if (!a.acknowledged && a.id > 0)
            emit acknowledgeRequested(a.id);
    for (Alarm &a : m_alarms)
        a.acknowledged = true;
    rebuild();
}

void AlarmCenter::clear()
{
    m_alarms.clear();
    rebuild();
}

void AlarmCenter::acknowledge(qint64 id)
{
    if (id > 0)
        emit acknowledgeRequested(id);
    onAlarmAcknowledged(id);
}

void AlarmCenter::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        emit closeRequested();
        return;
    }
    QFrame::keyPressEvent(e);
}

void AlarmCenter::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::PaletteChange)
        rebuild();
    QFrame::changeEvent(e);
}


/////////////////////////////////// Отрисовка ///////////////////////////////////

void AlarmCenter::rebuild()
{
    const qreal dpr = devicePixelRatioF();
    m_close->setIcon(QIcon(IndicatorItem::renderSvg(kIco.arg(QStringLiteral("x-lg")),
                                                    palette().color(QPalette::WindowText), 16, dpr)));

    // Убираем старые строки
    while (QLayoutItem *li = m_listLay->takeAt(0)) {
        delete li->widget();
        delete li;
    }

    // Порядок: непросмотренные, затем просмотренные; внутри — новые сверху
    QVector<Alarm> sorted = m_alarms;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Alarm &a, const Alarm &b) {
        if (a.acknowledged != b.acknowledged)
            return !a.acknowledged;
        return a.time > b.time;
    });
    for (const Alarm &a : std::as_const(sorted))
        m_listLay->addWidget(makeRow(a));
    m_listLay->addStretch(1);

    const int n = unacknowledgedCount();
    m_badge->setText(tr("нових: %1").arg(n));
    m_badge->setVisible(n > 0);
    m_ackAll->setEnabled(n > 0);
    m_scroll->setVisible(!m_alarms.isEmpty());
    m_empty->setVisible(m_alarms.isEmpty());

    if (n != m_lastCount) {
        m_lastCount = n;
        emit unacknowledgedCountChanged(n);
    }
}

QWidget *AlarmCenter::makeRow(const Alarm &a)
{
    const bool isAlarm = a.level == AlarmLevel::Alarm;
    const QColor levelColor = isAlarm ? StatusColor::alarm() : StatusColor::warning();
    const QColor base = palette().color(QPalette::Window);
    const QColor text = palette().color(QPalette::WindowText);
    const qreal dpr = devicePixelRatioF();

    auto *row = new QFrame(m_list);
    row->setObjectName(QStringLiteral("AlarmRow"));
    const QColor bg = a.acknowledged ? base : mix(base, levelColor, 0.14);
    row->setStyleSheet(QStringLiteral("QFrame#AlarmRow { background: %1; border-radius: 8px; }").arg(bg.name()));

    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(10, 8, 8, 8);
    h->setSpacing(10);

    // Маркер: тревога — круг, предупреждение — ромб (различаются формой, не только цветом)
    auto *mark = new QLabel(row);
    mark->setFixedSize(14, 14);
    const QString icon = a.acknowledged ? QStringLiteral("check2")
                                        : (isAlarm ? QStringLiteral("circle-fill") : QStringLiteral("diamond-fill"));
    mark->setPixmap(IndicatorItem::renderSvg(kIco.arg(icon), a.acknowledged ? StatusColor::muted() : levelColor, 14, dpr));
    h->addWidget(mark, 0, Qt::AlignTop);

    auto *texts = new QVBoxLayout();
    texts->setSpacing(2);
    auto *ttl = new QLabel(alarmText(a), row);
    ttl->setWordWrap(true);
    QFont f = ttl->font();
    f.setBold(!a.acknowledged);
    ttl->setFont(f);
    // Цвета — стилем надписи: у AlarmCenter свой styleSheet, палитру дочерних виджетов Qt не учитывает
    ttl->setStyleSheet(QStringLiteral("color: %1;").arg((a.acknowledged ? StatusColor::muted() : text).name()));

    QString sub = a.time.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    if (a.hasPos)
        sub += QStringLiteral(" · %1 %2").arg(a.lat, 0, 'f', 5).arg(a.lon, 0, 'f', 5);
    if (a.acknowledged)
        sub += tr(" · переглянуто");
    auto *meta = new QLabel(sub, row);
    meta->setStyleSheet(QStringLiteral("color: %1;").arg(StatusColor::muted().name()));
    QFont mf = meta->font();
    mf.setPointSizeF(mf.pointSizeF() * 0.9);
    meta->setFont(mf);

    texts->addWidget(ttl);
    texts->addWidget(meta);
    h->addLayout(texts, 1);

    auto *btns = new QVBoxLayout();
    btns->setSpacing(4);
    if (!a.acknowledged) {
        auto *ack = new QPushButton(tr("Бачу"), row);
        const qint64 id = a.id;
        connect(ack, &QPushButton::clicked, this, [this, id] { acknowledge(id); });
        btns->addWidget(ack);
    }
    if (a.hasPos) {
        auto *map = new QPushButton(tr("На карті"), row);
        map->setFlat(true);
        const double lat = a.lat, lon = a.lon;
        connect(map, &QPushButton::clicked, this, [this, lat, lon] { emit showOnMapRequested(lat, lon); });
        btns->addWidget(map);
    }
    btns->addStretch(1);
    h->addLayout(btns);
    return row;
}

QString AlarmCenter::alarmText(const Alarm &a)
{
    const QLocale loc;
    switch (a.type) {
    case AlarmType::DoseRate:
        return (a.level == AlarmLevel::Alarm ? tr("ПАЕД %1 мкЗв/год — вище порогу тривоги")
                                             : tr("ПАЕД %1 мкЗв/год — вище порогу попередження"))
            .arg(loc.toString(a.value, 'f', 2));
    case AlarmType::LoggerError:
        switch (a.msgId) {
        case 0x18: return tr("Аварія: спуфінг GPS (ID24)");
        case 0x19: return tr("Аварія: відмова приладу (ID25)");
        case 0x1A: return tr("Аварія: помилка годинника (ID26)");
        case 0x1C: return tr("Аварія: не вдалося запустити спектр (ID28)");
        default:   return tr("Аварія дрона (ID%1)").arg(a.msgId);
        }
    case AlarmType::SdError:      return tr("SD-карта несправна (ID16)");
    case AlarmType::LinkLost:     return tr("Зв'язок втрачено");
    case AlarmType::ModeMismatch: return tr("Режим логера не збігається з обчисленим");
    }
    return QString();
}
