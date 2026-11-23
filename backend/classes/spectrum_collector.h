#ifndef SPECTRUM_COLLECTOR_H
#define SPECTRUM_COLLECTOR_H

/**
    @file  spectrum_collector.h
    @brief Спектры (Ф10): сбор порций ID23 в накопленный спектр.

    Логгер присылает ID27 (начало набора) и порции ID23 (~раз в 6 с, 1024 канала).
    Итоговый спектр — сумма порций; в эфир логгер его не передаёт, только пишет на SD.
    Здесь сумма считается так же, как в логгере, — с насыщением 65535 в канале,
    иначе итог не совпадёт с SD при сверке (Ф13).

    @note  Конец набора замечает LoggerState (ID22, ID01 на ID12 / ID08 / ID04, авария)
           и сам SpectrumCollector: канал дошёл до 55705 или истекло заданное время.
    @note  Живёт в потоке ядра, в CoreEngine.
*/

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

#include "core_types.h"

class SpectrumCollector : public QObject
{
    Q_OBJECT

public:
    explicit SpectrumCollector(QObject *parent = nullptr);

    /**
        @brief ID27 — начало набора. Прежний набор, если не закрыт, закрывается (NewSpectrum).
    */
    void onStart(const SpectrumInfo &info);

    /**
        @brief ID23 — порция: прибавить к сумме с насыщением 65535.
        @note  Порция без ID27 (ID27 потерялся в эфире) — заводится спектр без номера.
    */
    void onPortion(const SpectrumPortion &portion);

    /**
        @brief Закончить набор (от LoggerState::spectrumFinished).
        @note  NewSpectrum здесь не обрабатывается: новый ID27 закрывает прежний набор в onStart.
               Набора нет — ничего не делает.
    */
    void finish(SpectrumStopReason reason);

    /**
        @brief Заданное время набора, с (из принятых ID04 и ID11); 0 — до ID12.
        @note  Действует со следующего набора — как в логгере (протокол, п. 4.12).
    */
    void setDuration(quint16 seconds);

    void reset();                                   ///< сброс без сохранения — новый сеанс связи
    bool isCollecting() const { return m_collecting; }
    const Spectrum &current() const { return m_current; }

    void setTimeMarginMs(int ms) { m_marginMs = ms; }   ///< запас к заданному времени; по умолчанию 10 с (в проверке — меньше)

signals:
    void spectrumUpdated(const Spectrum &spectrum);     ///< после каждой порции: сумма + только последняя порция
    void spectrumFinished(const Spectrum &spectrum);    ///< набор окончен: итог, причина, все порции
    void stoppedByItself(SpectrumStopReason reason);    ///< конец по 55705 или по времени → LoggerState

private:
    void begin(const SpectrumInfo &info);
    void close(SpectrumStopReason reason);
    void onTimer();

    Spectrum      m_current;
    bool          m_collecting = false;
    quint16       m_nextDurationS = 60;     ///< для следующего набора; до первой ID04 логгер набирает 60 с
    QElapsedTimer m_since;                  ///< время от начала набора
    QTimer       *m_timer = nullptr;        ///< раз в секунду — проверка заданного времени
    int           m_marginMs = 10000;
};

#endif // SPECTRUM_COLLECTOR_H
