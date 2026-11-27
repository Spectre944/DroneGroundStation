#ifndef SPECTRUMLAYER_H
#define SPECTRUMLAYER_H

/**
    @file  spectrumlayer.h
    @brief Значки спектров на карте (Р2-14): точка набора спектра — info.lat / lon.
    @note  Значок отличается от точки измерения: скруглённый квадрат цвета спектра
           (StatusColor::spectrum) с белыми «столбиками гистограммы» и номером спектра под ним.
             - идёт набор — светлый значок с цветной рамкой;
             - с SD-карты — тёмная обводка (как квадратные точки измерений с SD);
             - выбранный — кольцо вокруг.
           Наведение — карточка: номер, время, длительность, порции, ПАЕД, высота, причина конца.
           Щелчок — сигнал spectrumClicked(id, number): MainWindow открывает спектр
           на странице «Прилади».
           Данные: текущий полёт — upsert() по spectrumUpdated / spectrumFinished;
           прошлый полёт — setSpectra() из SessionData::spectra (MapWidget::showSessionView).
*/

#include <QDateTime>
#include <QPointF>
#include <QVector>

#include "maplayer.h"
#include "classes/core_types.h"

class SpectrumLayer : public MapLayer
{
    Q_OBJECT
public:
    explicit SpectrumLayer(QObject *parent = nullptr);

    void setSpectra(const QVector<SpectrumInfo> &list);
    void upsert(const SpectrumInfo &info);      ///< новый или обновлённый спектр (тот же номер и время начала)
    void clear();
    void select(qint64 id, quint16 number);     ///< подсветить (спектр открыт на «Прилади»); 0/0 — снять

    const QVector<SpectrumInfo> &spectra() const { return m_items; }

    void draw(QPainter &p) override;
    bool mouseMove(QMouseEvent *e) override;
    bool mouseRelease(QMouseEvent *e) override;
    void mouseLeave() override;

signals:
    void spectrumClicked(qint64 id, quint16 number, const QDateTime &startTime);

private:
    int  hitTest(const QPoint &pos) const;
    static bool same(const SpectrumInfo &a, const SpectrumInfo &b);
    void drawIcon(QPainter &p, const QPointF &c, const SpectrumInfo &s, bool selected, bool hover) const;
    void drawCard(QPainter &p, const QPointF &c, const SpectrumInfo &s, const QRect &bounds) const;

    QVector<SpectrumInfo> m_items;
    int m_hover = -1;
    int m_selected = -1;
};

#endif // SPECTRUMLAYER_H
