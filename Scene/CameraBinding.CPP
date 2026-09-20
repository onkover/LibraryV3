#include "pch.h"          // ? première ligne, toujours
#include "CameraBinding.hpp"
#include "core/enginesettings.h"
#include "core/engineconfig.h" 
#include "core/logger.h"

namespace LV3
{
    // Decoupe un rectangle. Aucune notion de camera.
    [[nodiscard]] size_t BuildLayout(ELayout layout, int w, int h, Viewport* out)
    {
        LV3_ASSERT(out && w > 1 && h > 1);
        const int hw = w / 2, hh = h / 2;

        switch (layout)
        {
        case ELayout::Single:
            out[0] = { 0, 0, w, h };
            return 1;

        case ELayout::SplitH:                       // cote a cote
            out[0] = { 0,  0, hw,     h };
            out[1] = { hw, 0, w - hw, h };
            return 2;

        case ELayout::SplitV:                       // l'un au-dessus de l'autre
            out[0] = { 0, 0,  w, hh };
            out[1] = { 0, hh, w, h - hh };
            return 2;
        
        case ELayout::MainSide:                     // 75% principal, 25% lateral
            {
                const int mainW = (w * 3) / 4;
                out[0] = { 0,     0, mainW,     h };
                out[1] = { mainW, 0, w - mainW, h };
            }
            return 2;

        case ELayout::Quad:
            out[0] = { 0,  0,  hw,     hh };
            out[1] = { hw, 0,  w - hw, hh };
            out[2] = { 0,  hh, hw,     h - hh };
            out[3] = { hw, hh, w - hw, h - hh };
            return 4;
        }
        return 0;   // enum hors domaine
    }


    size_t BuildCameraBindings(ELayout layout, const ViewSlot* slots, size_t slotCount, int w, int h, CameraBinding* out)
    {
        Viewport vps[kMaxCamerasHard];
        const size_t n = BuildLayout(layout, w, h, vps);

        // > et non >= : vps a EXACTEMENT kMaxCamerasHard cases valides (indices
        // 0..kMaxCamerasHard-1), donc n == kMaxCamerasHard remplit le tampon pile
        // jusqu'au bord et est légitime — ce n'est PAS un débordement. Seul n >
        // kMaxCamerasHard l'est (impossible aujourd'hui vu Viewport vps[kMaxCamerasHard],
        // mais la borne doit rester correcte le jour où BuildLayout gagne un layout
        // au-delà de Quad).
        if (n > kMaxCamerasHard)
        {
            LV3::Logger::warn("[CameraBinding] nombre de viewport (" + std::to_string(n)
                + ") > kMaxCamerasHard (" + std::to_string(kMaxCamerasHard) + ")");
            return 0;
        }

        // Plafond RUNTIME (engine.json, borné par kMaxCamerasHard au chargement) : distinct
        // du plafond compile-time ci-dessus. Rejeter ici, jamais dans un std::vector local
        // redimensionné à la volée dans un système appelé par frame.
        const size_t maxViewportSoft = static_cast<size_t>(LV3::EngineConfig::Get().viewport.maxViewport);
        if (n > maxViewportSoft)
        {
            LV3::Logger::warn("[CameraBinding] nombre de viewport (" + std::to_string(n)
                + ") > viewport.maxViewport configure (" + std::to_string(maxViewportSoft) + ")");
            return 0;
        }

        if (n != slotCount)
        {
            LV3::Logger::warn("[CameraBinding] nombre de slots (" + std::to_string(slotCount) + ") != nombre de vues (" + std::to_string(n) + ") pour le layout " + std::to_string(static_cast<int>(layout)));
            return 0;
        }

        for (size_t i = 0; i < n; ++i)
        {
            LV3_ASSERT(slots[i].m_camera != NULL_ENTITY);
            out[i] = { slots[i].m_camera, vps[i], slots[i].m_mode };
        }

        // Aucune camera deux fois : son gizmo porte m_hideForCamera et serait
        // masque dans TOUTES les vues ou elle apparait, donc invisible partout.
        #ifdef _DEBUG
            for (size_t i = 0; i < n; ++i)
                for (size_t j = i + 1; j < n; ++j)
                    LV3_ASSERT(out[i].m_camera != out[j].m_camera);
        #endif

        return n;
    }
}