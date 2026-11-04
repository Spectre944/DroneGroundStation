#pragma once

#include <QWidget>
#include <QMargins>
#include <QSizeF>
#include <QColor>

class QVBoxLayout;
class QLabel;
class QToolButton;

class OverlayContainer : public QWidget
{
    Q_OBJECT
public:
    explicit OverlayContainer(QWidget *parent);

    // Содержимое
    void setContentWidget(QWidget *widget);
    QWidget *contentWidget() const { return m_contentWidget; }
    void setContentMargins(int left, int top, int right, int bottom);

    // Расположение и размер
    void setAnchor(Qt::Alignment anchor);            // куда прижать внутри родителя
    void setOuterMargins(const QMargins &margins);   // отступы от краёв родителя
    void setStretch(Qt::Orientations orientations);  // растянуть по ширине/высоте
    void setRelativeSize(qreal w, qreal h);          // доля от родителя (0 = по содержимому)

    // Для страниц
    void setTitle(const QString &title);             // непустой заголовок включает шапку с кнопкой ✕
    void setModal(bool modal);                       // затемнение родителя, клик по фону/Esc закрывает

    // Оформление
    void setBackgroundColor(const QColor &color);
    void setBorderRadius(int radius);

public slots:
    void showOverlay();
    void hideOverlay();

signals:
    void closed();

protected:
    bool event(QEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

    // Не пропускаем мышь на карту под оверлеем
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void applyStyle();
    void updatePosition();

    QVBoxLayout *m_layout = nullptr;
    QWidget     *m_header = nullptr;
    QLabel      *m_titleLabel = nullptr;
    QToolButton *m_closeBtn = nullptr;
    QWidget     *m_contentWidget = nullptr;
    QWidget     *m_backdrop = nullptr;

    Qt::Alignment m_anchor = Qt::AlignLeft | Qt::AlignTop;
    QMargins      m_outer {10, 10, 10, 10};
    Qt::Orientations m_stretch;
    QSizeF        m_relSize {0, 0};
    bool          m_modal = false;
    bool          m_updating = false;

    QColor m_bg {35, 40, 50, 235};
    int    m_radius = 12;
};