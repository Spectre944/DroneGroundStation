#ifndef UTILITIES_H
#define UTILITIES_H

#include <QString>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QApplication>
#include <QPalette>
#include <QSvgRenderer>
#include <QByteArray>
#include <QFile>

class RadiationUtils {
public:
    static QString formatDoseRate(double der);
    static QString formatDoseValue(double der);

};

class TimeUtils{
public:
    static QString formatSeconds(int seconds);
    static QString formatTimeValue(int seconds);
    static QString formatSecToHHMMSS(int seconds);
};

class IconHelper
{
public:
    static QIcon createThemedIcon(const QString& svgPath, const QSize& size = QSize(40, 40));
    static QPixmap createThemedPixmap(const QString& svgPath, const QSize& size = QSize(32, 32));
};

#endif // UTILITIES_H
