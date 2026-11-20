#include "alarmdrawer.h"

#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariantAnimation>

#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor, renderSvg
#include "ui/modules/notifier/notifier.h"

namespace {

const QString kIco = QStringLiteral(":/icons/svg/bs5/%1.svg");

constexpr int kEdgeZone    = 48;    ///< ширина зоны у правого края, где появляется язычок, px
constexpr int kPanelWidth  = 400;
constexpr int kMargin      = 10;
constexpr int kAnimMs      = 180;
constexpr int kHideDelayMs = 700;   ///< язычок не пропадает сразу, когда мышь ушла
constexpr int kPollMs      = 120;

QColor mix(const QColor &a, const QColor &b, double k)
{
    return QColor::fromRgbF(a.redF() * (1 - k) + b.redF() * k,
                            a.greenF() * (1 - k) + b.greenF() * k,
                            a.blueF() * (1 - k) + b.blueF() * k);
}

} // namespace


/////////////////////////////////// AlarmEdgeHandle ///////////////////////////////////

AlarmEdgeHandle::AlarmEdgeHandle(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(sizeHint());
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setToolTip(tr("Тривоги"));
}

void AlarmEdgeHandle::setCount(int n)
{
    m_count = n;
    setToolTip(n > 0 ? tr("Тривоги — нових: %1").arg(n) : tr("Тривоги"));
    update();
}

void AlarmEdgeHandle::setFlash(qreal f)
{
    m_flash = f;
    update();
}

void AlarmEdgeHandle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Язычок: скруглён слева, правой стороной прилегает к краю карты
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, 10, -0.5);
    QPainterPath path;
    path.addRoundedRect(r, 10, 10);

    QColor bg = palette().color(QPalette::Window);
    if (m_hover)
        bg = mix(bg, palette().color(QPalette::Highlight), 0.18);
    if (m_flash > 0)
        bg = mix(bg, StatusColor::alarm(), 0.55 * m_flash);
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.setBrush(bg);
    p.drawPath(path);

    const qreal dpr = devicePixelRatioF();
    const bool alarm = m_count > 0;
    const QColor iconColor = alarm ? StatusColor::alarm() : palette().color(QPalette::WindowText);

    // Колокольчик
    const QPixmap bell = IndicatorItem::renderSvg(kIco.arg(alarm ? QStringLiteral("bell-fill") : QStringLiteral("bell")),
                                                  iconColor, 18, dpr);
    p.drawPixmap((width() - 18) / 2, 16, bell);

    // Число новых
    if (alarm) {
        const QString s = m_count > 99 ? QStringLiteral("99+") : QString::number(m_count);
        QFont f = font();
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        const int w = qMax(20, fm.horizontalAdvance(s) + 8);
        const QRectF badge((width() - w) / 2.0, 44, w, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(StatusColor::alarm());
        p.drawRoundedRect(badge, 10, 10);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, s);
    }

    // Стрелка «выдвинуть»
    const QPixmap chev = IndicatorItem::renderSvg(kIco.arg(QStringLiteral("chevron-left")),
                                                  palette().color(QPalette::WindowText), 14, dpr);
    p.drawPixmap((width() - 14) / 2, height() - 30, chev);
}

void AlarmEdgeHandle::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()))
        emit clicked();
}

void AlarmEdgeHandle::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();
    QWidget::enterEvent(e);
}

void AlarmEdgeHandle::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(e);
}


/////////////////////////////////// AlarmDrawer ///////////////////////////////////

AlarmDrawer::AlarmDrawer(QWidget *host)
    : QObject(host)
    , m_host(host)
{
    m_handle = new AlarmEdgeHandle(host);
    m_handle->hide();

    m_center = new AlarmCenter(host);
    m_center->hide();

    m_handleAnim = new QPropertyAnimation(m_handle, "pos", this);
    m_handleAnim->setDuration(kAnimMs);
    m_handleAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_panelAnim = new QPropertyAnimation(m_center, "pos", this);
    m_panelAnim->setDuration(kAnimMs);
    m_panelAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_flashAnim = new QVariantAnimation(this);
    m_flashAnim->setDuration(1800);
    m_flashAnim->setKeyValues({{0.0, 0.0}, {0.15, 1.0}, {0.33, 0.0}, {0.5, 1.0}, {0.66, 0.0}, {0.83, 1.0}, {1.0, 0.0}});
    connect(m_flashAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { m_handle->setFlash(v.toReal()); });

    m_poll = new QTimer(this);
    m_poll->setInterval(kPollMs);
    connect(m_poll, &QTimer::timeout, this, &AlarmDrawer::pollCursor);
    m_poll->start();

    m_hideDelay = new QTimer(this);
    m_hideDelay->setSingleShot(true);
    m_hideDelay->setInterval(kHideDelayMs);
    connect(m_hideDelay, &QTimer::timeout, this, [this] {
        m_hover = false;
        updateHandle();
    });

    connect(m_handle, &AlarmEdgeHandle::clicked, this, &AlarmDrawer::open);
    connect(m_center, &AlarmCenter::closeRequested, this, &AlarmDrawer::close);
    connect(m_center, &AlarmCenter::unacknowledgedCountChanged, this, [this](int n) {
        m_handle->setCount(n);
        updateHandle();
    });
    connect(m_center, &AlarmCenter::alarmAdded, this, &AlarmDrawer::onAlarmAdded);

    host->installEventFilter(this);
    m_handle->setCount(m_center->unacknowledgedCount());
    placeWidgets();
}

