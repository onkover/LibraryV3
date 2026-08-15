#include "pch.h"
#include "Renderer.h"
#include "Rasterizer.h"
#include "Fragment.h"

namespace LV3
{
    void Renderer::DrawTriangle(const RasterTriangle& tri, Color color)
    {
        if (!IsReady()) return;

        if (m_mode == ERenderMode::Wireframe)
        {
            const Vec3f a{ tri.v0.x, tri.v0.y, tri.z0 };
            const Vec3f b{ tri.v1.x, tri.v1.y, tri.z1 };
            const Vec3f c{ tri.v2.x, tri.v2.y, tri.z2 };
            DrawLineClipped(*m_ctx.fb, m_vp, a, b, color);
            DrawLineClipped(*m_ctx.fb, m_vp, b, c, color);
            DrawLineClipped(*m_ctx.fb, m_vp, c, a, color);
            return;
        }
        if (!m_fragment) return;

        // Mise à jour EN PLACE : 4 écritures, aucune construction.
        m_ctx.color = color;                                         // 4 écritures,
        m_ctx.z0 = tri.z0;  m_ctx.z1 = tri.z1;  m_ctx.z2 = tri.z2;   // aucune construction
//		m_ctx.depthDisplayRange = m_ctx.depthDisplayRange;
		m_ctx.invW0 = tri.invW0;  m_ctx.invW1 = tri.invW1;  m_ctx.invW2 = tri.invW2;

        RasterizeTriangle(tri.v0, tri.v1, tri.v2, m_vp, m_fragment, &m_ctx);

    }
}