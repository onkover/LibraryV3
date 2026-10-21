#pragma once
#include "Rasterizer.h"   // BarycentricWeights, FragmentCallback
#include "FrameBuffer.h"  // FrameBuffer, Color
#include "DepthBuffer.h"  // DepthBuffer

namespace LV3
{

#if LV3_ASSERTS_ENABLED
    // Sentinelle de vérification du contexte passé via void*.
    // Valeur arbitraire mais NON nulle et NON triviale : une struct mal castée
    // ou de la mémoire non initialisée a une probabilité négligeable de la produire.
    // Parade au bug n°17 (L05) : static_cast<T*>(void*) réussit TOUJOURS.
    inline constexpr uint32_t kFragmentContextMagic = 0xF2A9C0DEu;

#endif

    // ── Contextes de shading ──
    // Chaque fonction ShadeFragment_* définit SA propre structure de contexte.
    // C'est ce pointeur qui transite par le void* userData de RasterizeTriangle.


    struct FragmentContext
    {
        #if LV3_ASSERTS_ENABLED
                uint32_t magic = kFragmentContextMagic;   // existe pour qu'une assertion puisse vérifier qu'on n'a pas fait passer le mauvais void*
        #endif

        FrameBuffer* fb = nullptr;
        DepthBuffer* db = nullptr;
        Color        color{};
        float        z0 = 0.f, z1 = 0.f, z2 = 0.f;      // profondeurs AFFINES NDC (pas de correction) des 3 sommets

        float invW0 = 0.f, invW1 = 0.f, invW2 = 0.f;    // dénominateur perspectif
        // Le choix de 0.f est délibéré : si le remplissage est oublié, den = 0 et 1 / den = inf → l'écran devient visiblement faux. 
        // Un défaut qui échoue bruyamment vaut mieux qu'un défaut plausible.

        float depthDisplayRange = 100.f;   // debug seulement — PAS un paramètre de lentille
        
        #if LV3_RASTER_STATS
            // C0 : fragments ayant passe le Z pour le triangle COURANT.
            // Incremente par DepthTestAndSet, vide UNE fois par triangle par le Renderer.
            uint32_t depthPassed = 0;
        #endif
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
    #if LV3_ASSERTS_ENABLED
        LV3_ASSERT(ctx->magic == kFragmentContextMagic &&
            "FragmentContext : contexte incompatible derriere le void*");
    #endif    
        return ctx;
    }


    // Test de profondeur de TOUS les shaders : un seul endroit ou compter le surdessin.
    // Sans RASTER_STATS, c'est exactement l'ancien TestAndSet (inline, aucun surcout).
    [[nodiscard]] LV3_FORCEINLINE bool DepthTestAndSet(FragmentContext* ctx,
        int32_t x, int32_t y, float z) noexcept
    {
        const bool pass = ctx->db->TestAndSet(x, y, z);
        #if LV3_RASTER_STATS
                ctx->depthPassed += pass ? 1u : 0u;   // sans branche : setcc + add
        #endif
        return pass;
    }


    // ── Implémentations de FragmentCallback ──
    // Signature imposée par le type FragmentCallback défini dans Rasterizer.h

//    void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Depth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Barycentric(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_LinearDepth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Solid(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
    void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData); // TNR
    void ShadeFragment_LinearDepth_WRONG(int32_t x, int32_t y, const BarycentricWeights& b, void* userData); // TNR
} // namespace LV3