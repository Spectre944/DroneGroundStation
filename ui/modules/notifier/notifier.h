#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QWidget>
#include <QIcon>
#include <QFrame>
#include <QGridLayout>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QQueue>
#include <QDateTime>

#include <ui/components/custom/custom.h>


class Notifier : public QFrame
{
    Q_OBJECT
public:
    explicit Notifier(QWidget *parent = nullptr);

public slots:
    void showNotification(const QString &text, const QString &details, IndicatorType type);

signals:

private:
    IndicatorFeature    *indicator;
    QLabel              *message;
    QGridLayout         *gLayout;
    QTimer              *autoHideTimer;


    // QWidget interface
protected:
    void mousePressEvent(QMouseEvent *event) override;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class NotificationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NotificationWidget(const QString& message, QWidget* parent = nullptr);
    void showNotification();

private slots:
    void hideNotification();

private:
    QLabel* messageLabel;
    QLabel* iconLabel;
    QTimer* hideTimer;
    QPropertyAnimation* fadeAnimation;
    QGraphicsOpacityEffect* opacityEffect;
};

//--------------------------------------------------------------------//
//                                                                    //
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

    static NotificationManager& instance();

    // Основные методы для показа уведомлений
    static void showInfo(const QString& message);
    static void showWarning(const QString& message);
    static void showError(const QString& message);
    static void showSuccess(const QString& message);

    // Настройки
    void setDuration(int milliseconds) { duration = milliseconds; }
    void setMaxNotifications(int max) { maxNotifications = max; }

    static const int NOTIFICATION_WIDTH = 300;
    static const int NOTIFICATION_HEIGHT = 100;
    static const int MARGIN = 10;
    static const int TOP_SPACING = 60;


signals:
    // Сигнал для боковой панели уведомлений
    void notificationAdded(const QString& message, NotificationType type, const QDateTime& timestamp);

private:
    explicit NotificationManager(QObject* parent = nullptr);
    ~NotificationManager() = default;

    void showNotification(const QString& message, NotificationType type);
    void positionNotifications();

private slots:
    void onNotificationDestroyed();

private:
    QQueue<NotificationWidget*> activeNotifications;
    int duration;
    int maxNotifications;

};

#endif // NOTIFIER_H
