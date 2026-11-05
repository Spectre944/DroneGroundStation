#include "notifier.h"

#include <QApplication>
#include <QScreen>

Notifier::Notifier(QWidget *parent)
    : QFrame(parent) // Наследуем QFrame для добавления рамки
{
    hide();

    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setLineWidth(2);

    if (parent) {
        auto parentSize = parent->size();
        move((parentSize.width() - width()) / 2, 50);
    }

    gLayout = new QGridLayout(this);

    message = new QLabel(this);
    indicator = new IndicatorFeature(this);

    message->setAlignment(Qt::AlignCenter);
    message->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    indicator->setupLayout(this, 0, 1);
    gLayout->addWidget(message, 0, 0);
    gLayout->setSpacing(5);

    setStyleSheet("color: black; background-color: lightgray; border-radius: 5px;" );

    autoHideTimer = new QTimer(this);
    autoHideTimer->setSingleShot(true); // Одноразовый таймер
    connect(autoHideTimer, &QTimer::timeout, this, &Notifier::hide);
}

void Notifier::mousePressEvent(QMouseEvent *event)
{
    this->hide();
    QWidget::mousePressEvent(event);
}

void Notifier::showNotification(const QString &text, const QString &details, IndicatorType type)
{
    if (parentWidget()) {
        auto parentSize = parentWidget()->size();
        move((parentSize.width() - width()) / 2, 50);
    }

    QFontMetrics fm(message->font());
    int textWidth = fm.horizontalAdvance(text);
    int textHeight = fm.height();

    // Установить минимальные отступы и размеры виджета
    int padding = 50; // отступы вокруг текста
    int minWidth = 150; // минимальная ширина виджета
    int minHeight = 50; // минимальная высота виджета

    // Рассчитать размеры виджета
    int newWidth = std::max(minWidth, textWidth + padding);
    int newHeight = std::max(minHeight, textHeight);

    resize(newWidth, newHeight);

    message->setText(text);
    indicator->setIndicator(type, "");
    setToolTip(details);

    show();

    autoHideTimer->start(5000);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

NotificationWidget::NotificationWidget(const QString& message, QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    // Настройка внешнего вида
    setFixedSize(NotificationManager::NOTIFICATION_WIDTH,
                 NotificationManager::NOTIFICATION_HEIGHT);

    QString style = R"(
        QWidget {
            color: black;
            background-color: rgba(225, 225, 225, 200);
            border-radius: 8px;
            border: 1px solid rgba(255, 255, 255, 100);
        }
        QLabel {
            color: black;
            // font-size: 12px;
            padding: 10px;
            // background: transparent;
        }
    )";
    // setStyleSheet(style);

    // Создание лейбла с сообщением
    messageLabel = new QLabel(message, this);
    messageLabel->setWordWrap(true);
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setProperty("textSize", "sm");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(messageLabel);
    layout->setContentsMargins(10, 10, 10, 10);

    // Настройка анимации
    opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(opacityEffect);

    fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    fadeAnimation->setDuration(300);

    // Таймер для автоматического скрытия
    hideTimer = new QTimer(this);
    hideTimer->setSingleShot(true);
    connect(hideTimer, &QTimer::timeout, this, &NotificationWidget::hideNotification);
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

void NotificationWidget::showNotification()
{
    show();

    // Анимация появления
    fadeAnimation->setStartValue(0.0);
    fadeAnimation->setEndValue(1.0);
    fadeAnimation->start();

    // Запуск таймера для скрытия
    hideTimer->start(3000); // 3 секунды по умолчанию
}

void NotificationWidget::hideNotification()
{
    // Анимация исчезновения
    fadeAnimation->setStartValue(1.0);
    fadeAnimation->setEndValue(0.0);
    connect(fadeAnimation, &QPropertyAnimation::finished, this, &QWidget::deleteLater);
    fadeAnimation->start();
}

// Реализация NotificationManager
NotificationManager::NotificationManager(QObject* parent)
    : QObject(parent), duration(3000), maxNotifications(5)
{
}

NotificationManager& NotificationManager::instance()
{
    static NotificationManager instance;
    return instance;
}

void NotificationManager::showInfo(const QString& message)
{
    instance().showNotification(message, Info);
}

void NotificationManager::showWarning(const QString& message)
{
    instance().showNotification(message, Warning);
}

void NotificationManager::showError(const QString& message)
{
    instance().showNotification(message, Error);
}

void NotificationManager::showSuccess(const QString& message)
{
    instance().showNotification(message, Success);
}

void NotificationManager::showNotification(const QString& message, NotificationType type)
{
    // Ограничиваем количество активных уведомлений
    while (activeNotifications.size() >= maxNotifications) {
        NotificationWidget* oldest = activeNotifications.dequeue();
        oldest->deleteLater();
    }

    NotificationWidget* notification = new NotificationWidget(message);

    // Настройка стиля в зависимости от типа
    QString baseStyle = R"(
        QWidget {
            color: black;
            background-color: rgba(225, 225, 225, 200);
            border-radius: 8px;
            border: 4px solid rgba(%1, 200);
        }
        QLabel {
            color: black;
            // font-size: 14px;
            padding: 10px;
            // background: transparent;
        }
    )";

    QString style;
    switch (type) {
    case Info:
        style = baseStyle.arg("70, 130, 180"); // Синий
        break;
    case Warning:
        style = baseStyle.arg("255, 165, 0"); // Оранжевый
        break;
    case Error:
        style = baseStyle.arg("220, 53, 69"); // Красный
        break;
    case Success:
        style = baseStyle.arg("40, 167, 69"); // Зеленый
        break;
    }

    notification->setStyleSheet(style);

    // Добавляем в очередь активных уведомлений
    activeNotifications.enqueue(notification);

    // Дублируем сообщение в бокову панель уведомлений
    emit notificationAdded(message, type, QDateTime::currentDateTime());

    // Подключаем сигнал удаления
    connect(notification, &QObject::destroyed, this, &NotificationManager::onNotificationDestroyed);


    // Позиционируем и показываем
    positionNotifications();
    notification->findChild<QTimer*>()->setInterval(duration);
    notification->showNotification();
}

void NotificationManager::positionNotifications()
{
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int x = screenGeometry.right() - NOTIFICATION_WIDTH - MARGIN;
    int startY = screenGeometry.top() + TOP_SPACING + MARGIN;

    for (int i = 0; i < activeNotifications.size(); ++i) {
        NotificationWidget* notification = activeNotifications[i];
        int y = startY + i * (NOTIFICATION_HEIGHT + MARGIN);
        notification->move(x, y);
    }
}

void NotificationManager::onNotificationDestroyed()
{
    // Удаляем из очереди уничтоженные уведомления
    activeNotifications.removeAll(static_cast<NotificationWidget*>(sender()));

    // Переставляем оставшиеся уведомления
    positionNotifications();
}
