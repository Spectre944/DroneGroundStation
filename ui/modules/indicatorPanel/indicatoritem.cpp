#include "indicatoritem.h"

#include <QApplication>
#include <QEvent>
#include <QEnterEvent>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace {

QPointer<IndicatorTip> s_tip;
constexpr int kTipWidth = 340;

/// Цвет «приглушённого» текста: смесь текста и фона.
QColor mutedText(const QPalette &pal, QPalette::ColorRole fg, QPalette::ColorRole bg)
{
    const QColor a = pal.color(fg), b = pal.color(bg);
    return QColor((a.red() * 6 + b.red() * 4) / 10,
                  (a.green() * 6 + b.green() * 4) / 10,
                  (a.blue() * 6 + b.blue() * 4) / 10);
}

} // namespace


/////////////////////////////////// IndicatorItem ///////////////////////////////////

IndicatorItem::IndicatorItem(const QString &name, QWidget *parent)
    : QWidget(parent)
    , m_name(name)
{
    setFixedSize(32, 32);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

IndicatorItem::~IndicatorItem()
{
    IndicatorTip::hideFor(this);
}

void IndicatorItem::setName(const QString &name)
{
    m_name = name;
    IndicatorTip::refresh(this);
}

void IndicatorItem::setStates(const QVector<IndicatorState> &states)
{
    m_states = states;
    m_cache.clear();
    update();
    IndicatorTip::refresh(this);
}

void IndicatorItem::setState(int id, const QString &detail)
{
    if (m_current == id && m_detail == detail)
        return;
    m_current = id;
    m_detail = detail;
    update();
    IndicatorTip::refresh(this);
}

const IndicatorState *IndicatorItem::currentState() const
{
    for (const IndicatorState &s : m_states)
        if (s.id == m_current)
            return &s;
    return nullptr;
}

QPixmap IndicatorItem::renderSvg(const QString &path, const QColor &color, int px, qreal dpr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QPixmap();

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("currentColor"), color.name());

    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pm(QSize(px, px) * dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        renderer.render(&p);
    }
    pm.setDevicePixelRatio(dpr);
    return pm;
}

QPixmap IndicatorItem::iconFor(const IndicatorState &s, int px)
{
    const QColor c = s.color.isValid() ? s.color : palette().color(QPalette::WindowText);
    const qreal dpr = devicePixelRatioF();
    const QString key = QStringLiteral("%1|%2|%3|%4").arg(s.icon, c.name()).arg(px).arg(dpr);
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd())
        return *it;
    const QPixmap pm = renderSvg(s.icon, c, px, dpr);
    m_cache.insert(key, pm);
    return pm;
}

bool IndicatorItem::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {     // системная задержка подсказки — как у QToolTip
        IndicatorTip::showFor(this);
        return true;
    }
    return QWidget::event(e);
}

void IndicatorItem::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_hover) {
        QColor bg = palette().color(QPalette::Highlight);
        bg.setAlpha(45);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 7, 7);
    }

    if (const IndicatorState *s = currentState()) {
        const QPixmap pm = iconFor(*s, 20);
        p.drawPixmap((width() - 20) / 2, (height() - 20) / 2, pm);
    }
}

void IndicatorItem::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();
    QWidget::enterEvent(e);
}

void IndicatorItem::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();
    IndicatorTip::hideFor(this);
    QWidget::leaveEvent(e);
}

void IndicatorItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) {
        IndicatorTip::hideFor(this);
        emit clicked();
    }
    QWidget::mouseReleaseEvent(e);
}

void IndicatorItem::changeEvent(QEvent *e)
{
    // Сменилась тема — иконки без явного цвета перекрашиваются в новый цвет текста
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange) {
        m_cache.clear();
        update();
    }
    QWidget::changeEvent(e);
}


/////////////////////////////////// IndicatorTip ///////////////////////////////////

IndicatorTip::IndicatorTip()
    : QFrame(nullptr, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName(QStringLiteral("IndicatorTip"));
    setFrameShape(QFrame::StyledPanel);
    auto *lay = new QVBoxLayout(this);
    // Layout не должен сам задавать окну минимальный размер: он считает высоту
    // без переноса строк, и Windows ругается «Unable to set geometry». Размер задаём в rebuild().
    lay->setSizeConstraint(QLayout::SetNoConstraint);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(6);
}

void IndicatorTip::showFor(IndicatorItem *item)
{
    if (!item)
        return;
    if (!s_tip)
        s_tip = new IndicatorTip();
    s_tip->m_owner = item;
    s_tip->rebuild(item);
    s_tip->placeNear(item);
    s_tip->show();
    s_tip->raise();
}

void IndicatorTip::hideFor(IndicatorItem *item)
{
    if (s_tip && s_tip->m_owner == item) {
        s_tip->hide();
        s_tip->m_owner = nullptr;
    }
}

void IndicatorTip::refresh(IndicatorItem *item)
{
    if (s_tip && s_tip->isVisible() && s_tip->m_owner == item) {
        s_tip->rebuild(item);
        s_tip->placeNear(item);
    }
}

