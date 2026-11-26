#include "notifier.h"

#include <QApplication>
#include <QDebug>
#include <QEnterEvent>
#include <QFile>
#include <QHash>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QSvgRenderer>
#include <QTextLayout>
#include <QTimer>
#include <QVariantAnimation>

namespace {

constexpr int PAD = 12;        // внутренние отступы
constexpr int ICON = 20;       // размер иконки
constexpr int GAP = 10;        // иконка - текст
constexpr int CLOSE = 22;      // зона кнопки закрытия
constexpr int RADIUS = 10;
constexpr int SHADOW = 6;      // поле под тень вокруг карточки
constexpr int BAR = 3;         // полоска оставшегося времени

struct Theme {
    bool   dark;
    QColor bg, border, text, sub, closeHover;
};

Theme themeFor(const QWidget *w)
{
    QColor win = w ? w->palette().color(QPalette::Window) : QColor(Qt::white);
    win.setAlpha(255);
    Theme t;
    t.dark = win.lightness() < 128;
    if (t.dark) {
        t.bg         = QColor(0x2B, 0x2E, 0x34);
        t.border     = QColor(255, 255, 255, 28);
        t.text       = QColor(0xEC, 0xEE, 0xF1);
        t.sub        = QColor(0xA9, 0xB1, 0xBA);
        t.closeHover = QColor(255, 255, 255, 26);
    } else {
        t.bg         = QColor(0xFF, 0xFF, 0xFF);
        t.border     = QColor(0, 0, 0, 30);
        t.text       = QColor(0x1F, 0x23, 0x28);
        t.sub        = QColor(0x59, 0x63, 0x6E);
        t.closeHover = QColor(0, 0, 0, 18);
    }
    return t;
}

QString iconPath(int type)
{
    switch (type) {
    case NotificationManager::Warning: return QStringLiteral(":/icons/svg/bs5/exclamation-triangle-fill.svg");
    case NotificationManager::Error:   return QStringLiteral(":/icons/svg/bs5/x-octagon-fill.svg");
    case NotificationManager::Success: return QStringLiteral(":/icons/svg/bs5/check-circle-fill.svg");
    default:                           return QStringLiteral(":/icons/svg/bs5/info-circle-fill.svg");
    }
}

// SVG Bootstrap с currentColor -> картинка нужного цвета (с кэшем)
QPixmap svgPixmap(const QString &path, const QColor &color, int px, qreal dpr)
{
    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1|%2|%3|%4").arg(path, color.name()).arg(px).arg(dpr);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QPixmap();
    QByteArray svg = f.readAll();
    svg.replace("currentColor", color.name().toLatin1());

    QSvgRenderer r(svg);
    QPixmap pm(QSize(px, px) * dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        r.render(&p);
    }
    pm.setDevicePixelRatio(dpr);
    cache.insert(key, pm);
    return pm;
}

QFont titleFont(const QFont &base) { QFont f = base; f.setBold(true); return f; }

// Высота текста с переносом по словам
int wrappedHeight(const QString &text, const QFont &font, int width)
{
    if (text.isEmpty())
        return 0;
    const QFontMetrics fm(font);
    return fm.boundingRect(QRect(0, 0, width, 10000), Qt::TextWordWrap, text).height();
}

} // namespace

// ======================================================================
// ToastWidget
// ======================================================================

ToastWidget::ToastWidget(int type, const QString &title, const QString &message, int durationMs, QWidget *host)
    : QWidget(host), m_type(type), m_title(title), m_message(message), m_duration(durationMs)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QString());

    m_ticker = new QTimer(this);
    m_ticker->setInterval(33);
    connect(m_ticker, &QTimer::timeout, this, &ToastWidget::tick);

    m_fade = new QVariantAnimation(this);
    m_fade->setDuration(180);
    m_fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { setOpacity(v.toReal()); });

    m_move = new QPropertyAnimation(this, "pos", this);
    m_move->setDuration(220);
    m_move->setEasingCurve(QEasingCurve::OutCubic);

    relayout();
}

void ToastWidget::relayout()
{
    const int textW = WIDTH - 2 * SHADOW - PAD - ICON - GAP - CLOSE - PAD / 2;
    const QFont fTitle = titleFont(font());
    int h = 0;
    if (!m_title.isEmpty())
        h += QFontMetrics(fTitle).height();
    if (!m_message.isEmpty()) {
        if (h > 0)
            h += 2;
        h += wrappedHeight(m_message, font(), textW);
    }
    h = qMax(h, ICON);
    if (!m_actionText.isEmpty())
        h += 8 + QFontMetrics(font()).height() + 8;     // кнопка действия
    setFixedSize(WIDTH, h + 2 * PAD + BAR + 2 * SHADOW);
}

