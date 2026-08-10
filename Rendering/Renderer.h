#pragma once
#include "../Maths/Vectorlib.h"
#include "FrameBuffer.h"
#include "DepthBuffer.h"
#include "Viewport.h"
#include "RenderTypes.h"

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
        // --- État, posé une fois par frame ---
        void BeginFrame(FrameBuffer& fb, DepthBuffer& db) noexcept
        {
            m_fb = &fb; m_db = &db;
        }
        void EndFrame() noexcept { m_fb = nullptr; m_db = nullptr; }

        // --- État, posé une fois par VUE ---
        void SetViewport(const Viewport& vp) noexcept { m_vp = vp; }
        void SetMode(ERenderMode m)          noexcept { m_mode = m; }

        [[nodiscard]] const Viewport& GetViewport() const noexcept { return m_vp; }
        [[nodiscard]] ERenderMode     GetMode()     const noexcept { return m_mode; }

        // --- Soumission ---
        void DrawTriangle(const Triangle2D& tri, Color color);

    private:
        FrameBuffer* m_fb = nullptr;
        DepthBuffer* m_db = nullptr;
        Viewport     m_vp{};
        ERenderMode  m_mode = ERenderMode::Solid;
    };
}