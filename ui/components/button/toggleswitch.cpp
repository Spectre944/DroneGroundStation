#include "toggleswitch.h"

#include <QPainter>
#include <QPainterPath>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(this, &QAbstractButton::clicked, this, [this] {
        if (!m_pending)
            emit toggleRequested(!m_on);
    });
}

void ToggleSwitch::setOn(bool on)
{
    if (m_on == on)
        return;
    m_on = on;
    update();
}

void ToggleSwitch::setPending(bool pending)
{
    m_pending = pending;
    update();
}

void ToggleSwitch::setLabels(const QString &on, const QString &off)
{
    m_onText = on;
    m_offText = off;
    updateGeometry();
    update();
}

QSize ToggleSwitch::sizeHint() const
{
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1));
    const QFontMetrics fm(f);
    const int text = qMax(fm.horizontalAdvance(m_onText), fm.horizontalAdvance(m_offText));
    const int h = 28;
    return QSize(text + h + 18, h);        // текст + квадратный ползунок + поля
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QPalette pal = palette();
    QColor accent = m_accent.isValid() ? m_accent : pal.color(QPalette::Highlight);
    accent.setAlpha(255);
    const bool enabled = isEnabled();

    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = 6;

    // Дорожка: «увімк» — акцент, «вимк» — нейтральный фон кнопки
    QColor trackColor = m_on ? accent : pal.color(QPalette::Button);
    trackColor.setAlpha(255);
    if (!enabled)
        trackColor = m_on ? accent.lighter(150) : pal.color(QPalette::Window);
    p.setPen(QPen(m_on ? accent.darker(115) : pal.color(QPalette::Mid), 1));
    p.setBrush(trackColor);
    p.drawRoundedRect(track, radius, radius);

    // Ползунок — квадрат с закруглёнными углами, слева при «вимк», справа при «увімк»
    const qreal k = track.height() - 6;
    QRectF knob(0, 0, k, k);
    knob.moveCenter(QPointF(m_on ? track.right() - 3 - k / 2 : track.left() + 3 + k / 2, track.center().y()));
    QColor knobColor = Qt::white;
    if (m_pending)
        knobColor.setAlpha(140);
    p.setPen(QPen(QColor(0, 0, 0, 40), 1));
    p.setBrush(knobColor);
    p.drawRoundedRect(knob, radius - 2, radius - 2);

    // Подпись — на свободной стороне дорожки
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1));
    p.setFont(f);
    QColor textColor = m_on ? QColor(Qt::white) : pal.color(QPalette::ButtonText);
    if (!enabled)
        textColor = pal.color(QPalette::Disabled, QPalette::ButtonText);
    p.setPen(textColor);
    const QRectF textRect = m_on ? QRectF(track.left() + 6, track.top(), knob.left() - track.left() - 8, track.height())
                                 : QRectF(knob.right() + 4, track.top(), track.right() - knob.right() - 10, track.height());
    p.drawText(textRect, Qt::AlignCenter, m_pending ? QStringLiteral("…") : (m_on ? m_onText : m_offText));

    if (hasFocus()) {
        p.setPen(QPen(accent, 1.5, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(track.adjusted(-0.5, -0.5, 0.5, 0.5), radius, radius);
    }
}
