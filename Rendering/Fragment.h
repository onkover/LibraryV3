#pragma once
#include "Rasterizer.h"   // BarycentricWeights, FragmentCallback
#include "FrameBuffer.h"  // FrameBuffer, Color
#include "DepthBuffer.h"  // DepthBuffer

namespace LV3
{

#ifdef _DEBUG
    // Sentinelle de vérification du contexte passé via void*.
    // Valeur arbitraire mais NON nulle et NON triviale : une struct mal castée
    // ou de la mémoire non initialisée a une probabilité négligeable de la produire.
    // Parade au bug n°17 (L05) : static_cast<T*>(void*) réussit TOUJOURS.
    inline constexpr uint32_t kFragmentContextMagic = 0xF2A9C0DEu;
#endif

    // ── Contextes de shading ──
    // Chaque fonction ShadeFragment_* définit SA propre structure de contexte.
    // C'est ce pointeur qui transite par le void* userData de RasterizeTriangle.

    //struct UnlitContext
    //{
    //    FrameBuffer* fb;
    //    Color        color;
    //};

    //struct DepthContext
    //{
    //    FrameBuffer* fb;
    //    float z0, z1, z2; // profondeur des 3 sommets du triangle courant
    //};

    struct FragmentContext
    {
        #ifdef _DEBUG
                uint32_t magic = kFragmentContextMagic;   // vérifié par assertion au cast
        #endif

        FrameBuffer* fb = nullptr;
        DepthBuffer* db = nullptr;
        Color        color{};
        float        z0 = 0.f, z1 = 0.f, z2 = 0.f;      // profondeurs AFFINES NDC (pas de correction) des 3 sommets

        float invW0 = 0.f, invW1 = 0.f, invW2 = 0.f;    // dénominateur perspectif
        // Le choix de 0.f est délibéré : si le remplissage est oublié, den = 0 et 1 / den = inf → l'écran devient visiblement faux. 
        // Un défaut qui échoue bruyamment vaut mieux qu'un défaut plausible.

        float depthDisplayRange = 100.f;   // debug seulement — PAS un paramètre de lentille
    };

    // Comptage de couverture — utilise par la TNR top-left.
    struct CountContext
    {
        std::vector<uint8_t>* counts = nullptr; 
        int32_t width = 0;
    };

    [[nodiscard]] LV3_FORCEINLINE FragmentContext* AsFragmentContext(void* userData) noexcept
    {
        auto* ctx = static_cast<FragmentContext*>(userData);
    
        LV3_ASSERT(ctx != nullptr && "FragmentContext : userData nul");
        LV3_ASSERT(ctx->magic == kFragmentContextMagic &&
            "FragmentContext : contexte incompatible derriere le void*");
        
        return ctx;
    }


    // ── Implémentations de FragmentCallback ──
    // Signature imposée par le type FragmentCallback défini dans Rasterizer.h

//    void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Depth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Barycentric(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_LinearDepth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Solid(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData); // TNR
} // namespace LV3