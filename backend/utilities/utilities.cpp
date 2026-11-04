#include "utilities.h"


QString RadiationUtils::formatDoseRate(double der)
{
    // Возвращает текстовое значение мощности дозы
    if (der < 1000.0) {
        return QString::number(der, 'f', 2) + " мкЗв/год";
    } else if (der < 1000000.0) {
        return QString::number(der / 1000.0, 'f', 2) + " мЗв/год";
    } else {
        return QString::number(der / 1000000.0, 'f', 2) + " Зв/год";
    }
}

QString RadiationUtils::formatDoseValue(double der)
{
    // Возвращает текстовое значение дозы
    if (der < 1000.0) {
        return QString::number(der, 'f', 2) + " мкЗв";
    } else if (der < 1000000.0) {
        return QString::number(der / 1000.0, 'f', 2) + " мЗв";
    } else {
        return QString::number(der / 1000000.0, 'f', 2) + " Зв";
    }
}

QString TimeUtils::formatSeconds(int seconds)
{
    if (seconds < 60) {
        return QString("%1 сек. назад").arg(seconds);
    } else {
        int minutes = seconds / 60;
        return QString("%1 хв. назад").arg(minutes);
    }
}

QString TimeUtils::formatTimeValue(int seconds)
{
    if (seconds < 60) {
        return QString("%1 сек.").arg(seconds);
    } else if (seconds < 3600) {
        int minutes = seconds / 60;
        return QString("%1 хв.").arg(minutes);
    } else {
        int hours = seconds / 3600;
        int remainingMinutes = (seconds % 3600) / 60;

        if (remainingMinutes > 0) {
            return QString("%1 год. %2 хв.").arg(hours).arg(remainingMinutes);
        } else {
            return QString("%1 год.").arg(hours);
        }
    }
}

QString TimeUtils::formatSecToHHMMSS(int seconds)
{
    int totalSecs = seconds;
    int hours = totalSecs / 3600;
    int min = (totalSecs % 3600) / 60;
    int sec = totalSecs % 60;
    QString formatted = QString("%1:%2:%3")
                            .arg(hours, 2, 10, QLatin1Char('0'))
                            .arg(min,   2, 10, QLatin1Char('0'))
                            .arg(sec, 2, 10, QLatin1Char('0'));

    return formatted;
}


QIcon IconHelper::createThemedIcon(const QString &svgPath, const QSize &size)
{
    // Читаем SVG файл
    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QIcon();
    }

    QByteArray svgData = file.readAll();
    QString svgContent = QString::fromUtf8(svgData);

    // Получаем цвет из системной палитры
    QPalette palette = QApplication::palette();
    QColor textColor = palette.color(QPalette::WindowText);

    // Заменяем currentColor на актуальный цвет
    svgContent.replace("currentColor", textColor.name());

    // Создаем рендерер с модифицированным SVG
    QSvgRenderer renderer(svgContent.toUtf8());

    // Рендерим в пиксмап
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    // painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);

    return QIcon(pixmap);
}

QPixmap IconHelper::createThemedPixmap(const QString &svgPath, const QSize &size)
{
    // Читаем SVG файл
    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QByteArray svgData = file.readAll();
    QString svgContent = QString::fromUtf8(svgData);

    // Получаем цвет из системной палитры
    QPalette palette = QApplication::palette();
    QColor textColor = palette.color(QPalette::WindowText);

    // Заменяем currentColor на актуальный цвет
    svgContent.replace("currentColor", textColor.name());

    // Создаем рендерер с модифицированным SVG
    QSvgRenderer renderer(svgContent.toUtf8());

    // Рендерим в пиксмап
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    // painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);

    return pixmap;
}
