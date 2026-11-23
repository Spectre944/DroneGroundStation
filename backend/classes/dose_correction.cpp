#include "dose_correction.h"
#include "recon_area.h"

#include <cmath>

void DoseCorrection::setSettings(const CoreSettings &s)
{
    m_on   = s.correctionOn;
    m_mu   = s.correctionCoeffs.isEmpty() ? 0.0 : s.correctionCoeffs[0];
    m_minM = s.correctionMinM;
    m_maxM = s.correctionMaxM;
}

Measurement DoseCorrection::apply(const Measurement &in) const
{
    Measurement m = in;

    // ---- Ф9: достоверность ----
    const bool lidarOk = m.lidarCm >= LidarMinCm && m.lidarCm <= LidarMaxCm
                      && m.lidarStrength >= LidarMinStrength;
    if (!lidarOk)
        m.flags |= MfLidarRange;
    if (m_area.size() >= CoreConst::AreaMinVertices && !ReconArea::contains(m_area, m.lat, m.lon))
        m.flags |= MfOutOfArea;

    // ---- Ф6: ПАЕД на 1 м ----
    m.hasPaed1m = false;
    m.paed1m = 0.0f;
    if (m_on && lidarOk) {
        const double h = m.lidarCm / 100.0;
        const bool inRange = (m_maxM <= m_minM) || (h >= m_minM && h <= m_maxM);
        if (inRange) {
            m.paed1m = toOneMeter(m.paed, h, m_mu);
            m.hasPaed1m = true;
        }
    }
    return m;
}

float DoseCorrection::toOneMeter(float paed, double heightM, double mu)
{
    // Ослабление в воздухе между высотой h и 1 м — экспонента (временная модель)
    return float(paed * std::exp(mu * (heightM - 1.0)));
}
