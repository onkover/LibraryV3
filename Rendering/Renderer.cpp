#include "pch.h"
#include "Renderer.h"
#include "Rasterizer.h"
#include "Fragment.h"

namespace LV3
{
    void Renderer::DrawTriangle(const Triangle2D& tri, Color color)
    {
        if (!m_fb || !m_db || !m_vp.IsValid()) return;

        switch (m_mode)
        {
        case ERenderMode::Solid:
        {
            SolidContext ctx{ m_fb, m_db, color, tri.z0, tri.z1, tri.z2 };
            RasterizeTriangle(tri.v0, tri.v1, tri.v2, m_vp, &ShadeFragment_Solid, &ctx);
            break;
        }

        case ERenderMode::Wireframe:
        {
            // Le fil de fer ne passe pas par le rasterizer : il n'a pas de surface.
            const Vec3f a{ tri.v0.x, tri.v0.y, tri.z0 };
            const Vec3f b{ tri.v1.x, tri.v1.y, tri.z1 };
            const Vec3f c{ tri.v2.x, tri.v2.y, tri.z2 };
            DrawLineClipped(*m_fb, m_vp, a, b, color);
            DrawLineClipped(*m_fb, m_vp, b, c, color);
            DrawLineClipped(*m_fb, m_vp, c, a, color);
            break;
        }

        case ERenderMode::Depth:
        {
            DepthContext ctx{ m_fb, tri.z0, tri.z1, tri.z2 };
            RasterizeTriangle(tri.v0, tri.v1, tri.v2, m_vp, &ShadeFragment_Depth, &ctx);
            break;
        }

        case ERenderMode::BarycentricColors:
        {
            UnlitContext ctx{ m_fb, color };
            RasterizeTriangle(tri.v0, tri.v1, tri.v2, m_vp, &ShadeFragment_Barycentric, &ctx);
            break;
        }

        default:
            break;   // Normals, UV : à venir
        }
    }
}