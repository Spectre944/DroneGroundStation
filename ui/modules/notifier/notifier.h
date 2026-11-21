#ifndef NOTIFIER_H
#define NOTIFIER_H

// ============================================================================
//  NotificationManager - всплывающие уведомления ("тосты").
//
//      NotificationManager::showInfo("Налаштування збережені");
//      NotificationManager::showError("Немає зв'язку з дроном", "Радіолінія");
//
//  - Тосты - дочерние виджеты главного окна (не отдельные окна): двигаются
//    вместе с ним, не попадают на чужой монитор и не воруют фокус.
//  - Стопка внизу по центру окна, новые снизу, не больше maxNotifications.
//  - Одинаковое сообщение не дублируется: у существующего растёт счётчик "×N".
//  - Наведение мыши ставит таймер на паузу, клик - закрывает.
//  - Цвета берутся из палитры окна (светлая / тёмная тема), рисуется вручную,
//    поэтому таблицы стилей родителей на вид не влияют.
// ============================================================================

#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QPointer>
#include <QWidget>

class QTimer;
class QVariantAnimation;
class QPropertyAnimation;

class NotificationManager;

class ToastWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
public:
    ToastWidget(int type, const QString &title, const QString &message, int durationMs, QWidget *host);

    int     type() const    { return m_type; }
    QString title() const   { return m_title; }
    QString message() const { return m_message; }
    bool    isClosing() const { return m_closing; }

    void bump();                         // повтор того же сообщения: ×N и таймер заново
    void appear();                       // показать с анимацией
    void dismiss();                      // скрыть с анимацией и удалить
    void moveAnimated(const QPoint &pos);

    qreal opacity() const { return m_opacity; }
    void  setOpacity(qreal o) { m_opacity = o; update(); }

    static constexpr int WIDTH = 380;

signals:
    void dismissed(ToastWidget *toast);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void  relayout();
    void  tick();
    QRect closeRect() const;
    qreal progress() const;              // 1 -> 0 оставшееся время

    int     m_type;
    QString m_title;
    QString m_message;
    int     m_count = 1;

    int           m_duration;
    qint64        m_elapsedBefore = 0;   // накоплено до паузы
    QElapsedTimer m_clock;
    bool          m_hovered = false;
    bool          m_closeHover = false;
    bool          m_closing = false;

    qreal m_opacity = 0.0;
    QTimer *m_ticker = nullptr;
    QVariantAnimation  *m_fade = nullptr;
    QPropertyAnimation *m_move = nullptr;
};

//--------------------------------------------------------------------//

class NotificationManager : public QObject
{
    Q_OBJECT

public:
    enum NotificationType {
        Info,
        Warning,
        Error,
        Success
    };
    Q_ENUM(NotificationType)

    static NotificationManager& instance();

    // Основные методы. title - необязательный заголовок (жирным над текстом).
    static void showInfo(const QString& message, const QString& title = QString());
    static void showWarning(const QString& message, const QString& title = QString());
    static void showError(const QString& message, const QString& title = QString());
    static void showSuccess(const QString& message, const QString& title = QString());
    static void show(NotificationType type, const QString& message,
                     const QString& title = QString(), int durationMs = -1);

    // Окно, в котором показываются тосты. По умолчанию - главное окно приложения.
    void setHost(QWidget *host);

    // Базовая длительность (информация); предупреждения x1.5, ошибки x2
    void setDuration(int milliseconds) { duration = qMax(1000, milliseconds); }
    void setMaxNotifications(int max)  { maxNotifications = qMax(1, max); }
    void clear();

    static QColor typeColor(NotificationType type);

signals:
    // Сигнал для боковой панели уведомлений / журнала
    void notificationAdded(const QString& message, NotificationType type, const QDateTime& timestamp);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit NotificationManager(QObject* parent = nullptr);
    ~NotificationManager() override = default;

    void showNotification(const QString& message, const QString& title, NotificationType type, int durationMs);
    QWidget *resolveHost();
    void positionNotifications(bool animated = true);
    void onToastDismissed(ToastWidget *toast);

    QPointer<QWidget> host;
    QList<QPointer<ToastWidget>> activeNotifications;   // старые -> новые
    int duration = 4000;
    int maxNotifications = 4;

    static constexpr int MARGIN = 16;     // от края окна
    static constexpr int SPACING = 8;     // между тостами
};

#endif // NOTIFIER_H
