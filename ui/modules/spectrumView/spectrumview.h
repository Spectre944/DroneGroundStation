#ifndef SPECTRUMVIEW_H
#define SPECTRUMVIEW_H

#include <QWidget>
#include <QDateTime>

#include "backend/classes/core_types.h"

class QLabel;
class QCheckBox;
class QComboBox;
class QStackedWidget;
class WaterfallPlot;
class QPushButton;
class QSpinBox;
class QProgressBar;
class QTimer;
class SpectrumPlot;

/**
    @brief Окно спектра: заголовок, ход набора, переключатели, график, диапазон каналов.
    @note  Основа — WinSpectr.qml (ThermalQML). Подключение:
             connect(core, &DroneCore::spectrumUpdated,  view, &SpectrumView::setSpectrum);
             connect(core, &DroneCore::spectrumFinished, view, &SpectrumView::setSpectrum);
           Для сравнения — setOverlay() с другим спектром полёта (пунктир).

           Р2-14 — спектры полёта:
             «Спектр» — список законченных спектров полёта (и «Поточний» — идущий набор);
             «Порівняти з» — наложение другого спектра пунктиром (setOverlay);
             «Водоспад» — порции набора по времени × каналы (WaterfallPlot).
           Источники списка:
             текущий полёт — законченные спектры из setSpectrum (spectrumFinished), сброс — clearLive;
             прошлый полёт — showFlightSpectra(SessionData::spectra), возврат — showLive.
           Пока смотрят законченный или архивный спектр, новые порции идущего набора
           копятся, но окно на них не перескакивает — над графиком подсказка.
           Щелчок по значку спектра на карте — openSpectrum(); показанный спектр —
           сигнал spectrumShown (подсветить значок на карте).
*/
class SpectrumView : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumView(QWidget *parent = nullptr);

    SpectrumPlot *plot() const { return m_plot; }
    bool hasSpectrum() const   { return m_has; }
    const Spectrum &spectrum() const { return m_spectrum; }

public slots:
    void setSpectrum(const Spectrum &s);        ///< живой набор: spectrumUpdated / spectrumFinished
    void clearLive();                           ///< новый полёт — список текущих спектров заново
    void showFlightSpectra(const QVector<Spectrum> &list, const QString &flightName);  ///< прошлый полёт
    void showLive();                            ///< вернуться к текущему полёту
    void openSpectrum(qint64 id, quint16 number, const QDateTime &startTime);   ///< значок на карте
    void setOverlay(const Spectrum &s);
    void clearOverlay();
    void clear();

signals:
    void spectrumShown(qint64 id, quint16 number);   ///< показан спектр — подсветить значок на карте

private slots:
    void onRangeChanged(double from, double to);
    void onRangeEdited();
    void tick();

private:
    void display(const Spectrum &s);            ///< показать спектр (прежнее тело setSpectrum)
    void fillLists();                           ///< списки «Спектр» и «Порівняти з»
    void pick(int index);                       ///< -1 — «Поточний», иначе номер в m_list
    void updateLiveHint();
    QString itemText(const Spectrum &s) const;
    static bool sameSpectrum(const SpectrumInfo &a, const SpectrumInfo &b);
    void updateHeader();
    void updateInfo();
    QString stopReasonText(SpectrumStopReason r) const;

    SpectrumPlot *m_plot     = nullptr;
    QLabel       *m_title    = nullptr;
    QLabel       *m_state    = nullptr;
    QProgressBar *m_progress = nullptr;
    QCheckBox    *m_log      = nullptr;
    QPushButton  *m_reset    = nullptr;
    QPushButton  *m_clear    = nullptr;
    QSpinBox     *m_from     = nullptr;
    QSpinBox     *m_to       = nullptr;
    QTimer       *m_timer    = nullptr;
    QComboBox    *m_pick     = nullptr;     ///< «Спектр»
    QComboBox    *m_cmp      = nullptr;     ///< «Порівняти з»
    QPushButton  *m_wfButton = nullptr;     ///< «Водоспад»
    QLabel       *m_liveHint = nullptr;     ///< «йде набір спектра №N»
    QStackedWidget *m_stack  = nullptr;     ///< график / водопад
    WaterfallPlot  *m_waterfall = nullptr;

    QVector<Spectrum> m_list;               ///< законченные спектры в списке (текущий или прошлый полёт)
    QVector<Spectrum> m_liveList;           ///< законченные спектры текущего полёта
    Spectrum  m_live;                       ///< идущий (последний) спектр текущего полёта, все порции
    bool      m_hasLive = false;
    bool      m_archive = false;            ///< в списке — прошлый полёт
    QString   m_flightName;
    int       m_showing = -1;               ///< -1 — «Поточний», иначе номер в m_list

    Spectrum  m_spectrum;
    bool      m_has = false;
    quint16   m_overlayNumber = 0;
    QDateTime m_seenAt;         ///< когда пришла первая порция этого спектра (часы ПК) — для хода набора
};

#endif // SPECTRUMVIEW_H
