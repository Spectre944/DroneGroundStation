#include "indicatorpanel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLocale>

namespace {

const QString kIco = QStringLiteral(":/icons/svg/bs5/%1.svg");
inline QString ico(const char *name) { return kIco.arg(QLatin1String(name)); }

/// «2 с», «1 хв 05 с»; -1 — «—».
QString formatAgo(int s)
{
    if (s < 0)
        return QStringLiteral("—");
    if (s < 120)
        return IndicatorPanel::tr("%1 с").arg(s);
    return IndicatorPanel::tr("%1 хв %2 с").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString mark(DeviceCheck c)
{
    switch (c) {
    case DeviceCheck::Ok:   return QStringLiteral("✓");
    case DeviceCheck::Fail: return QStringLiteral("✗");
    default:                return QStringLiteral("—");
    }
}

} // namespace


IndicatorPanel::IndicatorPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("IndicatorPanel"));
    setStyleSheet(QStringLiteral(
        "#IndicatorPanel { background: palette(window); border: 1px solid palette(mid); border-radius: 10px; }"));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(2);

    for (int i = 0; i < int(Item::Count); ++i) {
        m_items[i] = new IndicatorItem(QString(), this);
        connect(m_items[i], &IndicatorItem::clicked, this, [this, i] { emit itemClicked(Item(i)); });

        if (Item(i) == Item::Session) {     // полёт — отдельная группа
            auto *sep = new QFrame(this);
            sep->setFrameShape(QFrame::VLine);
            sep->setFrameShadow(QFrame::Plain);
            sep->setFixedHeight(20);
            lay->addWidget(sep);
        }
        lay->addWidget(m_items[i]);
    }

    buildStates();
    applyAll();
}

void IndicatorPanel::setBatteryThresholds(float lowV, float criticalV)
{
    m_battLowV = lowV;
    m_battCritV = criticalV;
    applyLogger();
}


/////////////////////////////////// Слоты DroneCore ///////////////////////////////////

void IndicatorPanel::onLinkStatus(const LinkStatus &status)
{
    m_link = status;
    applyLink();
}

void IndicatorPanel::onLoggerStatus(const LoggerStatus &status)
{
    m_logger = status;
    applyLogger();
}

void IndicatorPanel::onSessionStarted(const SessionInfo &info)
{
    m_session = info;
    m_sessionState = SessRecording;
    applySession();
}

void IndicatorPanel::onSessionFinished(const SessionInfo &info)
{
    m_session = info;
    m_sessionState = SessNone;
    applySession();
}

void IndicatorPanel::onSessionOpened(const SessionInfo &info)
{
    m_session = info;
    m_sessionState = info.readOnly ? SessViewing : SessRecording;
    applySession();
}

void IndicatorPanel::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        buildStates();
        applyAll();
    }
    QFrame::changeEvent(e);
}


/////////////////////////////////// Таблицы состояний ///////////////////////////////////