void ToastWidget::setAction(const QString &text, std::function<void()> callback)
{
    m_actionText = text;
    m_action = std::move(callback);
    relayout();
    update();
}

/// Кнопка действия — под текстом, на одной вертикали с ним
QRect ToastWidget::actionRect() const
{
    if (m_actionText.isEmpty())
        return QRect();
    const QFontMetrics fm(font());
    const int x = SHADOW + PAD + ICON + GAP;
    const int h = fm.height() + 8;
    const int bottom = height() - SHADOW - 1 - BAR - PAD + 2;
    return QRect(x, bottom - h, fm.horizontalAdvance(m_actionText) + 20, h);
}

QRect ToastWidget::closeRect() const
{
    return QRect(width() - SHADOW - PAD / 2 - CLOSE, SHADOW + PAD / 2, CLOSE, CLOSE);
}

qreal ToastWidget::progress() const
{
    qint64 el = m_elapsedBefore;
    if (m_clock.isValid())
        el += m_clock.elapsed();
    return qBound(0.0, 1.0 - double(el) / qMax(1, m_duration), 1.0);
}

void ToastWidget::appear()
{
    show();
    raise();
    m_fade->stop();
    m_fade->setStartValue(m_opacity);
    m_fade->setEndValue(1.0);
    m_fade->start();

    m_elapsedBefore = 0;
    m_clock.start();
    m_ticker->start();
}

void ToastWidget::bump()
{
    ++m_count;
    m_elapsedBefore = 0;
    if (!m_hovered)
        m_clock.start();
    // короткая "вспышка", чтобы было видно, что сообщение повторилось
    m_fade->stop();
    m_fade->setStartValue(0.55);
    m_fade->setEndValue(1.0);
    m_fade->start();
    raise();
    update();
}

void ToastWidget::dismiss()
{
    if (m_closing)
        return;
    m_closing = true;
    m_ticker->stop();
    setAttribute(Qt::WA_TransparentForMouseEvents);

    m_fade->stop();
    m_fade->setStartValue(m_opacity);
    m_fade->setEndValue(0.0);
    connect(m_fade, &QVariantAnimation::finished, this, [this] {
        emit dismissed(this);
        deleteLater();
    });
    m_fade->start();
}

void ToastWidget::moveAnimated(const QPoint &target)
{
    if (!isVisible() || pos() == target) {
        m_move->stop();
        move(target);
        return;
    }
    if (m_move->state() == QAbstractAnimation::Running && m_move->endValue().toPoint() == target)
        return;
    m_move->stop();
    m_move->setStartValue(pos());
    m_move->setEndValue(target);
    m_move->start();
}

void ToastWidget::tick()
{
    if (m_hovered)
        return;
    if (progress() <= 0.0) {
        dismiss();
        return;
    }
    update(QRect(SHADOW, height() - SHADOW - BAR - 1, width() - 2 * SHADOW, BAR + 2));
}

void ToastWidget::enterEvent(QEnterEvent *)
{
    // пауза: запоминаем прошедшее время
    m_hovered = true;
    if (m_clock.isValid()) {
        m_elapsedBefore += m_clock.elapsed();
        m_clock.invalidate();
    }
    update();
}

void ToastWidget::leaveEvent(QEvent *)
{
    m_hovered = false;
    m_closeHover = false;
    m_clock.start();
    update();
}

void ToastWidget::mouseMoveEvent(QMouseEvent *e)
{
    const bool over = closeRect().contains(e->position().toPoint());
    if (over != m_closeHover) {
        m_closeHover = over;
        update(closeRect().adjusted(-2, -2, 2, 2));
    }
    const bool overAction = actionRect().contains(e->position().toPoint());
    if (overAction != m_actionHover) {
        m_actionHover = overAction;
        update();
    }
}

void ToastWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    if (m_action && actionRect().contains(e->position().toPoint()))
        m_action();
    dismiss();
}

void ToastWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);

    const Theme t = themeFor(parentWidget());
    const QColor accent = NotificationManager::typeColor(NotificationManager::NotificationType(m_type));
    const QRectF card = QRectF(rect()).adjusted(SHADOW, SHADOW - 1, -SHADOW, -SHADOW - 1);

    // мягкая тень - несколько полупрозрачных контуров
    p.setPen(Qt::NoPen);
    for (int i = SHADOW; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, t.dark ? 14 : 7));
        p.drawRoundedRect(card.adjusted(-i, -i + 2, i, i + 2), RADIUS + i, RADIUS + i);
    }

    QPainterPath shape;
    shape.addRoundedRect(card, RADIUS, RADIUS);
    p.fillPath(shape, t.bg);
    // лёгкий цветной оттенок по типу
    QColor tint = accent;
    tint.setAlpha(t.dark ? 22 : 14);
    p.fillPath(shape, tint);
    p.setPen(QPen(t.border, 1));
    p.drawPath(shape);

    // иконка
    const qreal dpr = devicePixelRatioF();
    const QRect iconR(int(card.left()) + PAD, int(card.top()) + PAD, ICON, ICON);
    p.drawPixmap(iconR, svgPixmap(iconPath(m_type), accent, ICON, dpr));

    // текст
    const int textX = iconR.right() + 1 + GAP;
    const int textW = int(card.right()) - CLOSE - PAD / 2 - textX;
    int y = int(card.top()) + PAD;
    const QFont fTitle = titleFont(font());

    QString titleText = m_title;
    QString msgText = m_message;
    if (titleText.isEmpty() && m_message.isEmpty())
        titleText = QStringLiteral(" ");

    if (!titleText.isEmpty()) {
        const QFontMetrics fm(fTitle);
        p.setFont(fTitle);
        p.setPen(t.text);
        p.drawText(QRect(textX, y, textW, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(titleText, Qt::ElideRight, textW));
        y += fm.height() + 2;
    } else {
        // без заголовка текст по центру иконки, если он в одну строку
        const int h = wrappedHeight(msgText, font(), textW);
        if (h < ICON)
            y += (ICON - h) / 2;
    }
    if (!msgText.isEmpty()) {
        p.setFont(font());
        p.setPen(m_title.isEmpty() ? t.text : t.sub);
        p.drawText(QRect(textX, y, textW, 10000), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, msgText);
    }

    // кнопка действия
    if (!m_actionText.isEmpty()) {
        const QRect ar = actionRect();
        QColor fill = accent;
        fill.setAlpha(m_actionHover ? 70 : 38);
        p.setPen(QPen(accent, 1));
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(ar).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        QFont fb = font();
        fb.setBold(true);
        p.setFont(fb);
        p.setPen(t.text);
        p.drawText(ar, Qt::AlignCenter, m_actionText);
        p.setFont(font());
    }

    // кнопка закрытия + счётчик повторов
    const QRect cr = closeRect();
    if (m_closeHover) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.closeHover);
        p.drawRoundedRect(cr, 6, 6);
    }
    p.setPen(QPen(m_hovered ? t.text : t.sub, 1.4, Qt::SolidLine, Qt::RoundCap));
    const QPointF c = QRectF(cr).center();
    const qreal k = 4.0;
    p.drawLine(c + QPointF(-k, -k), c + QPointF(k, k));
    p.drawLine(c + QPointF(-k, k), c + QPointF(k, -k));

    if (m_count > 1) {
        QFont fb = font();
        fb.setBold(true);
        fb.setPointSizeF(qMax(7.0, fb.pointSizeF() - 1.5));
        const QString s = QStringLiteral("×%1").arg(m_count);
        const QFontMetrics fm(fb);
        const QRectF pill(cr.left() + (cr.width() - (fm.horizontalAdvance(s) + 10)) / 2.0,
                          cr.bottom() + 4, fm.horizontalAdvance(s) + 10, fm.height() + 2);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
        p.setFont(fb);
        p.setPen(Qt::white);
        p.drawText(pill, Qt::AlignCenter, s);
    }

    // оставшееся время
    p.save();
    p.setClipPath(shape);
    const QRectF track(card.left(), card.bottom() - BAR, card.width(), BAR);
    QColor trackC = accent;
    trackC.setAlpha(40);
    p.fillRect(track, trackC);
    p.fillRect(QRectF(track.left(), track.top(), track.width() * progress(), BAR), accent);
    p.restore();
}

// ======================================================================
// NotificationManager
// ======================================================================

NotificationManager::NotificationManager(QObject* parent)
    : QObject(parent)
{
}

NotificationManager& NotificationManager::instance()
{
    static NotificationManager instance;
    return instance;
}

QColor NotificationManager::typeColor(NotificationType type)
{
    switch (type) {
    case Warning: return QColor(0xD9, 0x96, 0x1A);
    case Error:   return QColor(0xE5, 0x48, 0x4D);
    case Success: return QColor(0x2E, 0x9E, 0x6A);
    default:      return QColor(0x3D, 0x8B, 0xFD);
    }
}

void NotificationManager::showInfo(const QString& message, const QString& title)    { show(Info, message, title); }
void NotificationManager::showWarning(const QString& message, const QString& title) { show(Warning, message, title); }
void NotificationManager::showError(const QString& message, const QString& title)   { show(Error, message, title); }
void NotificationManager::showSuccess(const QString& message, const QString& title) { show(Success, message, title); }

