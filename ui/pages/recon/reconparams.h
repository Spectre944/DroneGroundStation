#ifndef RECONPARAMS_H
#define RECONPARAMS_H

/**
    @file  reconparams.h
    @brief Параметры разведки команды ID04 (кроме вершин) и их типовые значения.
    @note  Типовые значения задаются на странице «Налаштування» (раздел «Розвідка»)
           и подставляются на страницу «Розвідка». Связь односторонняя: правка на
           странице «Розвідка» относится к одному полёту и в настройки не пишется.
           Значения по умолчанию — предварительные (в протоколе не заданы), уточнить у Андрея.
*/

#include <QVariant>
#include "backend/classes/core_types.h"
#include "backend/settings/settings.h"

struct ReconParams {
    ReconMode reconMode = ReconMode::Distance;  ///< 0 — за командою, 1 — за часом, 2 — за відстанню
    int       intervalS = 60;                   ///< інтервал між вимірюваннями, с (> 0)
    int       distanceM = 100;                  ///< відстань між вимірюваннями, м (> 0)
    int       dupFactor = 2;                    ///< коефіцієнт дублювання пакетів, 2–5
    int       spectrumS = 0;                    ///< час набору спектра, с; 0 — до команди ID12

    // Ключи в файле настроек
    static constexpr char KEY_MODE[]     = "recon/mode";
    static constexpr char KEY_INTERVAL[] = "recon/intervalS";
    static constexpr char KEY_DISTANCE[] = "recon/distanceM";
    static constexpr char KEY_DUP[]      = "recon/dupFactor";
    static constexpr char KEY_SPECTRUM[] = "recon/spectrumS";

    // Допустимые диапазоны (ID04: uint16 / uint8)
    static constexpr int INTERVAL_MIN = 1, INTERVAL_MAX = 65535;
    static constexpr int DISTANCE_MIN = 1, DISTANCE_MAX = 65535;
    static constexpr int DUP_MIN      = 2, DUP_MAX      = 5;
    static constexpr int SPECTRUM_MIN = 0, SPECTRUM_MAX = 65535;

    /// Типовые значения из настроек; отсутствующий ключ — значение по умолчанию
    static ReconParams load()
    {
        const Settings &s = Settings::instance();
        ReconParams d, p;
        const int mode = s.getValue(KEY_MODE, int(d.reconMode)).toInt();
        p.reconMode = (mode >= 0 && mode <= 2) ? ReconMode(mode) : d.reconMode;
        p.intervalS = qBound(INTERVAL_MIN, s.getValue(KEY_INTERVAL, d.intervalS).toInt(), INTERVAL_MAX);
        p.distanceM = qBound(DISTANCE_MIN, s.getValue(KEY_DISTANCE, d.distanceM).toInt(), DISTANCE_MAX);
        p.dupFactor = qBound(DUP_MIN,      s.getValue(KEY_DUP,      d.dupFactor).toInt(), DUP_MAX);
        p.spectrumS = qBound(SPECTRUM_MIN, s.getValue(KEY_SPECTRUM, d.spectrumS).toInt(), SPECTRUM_MAX);
        return p;
    }

    /// Записать в настройки (без saveSettings — сохраняет вызывающий)
    void store() const
    {
        Settings &s = Settings::instance();
        s.setValue(KEY_MODE,     int(reconMode));
        s.setValue(KEY_INTERVAL, intervalS);
        s.setValue(KEY_DISTANCE, distanceM);
        s.setValue(KEY_DUP,      dupFactor);
        s.setValue(KEY_SPECTRUM, spectrumS);
    }

    bool operator==(const ReconParams &o) const
    {
        return reconMode == o.reconMode && intervalS == o.intervalS && distanceM == o.distanceM
            && dupFactor == o.dupFactor && spectrumS == o.spectrumS;
    }
    bool operator!=(const ReconParams &o) const { return !(*this == o); }
};

#endif // RECONPARAMS_H
