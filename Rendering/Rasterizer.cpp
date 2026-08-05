#include "pch.h"
#include "Rasterizer.h"

namespace LV3
{

    float EdgeFunction(const Vec2f& a, const Vec2f& b, const Vec2f& p)
    {
        return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    }

    bool IsTopLeft(const Vec2f& a, const Vec2f& b)
    {
        Vec2f edge{ b.x - a.x, b.y - a.y };
        bool isTop = (edge.y == 0.0f) && (edge.x < 0.0f);
        bool isLeft = (edge.y > 0.0f);
        return isTop || isLeft;
    }

    void RasterizeTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        int32_t screenWidth, int32_t screenHeight,
        FragmentCallback onFragment, void* userData)
    {
        // 1. Bounding box, clampée au viewport
        int32_t minX = static_cast<int32_t>(std::floor(std::min({ v0.x, v1.x, v2.x })));
        int32_t minY = static_cast<int32_t>(std::floor(std::min({ v0.y, v1.y, v2.y })));
        int32_t maxX = static_cast<int32_t>(std::ceil(std::max({ v0.x, v1.x, v2.x })));
        int32_t maxY = static_cast<int32_t>(std::ceil(std::max({ v0.y, v1.y, v2.y })));

        minX = std::max(minX, 0);
        minY = std::max(minY, 0);
        maxX = std::min(maxX, screenWidth - 1);
        maxY = std::min(maxY, screenHeight - 1);

        // 2. Aire totale du triangle (2x aire signée) — normalise les barycentriques
        const float area = EdgeFunction(v0, v1, v2);
        if (area == 0.0f) return; // triangle dégénéré — rejet immédiat

        // 3. Biais top-left par arête
        const float bias0 = IsTopLeft(v1, v2) ? 0.0f : -1.0f;
        const float bias1 = IsTopLeft(v2, v0) ? 0.0f : -1.0f;
        const float bias2 = IsTopLeft(v0, v1) ? 0.0f : -1.0f;

        // 4. Boucle pixel
        for (int32_t y = minY; y <= maxY; ++y)
        {
            for (int32_t x = minX; x <= maxX; ++x)
            {
                Vec2f p{ x + 0.5f, y + 0.5f }; // centre du pixel

                float w0 = EdgeFunction(v1, v2, p) + bias0;
                float w1 = EdgeFunction(v2, v0, p) + bias1;
                float w2 = EdgeFunction(v0, v1, p) + bias2;

                bool inside = (area > 0.0f) ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                    : (w0 <= 0 && w1 <= 0 && w2 <= 0);
                if (!inside) continue;

                BarycentricWeights bary{ w0 / area, w1 / area, w2 / area };
                onFragment(x, y, bary, userData);
            }
        }
    }

} // namespace LV3