void IndicatorPanel::buildStates()
{
    using SC = IndicatorState;
    const QColor none;  // невалидный — цвет текста темы

    item(Item::Link)->setName(tr("Зв'язок"));
    item(Item::Link)->setStates({
        SC{int(LinkState::Closed),  ico("plug"),          StatusColor::muted(),   tr("Порт закритий"),
           tr("Радіомодем не підключено. Оберіть COM-порт і натисніть «Підключити».")},
        SC{int(LinkState::Waiting), ico("hourglass-split"), StatusColor::info(),  tr("Очікування пакетів"),
           tr("Порт відкритий, від логера ще нічого не надійшло.")},
        SC{int(LinkState::Ok),      ico("reception-4"),   StatusColor::ok(),      tr("На зв'язку"),
           tr("Пакети надходять.")},
        SC{int(LinkState::Silent),  ico("reception-1"),   StatusColor::info(),    tr("Очікувана тиша"),
           tr("Пакетів немає, і так має бути — радіомовчання.")},
        SC{int(LinkState::Lost),    ico("wifi-off"),      StatusColor::alarm(),   tr("Зв'язок втрачено"),
           tr("Пакетів немає довше за поріг із налаштувань.")},
    });

    item(Item::Mode)->setName(tr("Режим логера"));
    item(Item::Mode)->setStates({
        SC{int(LoggerMode::Unknown), ico("question-circle"), StatusColor::muted(), tr("Невідомо"),
           tr("Логер ще не повідомив про стан. Після ввімкнення дрона — передпольотна перевірка.")},
        SC{int(LoggerMode::Test),    ico("clipboard-check"), StatusColor::info(),  tr("Перевірка"),
           tr("Передпольотна перевірка приладів (ID01).")},
        SC{int(LoggerMode::Expect),  ico("airplane"),        none,                 tr("Очікування"),
           tr("Політ до області. Логер надсилає координати раз на 60 с.")},
        SC{int(LoggerMode::Work),    ico("radar"),           StatusColor::ok(),    tr("Розвідка"),
           tr("Дрон в області, йдуть вимірювання.")},
        SC{int(LoggerMode::Return),  ico("arrow-return-left"), StatusColor::info(), tr("Повернення"),
           tr("Розвідку закінчено (ID08). Координати раз на 60 с.")},
        SC{int(LoggerMode::Error),   ico("exclamation-octagon-fill"), StatusColor::alarm(), tr("Аварія"),
           tr("Вихід лише вимкненням живлення, далі — нова перевірка й нова область.")},
    });

    item(Item::Recon)->setName(tr("Режим розвідки"));
    item(Item::Recon)->setStates({
        SC{int(ReconMode::Unknown),  ico("question-circle"), StatusColor::muted(), tr("Не задано"),
           tr("Область і параметри ще не надіслано (ID04).")},
        SC{int(ReconMode::Operator), ico("hand-index-thumb"), none, tr("За командою оператора"),
           tr("Вимір лише за командою «Виміряти» (ID10).")},
        SC{int(ReconMode::Time),     ico("clock"),  none, tr("За часом"),
           tr("Вимір через заданий інтервал.")},
        SC{int(ReconMode::Distance), ico("rulers"), none, tr("За відстанню"),
           tr("Вимір через задану відстань.")},
    });

    item(Item::Spectrum)->setName(tr("Спектр"));
    item(Item::Spectrum)->setStates({
        SC{int(SpectrumStage::None),       ico("bar-chart-line"),      StatusColor::muted(),    tr("Немає набору"), QString()},
        SC{int(SpectrumStage::Preparing),  ico("hourglass-split"),     StatusColor::spectrum(), tr("Підготовка"),
           tr("Команду прийнято, дрон має зависнути — чекаємо початку набору (ID27).")},
        SC{int(SpectrumStage::Collecting), ico("bar-chart-line-fill"), StatusColor::spectrum(), tr("Набір"),
           tr("Порції спектра надходять кожні 5–10 с.")},
        SC{int(SpectrumStage::Finished),   ico("bar-chart-line-fill"), StatusColor::ok(),       tr("Набір закінчено"),
           tr("Підсумковий спектр збережено.")},
    });

    item(Item::Radio)->setName(tr("Радіомовчання"));
    item(Item::Radio)->setStates({
        SC{0, ico("broadcast"),        StatusColor::muted(),   tr("Вимкнено"), QString()},
        SC{1, ico("volume-mute-fill"), StatusColor::warning(), tr("Увімкнено"),
           tr("Логер не передає (ID13). Дані пишуться лише на SD. Вимкнути — ID14.")},
    });

    item(Item::Battery)->setName(tr("Батарея логера"));
    item(Item::Battery)->setStates({
        SC{0, ico("battery"),      StatusColor::muted(),   tr("Немає даних"), tr("Напруга ще не надходила.")},
        SC{1, ico("battery-full"), StatusColor::ok(),      tr("Норма"),
           tr("Вище %1 В.").arg(QLocale().toString(m_battLowV, 'f', 1))},
        SC{2, ico("battery-half"), StatusColor::warning(), tr("Низький заряд"),
           tr("Нижче %1 В — плануйте повернення.").arg(QLocale().toString(m_battLowV, 'f', 1))},
        SC{3, ico("battery"),      StatusColor::alarm(),   tr("Критичний заряд"),
           tr("Нижче %1 В — логер перейде в аварію.").arg(QLocale().toString(m_battCritV, 'f', 1))},
    });

    item(Item::NoSd)->setName(tr("SD-карта"));
    item(Item::NoSd)->setStates({
        SC{1, ico("sd-card"), StatusColor::warning(), tr("Робота без SD"),
           tr("Оператор продовжив без SD (ID02). Звірка після польоту неможлива, дані радіомовчання втрачаються.")},
    });

    item(Item::Mismatch)->setName(tr("Узгодженість режиму"));
    item(Item::Mismatch)->setStates({
        SC{1, ico("shuffle"), StatusColor::warning(), tr("Режим не збігається"),
           tr("Логер відхилив дозволену команду (ID03): його режим відрізняється від обчисленого.")},
    });

    item(Item::Session)->setName(tr("Політ"));
    item(Item::Session)->setStates({
        SC{SessNone,      ico("circle"),             StatusColor::muted(), tr("Не розпочато"),
           tr("Дані не записуються. Почніть «Новий політ».")},
        SC{SessRecording, ico("record-circle-fill"), StatusColor::alarm(), tr("Запис"),
           tr("Усі дані польоту пишуться в базу.")},
        SC{SessViewing,   ico("eye"),                StatusColor::info(),  tr("Перегляд"),
           tr("Відкрито минулий політ. Команди вимкнено.")},
    });
}

