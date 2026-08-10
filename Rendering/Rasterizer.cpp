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
        // 1. Bounding box, rognée au VIEWPORT (= le scissor)
        int minX = int(std::floor(std::min({ v0.x, v1.x, v2.x })));
        int minY = int(std::floor(std::min({ v0.y, v1.y, v2.y })));
        int maxX = int(std::ceil(std::max({ v0.x, v1.x, v2.x }))) + 1;
        int maxY = int(std::ceil(std::max({ v0.y, v1.y, v2.y }))) + 1;
        vp.ClampBox(minX, minY, maxX, maxY);
        if (minX >= maxX || minY >= maxY) return;

        // 2. Aire signée : normalise les barycentriques et donne le sens de parcours
        const float area = EdgeFunction(v0, v1, v2);
        if (area == 0.0f) return;                       // triangle dégénéré

        // 3. Biais top-left par arête : évite qu'un pixel de frontière
        //    soit dessiné DEUX fois par deux triangles adjacents.
        const float bias0 = IsTopLeft(v1, v2) ? 0.0f : -1.0f;
        const float bias1 = IsTopLeft(v2, v0) ? 0.0f : -1.0f;
        const float bias2 = IsTopLeft(v0, v1) ? 0.0f : -1.0f;

        const float invArea = 1.0f / area;

        // 4. Boucle pixel
        for (int y = minY; y < maxY; ++y)
        {
            const float py = float(y) + 0.5f;
            for (int x = minX; x < maxX; ++x)
            {
                const float px = float(x) + 0.5f;

                const float w0 = EdgeFunction(v1, v2, px, py) + bias0;
                const float w1 = EdgeFunction(v2, v0, px, py) + bias1;
                const float w2 = EdgeFunction(v0, v1, px, py) + bias2;

                const bool inside = (area > 0.0f) ? (w0 >= 0.f && w1 >= 0.f && w2 >= 0.f)
                    : (w0 <= 0.f && w1 <= 0.f && w2 <= 0.f);
                if (!inside) continue;

                const BarycentricWeights bary{ w0 * invArea, w1 * invArea, w2 * invArea };
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

