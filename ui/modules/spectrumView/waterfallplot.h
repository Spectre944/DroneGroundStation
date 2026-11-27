#ifndef WATERFALLPLOT_H
#define WATERFALLPLOT_H

/**
    @file  waterfallplot.h
    @brief «Водоспад» спектра (Р2-14): порции набора по времени (сверху вниз) × каналы.
    @note  Строка — одна порция (SpectrumPortion::channels — импульсы этой порции, не сумма),
           цвет — число импульсов в канале: тёмно-синий → фиолетовый → оранжевый → жёлтый.
           Видно, как набирался спектр: появилась ли линия сразу, менялся ли фон по ходу
           (например, дрон снесло с точки).
           Диапазон каналов — тот же, что у графика спектра (setChannelRange);
           логарифмическая шкала цвета — та же галочка, что у графика.
           Наведение — номер порции, время приёма, канал и число импульсов.
*/

#include <QImage>
#include <QList>
#include <QWidget>

#include "backend/classes/core_types.h"

class WaterfallPlot : public QWidget
{
    Q_OBJECT
public:
    explicit WaterfallPlot(QWidget *parent = nullptr);

    void setPortions(const QList<SpectrumPortion> &portions);
    void setChannelRange(int from, int to);
    void setLogScale(bool on);
    void clear();

    QSize minimumSizeHint() const override { return QSize(300, 200); }

protected:
    void paintEvent(QPaintEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    QRectF plotRect() const;
    void   rebuild();                   ///< картинка по порциям, диапазону и шкале
    static QRgb colorAt(double k);      ///< 0..1 → цвет палитры

    QList<SpectrumPortion> m_portions;
    int    m_from = 0;
    int    m_to = CoreConst::SpectrumChannels - 1;
    bool   m_log = true;
    QImage m_image;                     ///< ширина — каналы диапазона, высота — порции
    quint16 m_max = 0;
    QPoint m_mouse{-1, -1};
};

#endif // WATERFALLPLOT_H
