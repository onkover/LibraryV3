#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

#include "../Maths/Vectorlib.h"

namespace LV3
{
    struct BarycentricWeights
    {
        float w0, w1, w2; // normalisés — somme == 1.0
    };

    // Signature que toute fonction de shading doit respecter (voir Fragment.h)
    using FragmentCallback = void(*)(int32_t x, int32_t y,
        const BarycentricWeights& bary,
        void* userData);

    // ── Fonction de bord — double aire signée de (A,B,P) ──
    float EdgeFunction(const Vec2f& a, const Vec2f& b, const Vec2f& p);

    // ── Top-Left Rule — détermine si l'arête A→B doit inclure ses pixels de frontière ──
    bool IsTopLeft(const Vec2f& a, const Vec2f& b);

    // ── Rasterisation d'un triangle en espace écran ──
    void RasterizeTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        int32_t screenWidth, int32_t screenHeight,
        FragmentCallback onFragment, void* userData);

} // namespace LV3