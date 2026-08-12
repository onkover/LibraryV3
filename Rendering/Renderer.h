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
    struct Triangle2D
    {
        Vec2f v0, v1, v2;
        float z0 = 0.f, z1 = 0.f, z2 = 0.f;   // profondeurs NDC, pour le Z-buffer
    };

    // Propriétaire de l'ÉTAT de rendu. C'est lui, et lui seul, qui sait
    // quel chemin de fragment correspond au mode courant.
    class Renderer
    {
    public:
        // ── État de FRAME (posé 1x/frame): les cibles ──────────────────────────
        void BeginFrame(FrameBuffer& fb, DepthBuffer& db) noexcept
        {
            m_ctx.fb = &fb;  m_ctx.db = &db;    // posés UNE fois par frame
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
            default:                             m_fragment = nullptr;                    break;
            }
        }
        
        void SetDepthRange(float nearPlane, float farPlane) noexcept
        {
            m_ctx.nearPlane = nearPlane;
            m_ctx.farPlane = farPlane;
        }

        [[nodiscard]] const Viewport& GetViewport() const noexcept { return m_vp; }
        [[nodiscard]] ERenderMode     GetMode()     const noexcept { return m_mode; }
        [[nodiscard]] bool            IsReady()     const noexcept
        {
            return m_ctx.fb && m_ctx.db && m_vp.IsValid();
        }

        // ── Soumission ──────────────────────────────────────────
        void DrawTriangle(const Triangle2D& tri, Color color);

    private:
        FragmentContext  m_ctx{};
        FragmentCallback m_fragment = nullptr;
        Viewport     m_vp{};
        ERenderMode  m_mode = ERenderMode::Solid;
    };
}