void NotificationManager::show(NotificationType type, const QString& message, const QString& title, int durationMs)
{
    instance().showNotification(message, title, type, durationMs);
}

void NotificationManager::showAction(NotificationType type, const QString& message, const QString& title,
                                     const QString& actionText, std::function<void()> callback)
{
    instance().showNotification(message, title, type, instance().duration * 2, actionText, std::move(callback));
}

void NotificationManager::setHost(QWidget *w)
{
    if (host == w)
        return;
    clear();
    if (host)
        host->removeEventFilter(this);
    host = w;
    if (host)
        host->installEventFilter(this);
}

QWidget *NotificationManager::resolveHost()
{
    if (host)
        return host;

    // Главное окно: видимый QMainWindow, иначе активное окно
    QWidget *found = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets()) {
        if (qobject_cast<QMainWindow *>(w) && w->isVisible()) {
            found = w;
            break;
        }
    }
    if (!found)
        found = QApplication::activeWindow();
    if (found)
        setHost(found);
    return host;
}

void NotificationManager::showNotification(const QString& message, const QString& title,
                                           NotificationType type, int durationMs,
                                           const QString& actionText, std::function<void()> callback)
{
    // В журнал / боковую панель - всегда, даже если показать негде
    emit notificationAdded(title.isEmpty() ? message : title + QStringLiteral(": ") + message,
                           type, QDateTime::currentDateTime());

    QWidget *h = resolveHost();
    if (!h) {
        qInfo().noquote() << "[notification]" << title << message;
        return;
    }

    // Повтор того же сообщения - не новый тост, а счётчик у старого
    for (const QPointer<ToastWidget> &t : std::as_const(activeNotifications)) {
        if (t && !t->isClosing() && t->type() == type && t->title() == title && t->message() == message) {
            t->bump();
            return;
        }
    }

    if (durationMs <= 0) {
        switch (type) {
        case Warning: durationMs = duration * 3 / 2; break;
        case Error:   durationMs = duration * 2;     break;
        default:      durationMs = duration;         break;
        }
    }

    auto *toast = new ToastWidget(type, title, message, durationMs, h);
    if (!actionText.isEmpty())
        toast->setAction(actionText, std::move(callback));
    connect(toast, &ToastWidget::dismissed, this, &NotificationManager::onToastDismissed);
    activeNotifications.append(toast);

    // Лишние (самые старые) закрываем
    int alive = 0;
    for (const QPointer<ToastWidget> &t : std::as_const(activeNotifications))
        if (t && !t->isClosing())
            ++alive;
    for (const QPointer<ToastWidget> &t : std::as_const(activeNotifications)) {
        if (alive <= maxNotifications)
            break;
        if (t && !t->isClosing()) {
            t->dismiss();
            --alive;
        }
    }

    positionNotifications(true);
    toast->appear();
}

// Стопка снизу по центру окна, новые - внизу
void NotificationManager::positionNotifications(bool animated)
{
    if (!host)
        return;

    activeNotifications.removeAll(QPointer<ToastWidget>());
    const QRect area = host->rect();
    int y = area.bottom() - MARGIN;

    for (int i = activeNotifications.size() - 1; i >= 0; --i) {
        ToastWidget *t = activeNotifications[i];
        if (!t || t->isClosing())
            continue;
        const int x = area.left() + (area.width() - t->width()) / 2;
        y -= t->height();
        const QPoint target(x, y);
        if (!t->isVisible())
            t->move(target + QPoint(0, 14));   // появляется, слегка "выезжая" снизу
        if (animated)
            t->moveAnimated(target);
        else
            t->move(target);
        y -= SPACING - 2 * SHADOW;             // поля под тень у соседних тостов перекрываются
    }
}

void NotificationManager::onToastDismissed(ToastWidget *toast)
{
    activeNotifications.removeAll(QPointer<ToastWidget>(toast));
    positionNotifications(true);
}

void NotificationManager::clear()
{
    for (const QPointer<ToastWidget> &t : std::as_const(activeNotifications))
        if (t)
            t->deleteLater();
    activeNotifications.clear();
}

bool NotificationManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == host) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::LayoutRequest:
            positionNotifications(false);
            break;
        case QEvent::ChildAdded:
            // новые дочерние виджеты окна (оверлеи, шторки) не должны перекрывать тосты
            QTimer::singleShot(0, this, [this] {
                for (const QPointer<ToastWidget> &t : std::as_const(activeNotifications))
                    if (t)
                        t->raise();
            });
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}
