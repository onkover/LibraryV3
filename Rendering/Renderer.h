#pragma once
#include "../Maths/Vectorlib.h"
#include "FrameBuffer.h"
#include "DepthBuffer.h"
#include "Viewport.h"
#include "RenderTypes.h"
#include "Fragment.h"   // FragmentContext, FragmentCallback

namespace LV3
{
    // Triangle déjà transformé en espace écran (post-viewport).
    struct RasterTriangle
    {
        Vec2f v0, v1, v2;                               // pixels, Y déjà flippé par ToRaster
        float z0 = 0.f, z1 = 0.f, z2 = 0.f;             // z_ndc ∈ [0,1] reverse-Z — interpolé AFFINEMENT
        float invW0 = 0.f, invW1 = 0.f, invW2 = 0.f;    // 1/w — dénominateur perspectif
    };

    // Propriétaire de l'ÉTAT de rendu. C'est lui, et lui seul, qui sait
    // quel chemin de fragment correspond au mode courant.
    class Renderer
    {
    public:
        // ── État de FRAME (posé 1x/frame): les cibles ──────────────────────────
        //En split - screen, attention : si les deux vues partagent le même DepthBuffer et que BeginFrame est appelé deux fois par frame, 
        // le second Clear() efface la profondeur de la première vue.
        // Un Clear() par frame, avant la première vue, serait plus juste.
        void BeginFrame(FrameBuffer& fb, DepthBuffer& db) noexcept
        {
            // Si tu redimensionnes la fenêtre sans redimensionner le Z-buffer, TestAndSet écrit hors du std::vector — corruption silencieuse en Release, assertion cryptique en Debug.
            LV3_ASSERT(db.Width() == fb.Width() &&
                db.Height() == fb.Height() &&
                "DepthBuffer et FrameBuffer de tailles differentes — TestAndSet ecrira hors bornes");

            m_ctx.fb = &fb;  
            m_ctx.db = &db;
            db.Clear();
        }

        void EndFrame() noexcept { m_ctx.fb = nullptr; m_ctx.db = nullptr; }

        // ── État de VUE (posé une fois par VUE) : région et mode ────────────────────────
        void SetViewport(const Viewport& vp) noexcept { m_vp = vp; }

        void SetMode(ERenderMode m) noexcept
        {
            m_mode = m;
            switch (m)                            // résolu UNE fois par VUE
            {
            case ERenderMode::Solid:             m_fragment = &ShadeFragment_Solid;       break;
            case ERenderMode::Depth:             m_fragment = &ShadeFragment_Depth;       break;
            case ERenderMode::BarycentricColors: m_fragment = &ShadeFragment_Barycentric; break;
            case ERenderMode::LinearDepth:       m_fragment = &ShadeFragment_LinearDepth;   break;
            default:                             m_fragment = nullptr;                    break;
            }
        }
        
        //void SetDepthRange(float nearPlane, float farPlane) noexcept
        //{
        //    m_ctx.nearPlane = nearPlane;
        //    m_ctx.farPlane = farPlane;
        //}

        void SetDepthDisplayRange(float rangeWorldUnits) noexcept
        {
            LV3_ASSERT(rangeWorldUnits > 0.0f);
            m_ctx.depthDisplayRange = rangeWorldUnits;
        }
        [[nodiscard]] const Viewport& GetViewport() const noexcept { return m_vp; }
        [[nodiscard]] ERenderMode     GetMode()     const noexcept { return m_mode; }
        [[nodiscard]] bool            IsReady()     const noexcept
        {
            return m_ctx.fb && m_ctx.db && m_vp.IsValid();
        }

        // ── Soumission ──────────────────────────────────────────
        void DrawTriangle(const RasterTriangle& tri, Color color);

    private:
        FragmentContext  m_ctx{};
        FragmentCallback m_fragment = nullptr;
        Viewport     m_vp{};
        ERenderMode  m_mode = ERenderMode::Solid;
    };
}