void AlarmDrawer::open()
{
    if (m_open || !m_host)
        return;
    m_open = true;
    updateHandle();

    const QRect r = panelRect();
    m_center->setGeometry(r.translated(r.width() + kMargin, 0));
    m_center->show();
    m_center->raise();
    m_panelAnim->stop();
    m_panelAnim->setStartValue(m_center->pos());
    m_panelAnim->setEndValue(r.topLeft());
    disconnect(m_panelAnim, &QPropertyAnimation::finished, nullptr, nullptr);
    m_panelAnim->start();
    m_center->setFocus();
}

void AlarmDrawer::close()
{
    if (!m_open || !m_host)
        return;
    m_open = false;

    const QRect r = panelRect();
    m_panelAnim->stop();
    m_panelAnim->setStartValue(m_center->pos());
    m_panelAnim->setEndValue(QPoint(m_host->width() + kMargin, r.top()));
    disconnect(m_panelAnim, &QPropertyAnimation::finished, nullptr, nullptr);
    connect(m_panelAnim, &QPropertyAnimation::finished, m_center, &QWidget::hide);
    m_panelAnim->start();

    updateHandle();
}

void AlarmDrawer::toggle()
{
    m_open ? close() : open();
}

bool AlarmDrawer::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_host) {
        if (e->type() == QEvent::Resize) {
            placeWidgets();
        } else if (e->type() == QEvent::MouseButtonPress && m_open) {
            close();    // щелчок по карте мимо шторки
        }
    }
    return QObject::eventFilter(obj, e);
}

void AlarmDrawer::pollCursor()
{
    if (!m_host || !m_host->isVisible() || m_open)
        return;

    const QPoint p = m_host->mapFromGlobal(QCursor::pos());
    const bool nearEdge = m_host->rect().contains(p) && p.x() >= m_host->width() - kEdgeZone;
    const bool overHandle = m_handle->isVisible() && m_handle->geometry().contains(p);

    if (nearEdge || overHandle) {
        m_hideDelay->stop();
        if (!m_hover) {
            m_hover = true;
            updateHandle();
        }
    } else if (m_hover && !m_hideDelay->isActive()) {
        m_hideDelay->start();
    }
}

void AlarmDrawer::updateHandle(bool animate)
{
    if (!m_host)
        return;
    const bool want = !m_open && (m_hover || m_center->unacknowledgedCount() > 0);
    if (want == m_handleShown && (want == m_handle->isVisible()))
        return;
    m_handleShown = want;

    m_handleAnim->stop();
    disconnect(m_handleAnim, &QPropertyAnimation::finished, nullptr, nullptr);
    if (want) {
        if (!m_handle->isVisible())
            m_handle->move(handlePos(false));
        m_handle->show();
        m_handle->raise();
        if (animate) {
            m_handleAnim->setStartValue(m_handle->pos());
            m_handleAnim->setEndValue(handlePos(true));
            m_handleAnim->start();
        } else {
            m_handle->move(handlePos(true));
        }
    } else if (m_handle->isVisible()) {
        if (animate) {
            m_handleAnim->setStartValue(m_handle->pos());
            m_handleAnim->setEndValue(handlePos(false));
            connect(m_handleAnim, &QPropertyAnimation::finished, m_handle, &QWidget::hide);
            m_handleAnim->start();
        } else {
            m_handle->hide();
        }
    }
}

void AlarmDrawer::placeWidgets()
{
    if (!m_host)
        return;
    // Размер карты поменялся во время анимации — иначе она довезёт виджет в старую точку
    if (m_handle->isVisible()) {
        if (m_handleAnim->state() == QAbstractAnimation::Running)
            m_handleAnim->setEndValue(handlePos(m_handleShown));
        else
            m_handle->move(handlePos(m_handleShown));
    }
    if (m_open) {
        m_panelAnim->stop();
        m_center->setGeometry(panelRect());
    }
}

QRect AlarmDrawer::panelRect() const
{
    const int w = qMin(kPanelWidth, m_host->width() - 2 * kMargin);
    return QRect(m_host->width() - w - kMargin, kMargin, w, m_host->height() - 2 * kMargin);
}

QPoint AlarmDrawer::handlePos(bool shown) const
{
    const int y = (m_host->height() - m_handle->height()) / 2;
    return QPoint(shown ? m_host->width() - m_handle->width() : m_host->width(), y);
}

void AlarmDrawer::onAlarmAdded(const Alarm &a)
{
    updateHandle();
    if (!m_open) {
        m_flashAnim->stop();
        m_flashAnim->start();
    }
    const QString text = AlarmCenter::alarmText(a);
    if (a.level == AlarmLevel::Alarm)
        NotificationManager::showError(text);
    else
        NotificationManager::showWarning(text);
}
