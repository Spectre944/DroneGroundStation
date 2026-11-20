#ifndef SPECTRUMVIEW_H
#define SPECTRUMVIEW_H

#include <QWidget>
#include <QDateTime>

#include "backend/classes/core_types.h"

class QLabel;
class QCheckBox;
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
    void setSpectrum(const Spectrum &s);
    void setOverlay(const Spectrum &s);
    void clearOverlay();
    void clear();

private slots:
    void onRangeChanged(double from, double to);
    void onRangeEdited();
    void tick();

private:
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

    Spectrum  m_spectrum;
    bool      m_has = false;
    quint16   m_overlayNumber = 0;
    QDateTime m_seenAt;         ///< когда пришла первая порция этого спектра (часы ПК) — для хода набора
};

#endif // SPECTRUMVIEW_H
