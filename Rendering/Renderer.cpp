#include "pch.h"
#include "Renderer.h"
#include "Rasterizer.h"
#include "Fragment.h"
#include "core/logger.h"
#if LV3_PROFILE
    #include "Core/Profiler.h"     // hors pch.h : inclusion explicite (comme Rasterizer.cpp)
#endif

namespace LV3
{
    #if LV3_RASTER_STATS
        // C0 : UN enregistrement par triangle (ou par point), jamais par pixel.
    #define LV3_FLUSH_DEPTH_PASSED(ctx) \
                do { LV3_PROF_COUNT(EProfCounter::PixelsDepthPassed, (ctx).depthPassed); \
                     (ctx).depthPassed = 0; } while (0)
    #else
        #define LV3_FLUSH_DEPTH_PASSED(ctx) ((void)0)
    #endif

    // C0b : sonde du 4K-aliasing. Z-buffer (float) et framebuffer (uint32_t, pitch = W*4)
    // ont la meme geometrie : l'ecart d'adresse D est le meme pour TOUS les pixels.
    // D mod 4096 proche de 0 (dans un sens ou dans l'autre) => fausses dependances
    // charge profondeur / stockage couleur, a chaque pixel.
    void Renderer::ProbeAlias4K(const FrameBuffer& fb, const DepthBuffer& db) noexcept
    {
        #if LV3_PROFILE
            const uintptr_t c = reinterpret_cast<uintptr_t>(fb.frameBuffer());
            const uintptr_t d = reinterpret_cast<uintptr_t>(db.Data());
            LV3_PROF_COUNT(EProfCounter::FbDbAlias4K, uint64_t((c - d) & uintptr_t(4095)));
            
            //// ⚠ SONDE PROVISOIRE C0b : UNE ligne par run. A RETIRER apres lecture.
            //static bool s_logged = false;
            //if (!s_logged)
            //{
            //    s_logged = true;
            //    char buf[160];
            //    std::snprintf(buf, sizeof(buf),
            //        "[C0b] couleur=%p (page+%llu) profondeur=%p (page+%llu) D mod 4096=%llu",
            //        fb.frameBuffer(), (unsigned long long)(c & 4095u),
            //        static_cast<const void*>(db.Data()), (unsigned long long)(d & 4095u),
            //        (unsigned long long)((c - d) & 4095u));
            //    Logger::info(buf);
            //}

        #else
            (void)fb; (void)db;
        #endif
    }


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
        LV3_FLUSH_DEPTH_PASSED(m_ctx);      // C0 : comptage seulement (RASTER_STATS)

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
        #if LV3_RASTER_STATS
            // C0 : un point n'est PAS un triangle. Il n'entre pas dans PixelsCovered, donc il
            // ne doit pas entrer dans PixelsDepthPassed. Remise a zero SANS enregistrement.
            m_ctx.depthPassed = 0;
        #endif
    }
}