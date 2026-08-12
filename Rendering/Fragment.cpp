#include "pch.h"
#include "Fragment.h"

namespace LV3
{

    //void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights& /*bary*/, void* userData)
    //{
    //    auto* ctx = static_cast<UnlitContext*>(userData);
    //    ctx->fb->SetPixel(x, y, ctx->color);
    //    //ctx->fb->SetPixel(x, y, Color{ 255, 255, 255, 255 });
    //}

    void ShadeFragment_Depth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
    {
        auto* ctx = static_cast<FragmentContext*>(userData);

        const float z = bary.w0 * ctx->z0 + bary.w1 * ctx->z1 + bary.w2 * ctx->z2;
        if (!ctx->db->TestAndSet(x, y, z)) return;

        // Inversion de la projection reverse-Z : on remonte a la DISTANCE reelle.
        //   z = n(f - d) / (d(f - n))   =>   d = n·f / (z(f - n) + n)
        const float n = ctx->nearPlane;
        const float f = ctx->farPlane;
        const float denom = z * (f - n) + n;
        const float d = (denom > 1e-9f) ? (n * f) / denom : f;

        // Rampe LINEAIRE en distance : blanc = proche, noir = loin.
        const float t = std::clamp((d - n) / (f - n), 0.0f, 1.0f);
        const uint8_t g = static_cast<uint8_t>((1.0f - t) * 255.0f);

        // a savoir qu'avec m_infiniteFar, farPlane vaut 1e30f et la rampe s'effondre. 
        // Deux options : 
        // * forcer f = 1000 dans ce mode debug, 
        // * ou exposer une distance de visualisation réglable. 
        // Le second est mieux — c'est exactement ce que fait le depth range d'un viewer.


        ctx->fb->SetPixelUnchecked(x, y, Color{ g, g, g, 255 });
    }

    void ShadeFragment_Barycentric(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
    {
        // w0 est le poids de v0, pas de l'arête v0v1

        auto* ctx = static_cast<FragmentContext*>(userData);   // ← alignement
        const float z = bary.w0 * ctx->z0 + bary.w1 * ctx->z1 + bary.w2 * ctx->z2;
        if (!ctx->db->TestAndSet(x, y, z)) return;

        ctx->fb->SetPixelUnchecked(x, y, Color{
            static_cast<uint8_t>(std::clamp(bary.w2, 0.f, 1.f) * 255.0f),   // b
            static_cast<uint8_t>(std::clamp(bary.w1, 0.f, 1.f) * 255.0f),   // g
            static_cast<uint8_t>(std::clamp(bary.w0, 0.f, 1.f) * 255.0f),   // r
            255 });
    }

   // struct CountContext { std::vector<uint8_t>* counts; int32_t width; };

    void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights&, void* userData)
    {
        auto* ctx = static_cast<CountContext*>(userData);
        (*ctx->counts)[y * ctx->width + x]++;   // ici width suffit : c'est TON buffer, pas SDL
    }

    void ShadeFragment_Solid(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
    {
        auto* ctx = static_cast<FragmentContext*>(userData);
        const float z = bary.w0 * ctx->z0 + bary.w1 * ctx->z1 + bary.w2 * ctx->z2;
        if (!ctx->db->TestAndSet(x, y, z)) return;          // REVERSE-Z : test GREATER
        ctx->fb->SetPixelUnchecked(x, y, ctx->color);
    }

} // namespace LV3