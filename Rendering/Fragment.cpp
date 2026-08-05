#include "pch.h"
#include "Fragment.h"

namespace LV3
{

    void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights& /*bary*/, void* userData)
    {
        auto* ctx = static_cast<UnlitContext*>(userData);
        ctx->fb->SetPixel(x, y, ctx->color);
        //ctx->fb->SetPixel(x, y, Color{ 255, 255, 255, 255 });
    }

    void ShadeFragment_Depth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
    {
        auto* ctx = static_cast<DepthContext*>(userData);
        float depth = bary.w0 * ctx->z0 + bary.w1 * ctx->z1 + bary.w2 * ctx->z2;
        uint8_t gray = static_cast<uint8_t>(std::clamp(depth, 0.0f, 1.0f) * 255.0f);
        ctx->fb->SetPixel(x, y, Color{ gray, gray, gray, 255 });
    }

    void ShadeFragment_Barycentric(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
    {
        // w0 est le poids de v0, pas de l'arête v0v1

        auto* ctx = static_cast<UnlitContext*>(userData);
        ctx->fb->SetPixel(x, y, Color{
            static_cast<uint8_t>(bary.w2 * 255.0f), // b
            static_cast<uint8_t>(bary.w1 * 255.0f), // g
            static_cast<uint8_t>(bary.w0 * 255.0f), // r
            255
            });
    }

} // namespace LV3