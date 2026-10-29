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


//    void Renderer::DrawTriangle(const RasterTriangle& tri, Color color)
//    {
//        if (!IsReady()) return;
//
//        if (m_mode == ERenderMode::Wireframe)
//        {
//            const Vec3f a{ tri.v0.x, tri.v0.y, tri.z0 };
//            const Vec3f b{ tri.v1.x, tri.v1.y, tri.z1 };
//            const Vec3f c{ tri.v2.x, tri.v2.y, tri.z2 };
//            DrawLineClipped(*m_ctx.fb, m_vp, a, b, color);
//            DrawLineClipped(*m_ctx.fb, m_vp, b, c, color);
//            DrawLineClipped(*m_ctx.fb, m_vp, c, a, color);
//            return;
//        }
//        if (!m_fragment) return;
//
//        // Mise à jour EN PLACE : 4 écritures, aucune construction.
//        m_ctx.color = color;                                         // 4 écritures,
//        m_ctx.z0 = tri.z0;  m_ctx.z1 = tri.z1;  m_ctx.z2 = tri.z2;   // aucune construction
////		m_ctx.depthDisplayRange = m_ctx.depthDisplayRange;
//		m_ctx.invW0 = tri.invW0;  m_ctx.invW1 = tri.invW1;  m_ctx.invW2 = tri.invW2;
//
//        RasterizeTriangle(tri.v0, tri.v1, tri.v2, m_vp, m_fragment, &m_ctx);
//        LV3_FLUSH_DEPTH_PASSED(m_ctx);      // C0 : comptage seulement (RASTER_STATS)
//
//    }
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
        if (!m_triangle) return;

        // Mise à jour EN PLACE : 4 écritures, aucune construction.
        m_ctx.color = color;                                         // 4 écritures,
        m_ctx.z0 = tri.z0;  m_ctx.z1 = tri.z1;  m_ctx.z2 = tri.z2;   // aucune construction
        //		m_ctx.depthDisplayRange = m_ctx.depthDisplayRange;
        m_ctx.invW0 = tri.invW0;  m_ctx.invW1 = tri.invW1;  m_ctx.invW2 = tri.invW2;

        m_triangle(tri.v0, tri.v1, tri.v2, m_vp, m_ctx);   // C1 : 1 appel indirect par triangle
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

    // ════════════════════════════════════════════════════════════
    //  C3 : LOT DE 8 FACES SURVIVANTES
    // ════════════════════════════════════════════════════════════
    void Renderer::DrawPixel1(const RasterVertex& a, const RasterVertex& b, const RasterVertex& c,
        const Pixel1Lanes8& p1, uint32_t k, Color color)
    {
        // Comptage : le meme enregistrement que le noyau pour une boite de 1 pixel.
        [[maybe_unused]] const bool covered = ((p1.covered >> k) & 1u) != 0;
        LV3_RASTER_RECORD(1u, 1u, covered ? 1u : 0u);
#if LV3_RASTER_STATS
        LV3_PROF_COUNT(EProfCounter::TrisPix1, 1);
#endif
        if (!covered) return;                       // V1 : 66 % des triangles s'arretent ICI

        // Le fragment, par le MEME shader que le noyau (Shade() du mode, via m_fragment).
        m_ctx.color = color;
        m_ctx.z0 = a.z;  m_ctx.z1 = b.z;  m_ctx.z2 = c.z;
        m_ctx.invW0 = a.invW;  m_ctx.invW1 = b.invW;  m_ctx.invW2 = c.invW;
        m_fragment(p1.x[k], p1.y[k], BarycentricWeights{ p1.b0[k], p1.b1[k], p1.b2[k] }, &m_ctx);
        LV3_FLUSH_DEPTH_PASSED(m_ctx);
    }

    void Renderer::DrawBatch8(const RasterVertex* rv, const uint32_t* idx, uint32_t surv,
        const Pixel1Lanes8& p1, bool hasTint, Color tint, int32_t base)
    {
        // ⚠ ABLATION M7 (chantier 2) : le dessin est debranche, comme SubmitRasterTriangle.
        #if LV3_ABLATION_RASTER
                (void)rv; (void)idx; (void)surv; (void)p1; (void)hasTint; (void)tint; (void)base;
                return;
        #else
            if (!IsReady()) return;
            // Le chemin C3 n'existe que la ou DrawTriangle appellerait le noyau.
            // Wireframe et modes sans shader : tout passe par DrawTriangle, comme avant.
            const uint32_t pix1 = (m_mode != ERenderMode::Wireframe && m_triangle) ? p1.pix1 : 0u;

            while (surv)
            {
                const uint32_t k = uint32_t(std::countr_zero(surv));   // couloir le plus bas
                surv &= surv - 1u;
                const uint32_t* t = idx + k * 3;
                const RasterVertex& a = rv[t[0]];
                const RasterVertex& b = rv[t[1]];
                const RasterVertex& c = rv[t[2]];
                const Color col = hasTint ? tint : FaceColor(base + int32_t(k));

                if ((pix1 >> k) & 1u)
                    DrawPixel1(a, b, c, p1, k, col);
                else
                    DrawTriangle(RasterTriangle{ { a.x, a.y }, { b.x, b.y }, { c.x, c.y },
                                                 a.z, b.z, c.z, a.invW, b.invW, c.invW }, col);
            }
        #endif
    }
}