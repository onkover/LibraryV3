#include <algorithm>            // std::clamp

namespace LV3
{
    struct SimulationClock
    {
        float m_timeScale = 0.20f;   // jours simules par seconde reelle
        double m_simDays = 0.0;    // date simulee, en jours depuis l'epoque
        bool  m_paused = false;

        // Ratio multiplicatif, comme CameraZoomSystem : les echelles utiles
        // s'etalent sur cinq decades, un pas additif y serait inutilisable.
        static constexpr float kStep = 1.5f;
        static constexpr float kSprint = 4.0f;
        static constexpr float kMin = 0.001f;
        static constexpr float kMax = 1000.0f;

        // realDt DOIT etre clampe en amont (bug 44) : un point d'arret dans le
        // debogueur produit sinon un dt de plusieurs secondes, et la scene fait
        // un bond de plusieurs annees.
        float Advance(float realDt)
        {
            const float simDt = m_paused ? 0.0f : realDt * m_timeScale;
            m_simDays += simDt;
            return simDt;
        }

        void Scale(bool up, bool sprint)
        {
            const float f = sprint ? kSprint : kStep;
            m_timeScale = std::clamp(up ? m_timeScale * f : m_timeScale / f, kMin, kMax);
        }
    };
}