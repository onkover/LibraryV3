#pragma once
#include <algorithm>            // std::clamp

namespace LV3
{
    // Reglage : lu une fois depuis engine.json, constant ensuite.
    struct SimulationClockSettings
    {
        float m_step = 1.5f;      // ratio par cran
        float m_sprint = 4.0f;      // ratio par cran, Shift enfonce
        float m_min = 0.001f;
        float m_max = 1000.0f;
        float m_default = 0.20f;    // valeur de reinitialisation (touche 0)
        float m_timeScale = 0.20f;  // jours simules par seconde reelle
		double m_simTime = 0.0;     // date simulee, en jours depuis l'epoque
    };

    struct SimulationClock
    {
        // --- reglage (invariant apres Configure) ---
        SimulationClockSettings m_cfg;

        // --- etat (change a chaque frame) ---
        float  m_timeScale = 0.20f;     // le facteur de conversion. Combien de temps-monde pour une seconde réelle. Jours simules par seconde reelle
        double m_simTime = 0.0;         // l'intégrale de la conversion . Etat — la date, pour ceux qui veulent une position absolue. Date simulee, en jours depuis l'epoque
        bool   m_paused = false;        // Interrupteur sur la conversion. Etat — suspendre sans détruire l'échelle

        // --- Configuration ---
        // Pas de constructeur : l'objet reste un agregat, donc constructible
        // par defaut. C'est ce qui permet a main.cpp de le declarer AVANT que
        // engine.json ne soit lu, puis de le configurer apres.
        void Configure(const SimulationClockSettings& cfg, double epochDays = 0.0)
        {
            LV3_ASSERT(cfg.m_step > 1.0f);   // un ratio <= 1 inverse ou fige le zoom temporel
            LV3_ASSERT(cfg.m_sprint > 1.0f);
            LV3_ASSERT(cfg.m_min > 0.0f);   // 0 rendrait le clamp bas absorbant
            LV3_ASSERT(cfg.m_min < cfg.m_max);
            m_cfg = cfg;
            m_timeScale = std::clamp(cfg.m_default, cfg.m_min, cfg.m_max);
            m_simTime = epochDays;
            m_paused = false;
        }

        // realDt DOIT etre clampe en amont (bug 44) : un point d'arret dans le
        // debogueur produit sinon un dt de plusieurs secondes, et la scene fait
        // un bond de plusieurs annees.
        // simDt(le retour) sert aux systèmes qui intègrent.C'est ton AnimationSystem d'aujourd'hui : angle += vitesse * simDt.
        // m_simDays(l'état) servira aux systèmes qui calculent. C'est OrbitSystem du v2 : angle = phase + vitesse × simDays.
        float Advance(float realDt)
        {
            const float simDt = m_paused ? 0.0f : realDt * m_timeScale;
            m_simTime += simDt;
            return simDt;
        }

        void Scale(bool up, bool sprint)
        {
            const float f = sprint ? m_cfg.m_sprint : m_cfg.m_step;
            m_timeScale = std::clamp(up ? m_timeScale * f : m_timeScale / f, m_cfg.m_min, m_cfg.m_max);
        }

        void Reset()
        {
            m_timeScale = m_cfg.m_default; 
        }

    };
}