void IndicatorPanel::applyAll()
{
    applyLink();
    applyLogger();
    applySession();
}

void IndicatorPanel::applyLink()
{
    QString detail;
    switch (m_link.state) {
    case LinkState::Closed:
        detail = tr("Порт закритий");
        break;
    case LinkState::Waiting:
        detail = tr("%1 відкрито, пакетів ще не було").arg(m_link.portName);
        break;
    case LinkState::Ok:
        detail = tr("%1 · останній пакет %2 тому").arg(m_link.portName, formatAgo(m_link.secondsSinceLastPacket));
        break;
    case LinkState::Silent:
    case LinkState::Lost:
        detail = tr("%1 · без пакетів %2").arg(m_link.portName, formatAgo(m_link.secondsSinceLastPacket));
        break;
    }
    if (m_link.commandPending)
        detail += QLatin1Char('\n') + tr("Команда очікує відповіді — нову не надіслати.");

    item(Item::Link)->setState(int(m_link.state), detail);
}

void IndicatorPanel::applyLogger()
{
    const LoggerStatus &s = m_logger;

    // ---- режим логгера ----
    QString modeDetail;
    if (s.mode == LoggerMode::Error) {
        modeDetail = errorText(s);
    } else if (s.mode == LoggerMode::Test || s.checking) {
        const auto dev = [&s](LoggerDevice d) { return s.devices.value(int(d)).check; };
        modeDetail = (s.checking ? tr("Іде перевірка") : tr("Перевірку закінчено"))
                   + QStringLiteral(": GPS %1 · LIDAR %2 · %3 %4 · %5 %6 · SD %7")
                         .arg(mark(dev(LoggerDevice::Gps)), mark(dev(LoggerDevice::Lidar)),
                              tr("БДБГ"), mark(dev(LoggerDevice::Bdbg)),
                              tr("Бат."), mark(dev(LoggerDevice::Battery)), mark(dev(LoggerDevice::Sd)));
    }
    item(Item::Mode)->setState(int(s.mode), modeDetail);

    // ---- режим разведки ----
    item(Item::Recon)->setState(int(s.reconMode));

    // ---- спектр ----
    item(Item::Spectrum)->setState(int(s.spectrumStage));

    // ---- радиомолчание: значок только когда включено ----
    item(Item::Radio)->setState(s.radioSilence ? 1 : 0);
    item(Item::Radio)->setVisible(s.radioSilence);

    // ---- батарея ----
    int batt = 0;
    QString battDetail;
    if (s.hasBattV) {
        batt = s.battV < m_battCritV ? 3 : (s.battV < m_battLowV ? 2 : 1);
        battDetail = tr("%1 В").arg(QLocale().toString(s.battV, 'f', 1));
    }
    item(Item::Battery)->setState(batt, battDetail);

    // ---- редкие состояния — только когда есть ----
    item(Item::NoSd)->setState(1);
    item(Item::NoSd)->setVisible(s.noSd);
    item(Item::Mismatch)->setState(1);
    item(Item::Mismatch)->setVisible(s.modeMismatch);
}

void IndicatorPanel::applySession()
{
    QString detail;
    const QString start = m_session.startTime.toLocalTime().toString(QStringLiteral("dd.MM HH:mm"));
    switch (m_sessionState) {
    case SessRecording:
        detail = tr("«%1» · з %2 · вимірів: %3").arg(m_session.name, start).arg(m_session.measurementCount);
        break;
    case SessViewing:
        detail = tr("«%1» від %2").arg(m_session.name, start);
        break;
    case SessNone:
        break;
    }
    item(Item::Session)->setState(m_sessionState, detail);
}


/////////////////////////////////// Тексты ///////////////////////////////////

QString IndicatorPanel::deviceName(quint8 code) const
{
    switch (code) {
    case 0: return tr("БДБГ");
    case 1: return tr("LIDAR");
    case 2: return tr("GPS");
    case 3: return tr("акумулятор");
    default: return tr("невідомий прилад (%1)").arg(code);
    }
}

QString IndicatorPanel::errorText(const LoggerStatus &s) const
{
    switch (s.errorMsgId) {
    case 0x18: return tr("Спуфінг GPS (ID24)");
    case 0x19: return tr("Відмова приладу: %1 (ID25)").arg(deviceName(s.errorDevice));
    case 0x1A: return tr("Помилка годинника (ID26)");
    case 0x1C: return tr("Не вдалося запустити набір спектра (ID28)");
    default:   return tr("Причина невідома");
    }
}
