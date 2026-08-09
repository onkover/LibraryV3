#include "pch.h"
#include "Rasterizer.h"

namespace LV3
{
    // Vecteur-ligne SANS division par w : le w sert au rejet du near.
    LV3_FORCEINLINE Vec4f MulRow(const Matrix44f& m, const Vec3f& p) noexcept
    {
        return { p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
                 p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
                 p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2],
                 p.x * m[0][3] + p.y * m[1][3] + p.z * m[2][3] + m[3][3] };
    }

    //float EdgeFunction(const Vec2f& a, const Vec2f& b, const Vec2f& p)
    //{
    //    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    //}

    // ------------------------------------------------------------
    //  Fonction d'arête de Pineda.
    //  Son SIGNE donne le côté ; sa valeur, deux fois l'aire du triangle.
    //  C'est elle qui fait, gratuitement : le test d'appartenance,
    //  les coordonnées barycentriques, et le backface culling.
    // ------------------------------------------------------------
    //LV3_FORCEINLINE float EdgeFn(const Vec3f& a, const Vec3f& b, float px, float py) noexcept
    //{
    //    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
    //}
    // ============================================================
//  Fonction d'arete de Pineda.
//
//  Elle rend TROIS services pour le prix d'un :
//    * EdgeFn(v0, v1, v2)          -> deux fois l'aire signee
//                                     -> backface culling + normalisation
//    * signe de EdgeFn(a, b, px,py) -> le pixel est-il du bon cote de l'arete
//    * valeur / aire                -> coordonnee barycentrique
// ============================================================