void IndicatorTip::rebuild(IndicatorItem *item)
{
    // Палитра подсказки — от приложения, как у QToolTip
    QPalette pal = QApplication::palette();
    setPalette(pal);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::ToolTipBase);
    setForegroundRole(QPalette::ToolTipText);

    const QColor text  = pal.color(QPalette::ToolTipText);
    const QColor muted = mutedText(pal, QPalette::ToolTipText, QPalette::ToolTipBase);
    // Подсветка текущей строки — непрозрачная смесь фона и Highlight
    // (полупрозрачный фон дочернего виджета на окне Qt::ToolTip рисуется не везде)
    const QColor base = pal.color(QPalette::ToolTipBase), sel = pal.color(QPalette::Highlight);
    const QColor hl((base.red() * 4 + sel.red()) / 5, (base.green() * 4 + sel.green()) / 5,
                    (base.blue() * 4 + sel.blue()) / 5);

    // Удаляем старое содержимое
    QLayout *lay = layout();
    while (QLayoutItem *li = lay->takeAt(0)) {
        delete li->widget();
        delete li;
    }

    const IndicatorState *cur = item->currentState();

    auto *head = new QLabel(this);
    head->setTextFormat(Qt::RichText);
    head->setText(QStringLiteral("<b>%1</b>%2")
                      .arg(item->name().toHtmlEscaped(),
                           cur ? QStringLiteral(": ") + cur->title.toHtmlEscaped() : QString()));
    lay->addWidget(head);

    if (!item->detail().isEmpty()) {
        auto *det = new QLabel(item->detail(), this);
        det->setWordWrap(true);
        QPalette dp = det->palette();
        dp.setColor(QPalette::WindowText, text);
        det->setPalette(dp);
        lay->addWidget(det);
    }

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    QPalette lp = line->palette();
    lp.setColor(QPalette::WindowText, muted);
    line->setPalette(lp);
    lay->addWidget(line);

    const qreal dpr = devicePixelRatioF();
    for (const IndicatorState &s : item->states()) {
        auto *row = new QWidget(this);
        auto *g = new QGridLayout(row);
        g->setContentsMargins(6, 4, 6, 4);
        g->setHorizontalSpacing(10);
        g->setVerticalSpacing(0);

        const bool isCur = cur && cur->id == s.id;
        if (isCur) {
            row->setAutoFillBackground(true);
            row->setBackgroundRole(QPalette::Window);   // иначе роль наследуется от окна (ToolTipBase)
            QPalette rp = row->palette();
            rp.setColor(QPalette::Window, hl);
            row->setPalette(rp);
        }

        auto *ico = new QLabel(row);
        ico->setPixmap(IndicatorItem::renderSvg(s.icon, s.color.isValid() ? s.color : text, 20, dpr));
        ico->setFixedSize(22, 22);
        g->addWidget(ico, 0, 0, 2, 1, Qt::AlignTop);

        auto *ttl = new QLabel(row);
        ttl->setTextFormat(Qt::RichText);
        ttl->setText(isCur ? QStringLiteral("<b>%1</b> ◂").arg(s.title.toHtmlEscaped())
                           : s.title.toHtmlEscaped());
        g->addWidget(ttl, 0, 1);

        if (!s.description.isEmpty()) {
            auto *ds = new QLabel(s.description, row);
            ds->setWordWrap(true);
            QFont f = ds->font();
            f.setPointSizeF(f.pointSizeF() * 0.9);
            ds->setFont(f);
            QPalette dp = ds->palette();
            dp.setColor(QPalette::WindowText, muted);
            ds->setPalette(dp);
            g->addWidget(ds, 1, 1);
        }
        g->setColumnStretch(1, 1);
        lay->addWidget(row);
    }

    // Если подсказка уже на экране (повторное наведение, обновление раз в секунду),
    // новые виджеты layout показал бы только в следующем цикле событий — тогда они
    // считаются нулевыми, и окно схлопывается. Показываем их сразу, до расчёта высоты.
    const auto children = findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *w : children)
        w->show();

    // Высота — с учётом переноса строк при ширине kTipWidth; min = max, чтобы ОС не пересчитывала
    lay->activate();
    const int h = lay->hasHeightForWidth() ? lay->totalHeightForWidth(kTipWidth)
                                           : lay->totalSizeHint().height();
    setFixedSize(kTipWidth, h);
}

void IndicatorTip::placeNear(IndicatorItem *item)
{
    const QPoint below = item->mapToGlobal(QPoint(0, item->height() + 6));
    QScreen *scr = item->screen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);

    QPoint pos = below;
    // Панель в правом верхнем углу — прижимаем подсказку к правому краю экрана
    if (pos.x() + width() > avail.right())
        pos.setX(qMax(avail.left(), avail.right() - width()));
    if (pos.y() + height() > avail.bottom())
        pos.setY(item->mapToGlobal(QPoint(0, 0)).y() - height() - 6);
    move(pos);
}