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
    /// <summary>
	/// Dessine un point dans la vue à la place du mesh (car celui-ci est reduit à un seul pixel). 
    /// Le fragment est appelé avec des poids barycentriques (1, 0, 0) et la profondeur du point.
    /// </summary>
    /// <param name="p"></param>
    /// <param name="color"></param>
    void Renderer::DrawPoint(const RasterVertex& p, Color color)
    {
        if (!IsReady()) return;

        // Pixel qui CONTIENT le point : les centres sont en (i + 0,5), floor donne i.
        const int32_t x = int32_t(std::floor(p.x));
        const int32_t y = int32_t(std::floor(p.y));
        if (x < m_vp.x || x >= m_vp.x + m_vp.width ||
            y < m_vp.y || y >= m_vp.y + m_vp.height) return;   // centre hors de la vue

        if (m_mode == ERenderMode::Wireframe) { m_ctx.fb->SetPixel(x, y, color); return; }
        if (!m_fragment) return;

        // Triangle degenere : les trois sommets SONT le point, poids (1, 0, 0).
        m_ctx.color = color;
        m_ctx.z0 = m_ctx.z1 = m_ctx.z2 = p.z;
        m_ctx.invW0 = m_ctx.invW1 = m_ctx.invW2 = p.invW;
        m_fragment(x, y, BarycentricWeights{ 1.0f, 0.0f, 0.0f }, &m_ctx);
    }
}