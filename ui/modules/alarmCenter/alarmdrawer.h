#ifndef ALARMDRAWER_H
#define ALARMDRAWER_H

#include <QObject>
#include <QWidget>
#include <QPointer>

#include "alarmcenter.h"

class QTimer;
class QPropertyAnimation;
class QVariantAnimation;

/**
    @brief Язычок шторки у правого края карты: колокольчик и число новых тревог.
*/
class AlarmEdgeHandle : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmEdgeHandle(QWidget *parent = nullptr);

    void setCount(int n);
    void setFlash(qreal f);     ///< 0..1 — подсветка при новой тревоге

    QSize sizeHint() const override { return QSize(32, 132); }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    int   m_count = 0;
    qreal m_flash = 0.0;
    bool  m_hover = false;
};

/**
    @brief Шторка тревог: язычок появляется, когда мышь у правого края карты,
           по нажатию выезжает AlarmCenter.
    @note  Пока есть непросмотренные тревоги, язычок виден всегда.
           Новая тревога — язычок мигает и показывается тост (NotificationManager).
           Закрыть: ✕, Esc или щелчок по карте.
*/
class AlarmDrawer : public QObject
{
    Q_OBJECT

public:
    explicit AlarmDrawer(QWidget *host);    ///< host — виджет карты

    AlarmCenter *center() const { return m_center; }
    bool isOpen() const         { return m_open; }

public slots:
    void open();
    void close();
    void toggle();

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    void pollCursor();
    void updateHandle(bool animate = true);
    void placeWidgets();
    QRect panelRect() const;
    QPoint handlePos(bool shown) const;
    void onAlarmAdded(const Alarm &a);

    QPointer<QWidget>  m_host;
    AlarmEdgeHandle   *m_handle = nullptr;
    AlarmCenter       *m_center = nullptr;

    QTimer            *m_poll      = nullptr;
    QTimer            *m_hideDelay = nullptr;
    QPropertyAnimation *m_handleAnim = nullptr;
    QPropertyAnimation *m_panelAnim  = nullptr;
    QVariantAnimation  *m_flashAnim  = nullptr;

    bool m_open = false;
    bool m_hover = false;       ///< мышь у края или на язычке
    bool m_handleShown = false;
};

#endif // ALARMDRAWER_H
