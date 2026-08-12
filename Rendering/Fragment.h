#pragma once
#include "Rasterizer.h"   // BarycentricWeights, FragmentCallback
#include "FrameBuffer.h"  // FrameBuffer, Color
#include "DepthBuffer.h"  // DepthBuffer

namespace LV3
{

    // ── Contextes de shading ──
    // Chaque fonction ShadeFragment_* définit SA propre structure de contexte.
    // C'est ce pointeur qui transite par le void* userData de RasterizeTriangle.

    struct UnlitContext
    {
        FrameBuffer* fb;
        Color        color;
    };

    struct DepthContext
    {
        FrameBuffer* fb;
        float z0, z1, z2; // profondeur des 3 sommets du triangle courant
    };

    struct FragmentContext
    {
        FrameBuffer* fb = nullptr;
        DepthBuffer* db = nullptr;
        Color        color{};
        float        z0 = 0.f, z1 = 0.f, z2 = 0.f;   // profondeurs NDC des 3 sommets
    };

    // Comptage de couverture — utilise par la TNR top-left.
    struct CountContext
    {
        std::vector<uint8_t>* counts = nullptr; 
        int32_t width = 0;
    };

    // ── Implémentations de FragmentCallback ──
    // Signature imposée par le type FragmentCallback défini dans Rasterizer.h

    void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Depth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Barycentric(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Solid(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
} // namespace LV3