// Forme SCALAIRE : le point varie a chaque pixel de la boucle interne.
    [[nodiscard]] LV3_FORCEINLINE float EdgeFn(const Vec3f& a, const Vec3f& b, float px, float py) noexcept
    {
        return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
    }

    // Forme SOMMET : pour l'aire signee d'un triangle. Simple confort.
    [[nodiscard]] LV3_FORCEINLINE float EdgeFn(const Vec3f& a, const Vec3f& b, const Vec3f& c) noexcept
    {
        return EdgeFn(a, b, c.x, c.y);
    }

    bool IsTopLeft(const Vec2f& a, const Vec2f& b)
    {
        Vec2f edge{ b.x - a.x, b.y - a.y };
        bool isTop = (edge.y == 0.0f) && (edge.x < 0.0f);
        bool isLeft = (edge.y > 0.0f);
        return isTop || isLeft;
    }


    // Couleur par face : permet de voir le facettage sans eclairage.
    Color FaceColor(int i) noexcept
    {
        const uint32_t h = uint32_t(i) * 2654435761u;
        return MakeColor(uint8_t(((h >> 16) & 0x7F) + 0x60),
            uint8_t(((h >> 8) & 0x7F) + 0x60),
            uint8_t((h & 0x7F) + 0x60));
    }


    //void RasterizeTriangleOLD(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
    //    int32_t screenWidth, int32_t screenHeight,
    //    FragmentCallback onFragment, void* userData)
    //{
    //    // 1. Bounding box, clampée au viewport
    //    int32_t minX = static_cast<int32_t>(std::floor(std::min({ v0.x, v1.x, v2.x,0.0f })));
    //    int32_t minY = static_cast<int32_t>(std::floor(std::min({ v0.y, v1.y, v2.y,0.0f })));
    //    int32_t maxX = static_cast<int32_t>(std::ceil(std::max({ v0.x, v1.x, v2.x,0.0f })));
    //    int32_t maxY = static_cast<int32_t>(std::ceil(std::max({ v0.y, v1.y, v2.y,0.0f })));

    //    minX = std::max(minX, 0);
    //    minY = std::max(minY, 0);
    //    maxX = std::min(maxX, screenWidth - 1);
    //    maxY = std::min(maxY, screenHeight - 1);

    //    // 2. Aire totale du triangle (2x aire signée) — normalise les barycentriques
    //    const float area = EdgeFunction(v0, v1, v2);
    //    if (area == 0.0f) return; // triangle dégénéré — rejet immédiat

    //    // 3. Biais top-left par arête
    //    const float bias0 = IsTopLeft(v1, v2) ? 0.0f : -1.0f;
    //    const float bias1 = IsTopLeft(v2, v0) ? 0.0f : -1.0f;
    //    const float bias2 = IsTopLeft(v0, v1) ? 0.0f : -1.0f;

    //    // 4. Boucle pixel
    //    for (int32_t y = minY; y <= maxY; ++y)
    //    {
    //        for (int32_t x = minX; x <= maxX; ++x)
    //        {
    //            Vec2f p{ x + 0.5f, y + 0.5f }; // centre du pixel

    //            float w0 = EdgeFunction(v1, v2, p) + bias0;
    //            float w1 = EdgeFunction(v2, v0, p) + bias1;
    //            float w2 = EdgeFunction(v0, v1, p) + bias2;

    //            bool inside = (area > 0.0f) ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
    //                : (w0 <= 0 && w1 <= 0 && w2 <= 0);
    //            if (!inside) continue;

    //            BarycentricWeights bary{ w0 / area, w1 / area, w2 / area };
    //            onFragment(x, y, bary, userData);
    //        }
    //    }
    //}
    // ------------------------------------------------------------
    //  Rasterisation pleine, méthode Pineda.
    //  `area` est fourni : il a déjà servi au backface culling.
    // ------------------------------------------------------------
    void RasterizeTriangle(FrameBuffer& fb, DepthBuffer& db, const Viewport& vp,
        const Vec3f& r0, const Vec3f& r1, const Vec3f& r2,
        float area, Color color) noexcept
    {
        // Bounding box entiere, rognee au viewport.
        // C'est le SCISSOR : il remplace le clipping des 4 plans lateraux.
        int32_t x0 = int32_t(std::floor(std::min({ r0.x, r1.x, r2.x })));
        int32_t y0 = int32_t(std::floor(std::min({ r0.y, r1.y, r2.y })));
        int32_t x1 = int32_t(std::ceil(std::max({ r0.x, r1.x, r2.x })));
        int32_t y1 = int32_t(std::ceil(std::max({ r0.y, r1.y, r2.y })));
        vp.ClampBox(x0, y0, x1, y1);
        if (x0 >= x1 || y0 >= y1) return;

        const float invArea = 1.0f / area;

        for (int32_t y = y0; y < y1; ++y)
        {
            const float py = float(y) + 0.5f;              // centre du pixel
            for (int32_t x = x0; x < x1; ++x)
            {
                const float px = float(x) + 0.5f;

                const float w0 = EdgeFn(r1, r2, px, py);
                const float w1 = EdgeFn(r2, r0, px, py);
                const float w2 = EdgeFn(r0, r1, px, py);

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;   // hors du triangle

                // Barycentriques — serviront aussi aux UV et aux couleurs (Lecon 04 P2)
                const float b0 = w0 * invArea;
                const float b1 = w1 * invArea;
                const float b2 = w2 * invArea;

                // Profondeur : z_ndc s'interpole LINEAIREMENT en espace ecran.
                // (les UV et les couleurs, elles, exigeront le 1/w)
                const float z = b0 * r0.z + b1 * r1.z + b2 * r2.z;

                // REVERSE-Z : test GREATER. TestAndSet fait le test ET l'ecriture
                // en un seul calcul d'index.
                if (!db.TestAndSet(x, y, z)) continue;

                // Deja rogne par ClampBox : pas de retest des bornes.
                fb.SetPixelUnchecked(x, y, color);
            }
        }
    }

    // Bresenham confiné au viewport. En split-screen, sans ce test,
    // les aretes d'une vue debordent sur l'autre.
    void DrawLineClipped(FrameBuffer& fb, const Viewport& vp, const Vec3f& a, const Vec3f& b, Color c) noexcept
    {
        int32_t x0 = int32_t(a.x), y0 = int32_t(a.y);
        const int32_t x1 = int32_t(b.x), y1 = int32_t(b.y);
        const int32_t dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int32_t dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int32_t err = dx + dy;

        for (;;)
        {
            if (vp.Contains(x0, y0)) fb.SetPixel(x0, y0, c);   // <-- LE test
            if (x0 == x1 && y0 == y1) break;
            const int32_t e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

} // namespace LV3