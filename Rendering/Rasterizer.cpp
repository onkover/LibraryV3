#include "pch.h"
#include "Rasterizer.h"

namespace LV3
{
    bool IsTopLeft(const Vec2f& a, const Vec2f& b) noexcept
    {
        const Vec2f edge{ b.x - a.x, b.y - a.y };
        const bool isTop = (edge.y == 0.0f) && (edge.x < 0.0f);
        const bool isLeft = (edge.y > 0.0f);
        return isTop || isLeft;
    }

    Color FaceColor(int i) noexcept
    {
        const uint32_t h = uint32_t(i) * 2654435761u;
        return MakeColor(uint8_t(((h >> 16) & 0x7F) + 0x60),
            uint8_t(((h >> 8) & 0x7F) + 0x60),
            uint8_t((h & 0x7F) + 0x60));
    }

    void RasterizeTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        const Viewport& vp,
        FragmentCallback onFragment, void* userData)
    {

       /*
       Si des pixels ne sont pas déssiné
           * épars comme du bruit (Épars et fins → le biais top-left) est la signature exacte d'un seuil dépendant de l'échelle
           * Une fente le long d'une arête aurait désigné la géométrie ; 
           * Fentes nettes le long des arêtes partagées → le biais aussi, mais plus grave
           * Trous par triangle entier → géométrie ou culling, pas le remplissage
        3 tests
        * Coupe 1 — le biais : Remets bias = 0.0f partout dans ta version actuelle. Les trous disparaissent → c'était bien lui, applique le correctif ci-dessus.
        * Coupe 2 — le Z-buffer : Dans ShadeFragment_Solid, commente if (!ctx->db->TestAndSet(...)) return;. Les trous disparaissent → ce sont deux triangles à profondeur quasi identique qui se rejettent mutuellement. Symptôme d'un z-fighting sur géométrie coplanaire.
        * Coupe 3 — la géométrie : Passe en Wireframe. Regarde de près : les arêtes forment-elles une surface fermée ? S'il y a des vides dans le fil de fer, le problème est dans le mesh ou dans la triangulation en éventail des quads, pas dans le rasterizer.
       */

        // ── Normalise le sens de parcours : on travaille toujours en aire POSITIVE.
        //    Les poids barycentriques seront reremis dans l'ordre d'origine plus bas.
        Vec2f p0 = v0, p1 = v1, p2 = v2;
        float area = EdgeFunction(p0, p1, p2);
        if (area == 0.0f) return;                       // triangle dégénéré

        const bool flipped = (area < 0.0f);
        if (flipped) { std::swap(p1, p2); area = -area; }

        // ── Bounding box, rognée au VIEWPORT (= le scissor)
        int minX = int(std::floor(std::min({ p0.x, p1.x, p2.x })));
        int minY = int(std::floor(std::min({ p0.y, p1.y, p2.y })));
        int maxX = int(std::ceil(std::max({ p0.x, p1.x, p2.x }))) + 1;
        int maxY = int(std::ceil(std::max({ p0.y, p1.y, p2.y }))) + 1;
        vp.ClampBox(minX, minY, maxX, maxY);
        if (minX >= maxX || minY >= maxY) return;

        // ── Règle top-left : évaluée UNE fois par triangle, pas par pixel
        const bool tl0 = IsTopLeft(p1, p2);
        const bool tl1 = IsTopLeft(p2, p0);
        const bool tl2 = IsTopLeft(p0, p1);

        const float invArea = 1.0f / area;

        for (int y = minY; y < maxY; ++y)
        {
            const float py = float(y) + 0.5f;
            for (int x = minX; x < maxX; ++x)
            {
                const float px = float(x) + 0.5f;

                const float w0 = EdgeFunction(p1, p2, px, py);
                const float w1 = EdgeFunction(p2, p0, px, py);
                const float w2 = EdgeFunction(p0, p1, px, py);

                // Strictement dedans, OU exactement sur une arête top/left.
                // Aucune constante magique : indépendant de la taille du triangle.
                if (!(w0 > 0.0f || (w0 == 0.0f && tl0))) continue;
                if (!(w1 > 0.0f || (w1 == 0.0f && tl1))) continue;
                if (!(w2 > 0.0f || (w2 == 0.0f && tl2))) continue;

                // Si les sommets ont été échangés, w1 et w2 le sont aussi :
                // on les remet dans l'ordre du triangle d'origine.
                const BarycentricWeights bary = flipped
                    ? BarycentricWeights{ w0 * invArea, w2 * invArea, w1 * invArea }
                : BarycentricWeights{ w0 * invArea, w1 * invArea, w2 * invArea };

                onFragment(x, y, bary, userData);
            }
        }


    }


    // Bresenham confiné au viewport. En split-screen, sans ce test,
    // les aretes d'une vue debordent sur l'autre.
    void DrawLineClipped(FrameBuffer& fb, const Viewport& vp,
        const Vec3f& a, const Vec3f& b, Color c) noexcept
    {
        int32_t x0 = int32_t(a.x), y0 = int32_t(a.y);
        const int32_t x1 = int32_t(b.x), y1 = int32_t(b.y);
        const int32_t dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int32_t dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int32_t err = dx + dy;

        for (;;)
        {
            if (vp.Contains(x0, y0)) fb.SetPixel(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            const int32_t e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

