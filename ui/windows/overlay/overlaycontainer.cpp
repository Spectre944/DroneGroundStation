#include "overlaycontainer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QGraphicsDropShadowEffect>
#include <QEvent>
#include <QKeyEvent>

OverlayContainer::OverlayContainer(QWidget *parent)
    : QWidget(parent)
{
    Q_ASSERT(parent);

    setObjectName("OverlayContainer");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    // Шапка (по умолчанию скрыта)
    m_header = new QWidget(this);
    m_header->setObjectName("OverlayHeader");
    auto *hl = new QHBoxLayout(m_header);
    hl->setContentsMargins(16, 12, 8, 4);

    m_titleLabel = new QLabel(m_header);
    m_titleLabel->setObjectName("OverlayTitle");

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setObjectName("OverlayClose");
    m_closeBtn->setText(QString::fromUtf8("\u2715"));   // можно заменить на SVG-иконку x-lg
    m_closeBtn->setAutoRaise(true);
    m_closeBtn->setCursor(Qt::PointingHandCursor);

    hl->addWidget(m_titleLabel, 1);
    hl->addWidget(m_closeBtn);
    m_header->hide();
    m_layout->addWidget(m_header);

    connect(m_closeBtn, &QToolButton::clicked, this, &OverlayContainer::hideOverlay);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 70));
    setGraphicsEffect(shadow);

    applyStyle();

    parent->installEventFilter(this);   // следим за ресайзом родителя
    updatePosition();
}

// ---------- Содержимое ----------

void OverlayContainer::setContentWidget(QWidget *widget)
{
    if (m_contentWidget == widget)
        return;

    if (m_contentWidget) {
        m_layout->removeWidget(m_contentWidget);
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;

    if (m_contentWidget)
        m_layout->addWidget(m_contentWidget);

    updatePosition();
}

void OverlayContainer::setContentMargins(int l, int t, int r, int b)
{
    m_layout->setContentsMargins(l, t, r, b);
    updatePosition();
}

// ---------- Расположение ----------

void OverlayContainer::setAnchor(Qt::Alignment anchor)
{
    m_anchor = anchor;
    updatePosition();
}

void OverlayContainer::setOuterMargins(const QMargins &margins)
{
    m_outer = margins;
    updatePosition();
}

void OverlayContainer::setStretch(Qt::Orientations o)
{
    m_stretch = o;
    updatePosition();
}

void OverlayContainer::setRelativeSize(qreal w, qreal h)
{
    m_relSize = QSizeF(w, h);
    updatePosition();
}

void OverlayContainer::updatePosition()
{
    QWidget *p = parentWidget();
    if (!p || m_updating)
        return;

    const QRect area = p->rect().marginsRemoved(m_outer);
    if (area.width() <= 0 || area.height() <= 0)
        return;

    m_updating = true;

    const QSize minHint = minimumSizeHint();
    const QSize hint    = sizeHint().expandedTo(minHint);

    int w = hint.width();
    if (m_relSize.width() > 0)               w = int(area.width() * m_relSize.width());
    else if (m_stretch & Qt::Horizontal)     w = area.width();

    int h = hint.height();
    if (m_relSize.height() > 0)              h = int(area.height() * m_relSize.height());
    else if (m_stretch & Qt::Vertical)       h = area.height();

    // maximumSize() задаёт потолок для больших страниц: setMaximumSize(1000, 800)
    QSize s = QSize(w, h).expandedTo(minHint)
                  .boundedTo(maximumSize())
                  .boundedTo(area.size());

    int x = area.left();
    if (m_anchor & Qt::AlignRight)
        x = area.right() - s.width() + 1;
    else if (m_anchor & Qt::AlignHCenter)
        x = area.left() + (area.width() - s.width()) / 2;

    int y = area.top();
    if (m_anchor & Qt::AlignBottom)
        y = area.bottom() - s.height() + 1;
    else if (m_anchor & Qt::AlignVCenter)
        y = area.top() + (area.height() - s.height()) / 2;

    setGeometry(QRect(QPoint(x, y), s));

    m_updating = false;
}

// ---------- Страницы ----------

void OverlayContainer::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
    m_header->setVisible(!title.isEmpty());
    updatePosition();
}

void OverlayContainer::setModal(bool modal)
{
    m_modal = modal;

    if (modal && !m_backdrop) {
        m_backdrop = new QWidget(parentWidget());
        m_backdrop->setObjectName("OverlayBackdrop");
        m_backdrop->setAttribute(Qt::WA_StyledBackground, true);
        m_backdrop->setStyleSheet("#OverlayBackdrop { background-color: rgba(0, 0, 0, 110); }");
        m_backdrop->installEventFilter(this);
        m_backdrop->hide();
    }
    if (!modal && m_backdrop)
        m_backdrop->hide();
}

void OverlayContainer::showOverlay()
{
    updatePosition();
    show();
}

void OverlayContainer::hideOverlay()
{
    if (!isVisible())
        return;
    hide();
    emit closed();
}

// ---------- Оформление ----------

void OverlayContainer::setBackgroundColor(const QColor &color)
{
    m_bg = color;
    applyStyle();
}

void OverlayContainer::setBorderRadius(int radius)
{
    m_radius = radius;
    applyStyle();
}

void OverlayContainer::applyStyle()
{
    setStyleSheet(QString(R"(
        #OverlayContainer {
            background-color: rgba(%1, %2, %3, %4);
            border: 1px solid rgba(255, 255, 255, 25);
            border-radius: %5px;
        }
        #OverlayTitle { color: white; font-size: 15px; font-weight: 600; }
        #OverlayClose { color: #aab; border: none; font-size: 14px; padding: 4px 8px; }
        #OverlayClose:hover { color: white; background: rgba(255,255,255,30); border-radius: 6px; }
    )").arg(m_bg.red()).arg(m_bg.green()).arg(m_bg.blue()).arg(m_bg.alpha()).arg(m_radius));
}

// ---------- События ----------

bool OverlayContainer::event(QEvent *e)
{
    const bool r = QWidget::event(e);
    // содержимое изменило sizeHint (например, текст в панели координат стал длиннее)
    if (e->type() == QEvent::LayoutRequest)
        updatePosition();
    return r;
}

bool OverlayContainer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        if (m_backdrop)
            m_backdrop->setGeometry(parentWidget()->rect());
        updatePosition();
    }
    else if (watched == m_backdrop) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            hideOverlay();               // клик по затемнению закрывает страницу
            return true;
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
            return true;                 // не даём событиям дойти до карты
        default: break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void OverlayContainer::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    if (m_modal && m_backdrop) {
        m_backdrop->setGeometry(parentWidget()->rect());
        m_backdrop->show();
        m_backdrop->raise();
    }
    raise();
    updatePosition();

    if (m_modal)
        setFocus();
}

void OverlayContainer::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    if (m_backdrop)
        m_backdrop->hide();
}

void OverlayContainer::keyPressEvent(QKeyEvent *e)
{
    if (m_modal && e->key() == Qt::Key_Escape) {
        hideOverlay();
        return;
    }
    QWidget::keyPressEvent(e);
}

void OverlayContainer::mousePressEvent(QMouseEvent *e)       { e->accept(); }
void OverlayContainer::mouseReleaseEvent(QMouseEvent *e)     { e->accept(); }
void OverlayContainer::mouseMoveEvent(QMouseEvent *e)        { e->accept(); }
void OverlayContainer::mouseDoubleClickEvent(QMouseEvent *e) { e->accept(); }
void OverlayContainer::wheelEvent(QWheelEvent *e)            { e->accept(); }