#include "pagebar.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>
#include <QToolButton>

namespace {

constexpr int IconPx   = 22;    ///< размер значка
constexpr int ButtonW  = 88;    ///< ширина кнопки: «Налаштування» в одну строку

/**
    @brief SVG (bootstrap: currentColor) → картинка нужного цвета.
*/
QPixmap tinted(const QString &path, const QColor &color, qreal dpr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QPixmap();
    QByteArray svg = f.readAll();
    svg.replace("currentColor", color.name().toLatin1());

    QSvgRenderer r(svg);
    QPixmap pm(QSize(IconPx, IconPx) * dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

QString rgba(const QColor &c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

} // namespace

PageBar::PageBar(QWidget *parent)
    : QToolBar(parent)
{
    setObjectName("pageBar");
    setWindowTitle(tr("Сторінки"));
    setOrientation(Qt::Vertical);
    setMovable(false);
    setFloatable(false);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setIconSize(QSize(IconPx, IconPx));
    setContextMenuPolicy(Qt::PreventContextMenu);      // без меню «скрыть панель»
    toggleViewAction()->setVisible(false);

    // Растяжка: всё, что после неё, прижато к низу панели
    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_spacer = addWidget(spacer);

    applyTheme();
}

void PageBar::addPage(QAction *action, const QString &svgPath)
{
    insertAction(m_spacer, action);
    m_icons.insert(action, svgPath);
    setupButton(action);
}

void PageBar::addBottomPage(QAction *action, const QString &svgPath)
{
    addAction(action);
    m_icons.insert(action, svgPath);
    setupButton(action);
}

void PageBar::setupButton(QAction *action)
{
    if (auto *b = qobject_cast<QToolButton *>(widgetForAction(action))) {
        b->setFixedWidth(ButtonW);
        b->setCursor(Qt::PointingHandCursor);
        b->setAutoRaise(true);
    }
    applyTheme();
}

void PageBar::changeEvent(QEvent *event)
{
    QToolBar::changeEvent(event);
    if (!m_styling && (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange))
        applyTheme();
}

/**
    @brief Цвета панели и значков — от палитры приложения (тема Windows).
    @note  Палитра приложения, а не виджета: таблица стилей самой панели меняет её палитру.
*/
void PageBar::applyTheme()
{
    if (m_styling)
        return;
    m_styling = true;

    const QPalette pal = QApplication::palette();
    QColor win = pal.color(QPalette::Window);
    win.setAlpha(255);
    const bool dark = win.lightness() < 128;

    QColor accent = pal.color(QPalette::Highlight);
    accent.setAlpha(255);
    if (accent.saturation() < 40)
        accent = dark ? QColor(0x4C, 0xA3, 0xF0) : QColor(0x00, 0x78, 0xD7);

    QColor text = pal.color(QPalette::WindowText);
    text.setAlpha(255);
    QColor muted = text;
    muted.setAlpha(170);
    const QColor iconNormal = dark ? text.darker(115) : text.lighter(160);

    QColor bar = dark ? win.lighter(112) : win.darker(104);
    QColor border = dark ? win.lighter(140) : win.darker(115);
    QColor hover = text;
    hover.setAlpha(22);
    QColor checked = accent;
    checked.setAlpha(dark ? 60 : 38);

    setStyleSheet(QStringLiteral(
        "QToolBar#pageBar { background: %1; border: none; border-right: 1px solid %2; padding: 6px 3px; spacing: 4px; }"
        "QToolBar#pageBar QToolButton { border: none; border-radius: 6px; padding: 6px 0 5px 0; color: %3; font-size: 8pt; }"
        "QToolBar#pageBar QToolButton:hover { background: %4; }"
        "QToolBar#pageBar QToolButton:checked { background: %5; color: %6; font-weight: 600; }")
                      .arg(bar.name(), border.name(), rgba(muted), rgba(hover), rgba(checked), accent.name()));

    // Значки: обычный — приглушённый текст, нажатая страница — акцент
    const qreal dpr = devicePixelRatioF();
    for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it) {
        QIcon icon;
        icon.addPixmap(tinted(it.value(), iconNormal, dpr), QIcon::Normal, QIcon::Off);
        icon.addPixmap(tinted(it.value(), accent, dpr), QIcon::Normal, QIcon::On);
        icon.addPixmap(tinted(it.value(), accent, dpr), QIcon::Active, QIcon::On);
        it.key()->setIcon(icon);
    }

    m_styling = false;
}
