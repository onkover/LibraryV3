#pragma once
#include "Rasterizer.h"   // BarycentricWeights, FragmentCallback
#include "FrameBuffer.h"  // FrameBuffer, Color
#include "DepthBuffer.h"  // DepthBuffer
#include "Viewport.h"
#include "../Maths/MathUtils.h"   // Saturate (ShaderDepth)
#include <bit>


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


  
    // ════════════════════════════════════════════════════════════
    //  C1 : SHADERS RÉSOLUS À LA COMPILATION
    //  Un shader = une struct avec UNE fonction statique Shade(), inlinée
    //  dans la boucle pixels. Il reçoit la case de profondeur et la case
    //  couleur de SON pixel : plus d'index, plus de test de bornes, plus de void*.
    //  Shade() rend true si le fragment a passé le test de profondeur
    //  (compteur C0 PixelsDepthPassed, sous RASTER_STATS).
    //  SOURCE UNIQUE : les ShadeFragment_* (chemin générique) y délèguent.
    //  Remplace le DepthTestAndSet(FragmentContext*, x, y, z) de C0.
    // ════════════════════════════════════════════════════════════

    // ORDRE FIGÉ : w0·z0 + w1·z1 + w2·z2, de gauche à droite.
    // Toute réécriture « équivalente » change des bits (contre-exemple C1).
    [[nodiscard]] LV3_FORCEINLINE float InterpolateDepth(const FragmentContext& c, const BarycentricWeights& b) noexcept
    {
        return b.w0 * c.z0 + b.w1 * c.z1 + b.w2 * c.z2;
    }

    // Dénominateur perspectif : 1/w interpolé affinement, puis inversé par le shader.
    [[nodiscard]] LV3_FORCEINLINE float InterpolateInvW(const FragmentContext& c, const BarycentricWeights& b) noexcept
    {
        return b.w0 * c.invW0 + b.w1 * c.invW1 + b.w2 * c.invW2;
    }

    struct ShaderSolid
    {
        static LV3_FORCEINLINE bool Shade(const FragmentContext& c, const BarycentricWeights& b,
            float& depth, uint32_t& pixel) noexcept
        {
            // 1. PROFONDEUR affine, 2. EARLY-Z avant tout autre calcul (reverse-Z, GREATER)
            if (!DepthTestAndSet(depth, InterpolateDepth(c, b))) return false;
            pixel = std::bit_cast<uint32_t>(c.color);
            return true;
        }
    };

    // DEBUG : poids barycentriques en couleur (r = w0, g = w1, b = w2).
    struct ShaderBarycentric
    {
        static LV3_FORCEINLINE bool Shade(const FragmentContext& c, const BarycentricWeights& b,
            float& depth, uint32_t& pixel) noexcept
        {
            if (!DepthTestAndSet(depth, InterpolateDepth(c, b))) return false;
            pixel = std::bit_cast<uint32_t>(Color{
                static_cast<uint8_t>(std::clamp(b.w2, 0.f, 1.f) * 255.0f),   // b
                static_cast<uint8_t>(std::clamp(b.w1, 0.f, 1.f) * 255.0f),   // g
                static_cast<uint8_t>(std::clamp(b.w0, 0.f, 1.f) * 255.0f),   // r
                255 });
            return true;
        }
    };

    /*
    DEBUG : diagnostique la correction de perspective.
    * Bandes droites, continues, resserrées au loin         : ✅ correct
    * Bandes qui plient en V sur la diagonale des quads     : ❌ interpolation affine — le dénominateur n'est pas appliqué
    * Bandes régulièrement espacées jusqu'à l'horizon       : ❌ tu interpoles dist affinement au lieu de 1/dist
    * Bandes qui sautent d'un triangle à l'autre            : ❌ invW mal rempli sur certains sommets
    * Écran uniforme                                        : ❌ invW à zéro → dist = inf
    */
    struct ShaderLinearDepth
    {
        static LV3_FORCEINLINE bool Shade(const FragmentContext& c, const BarycentricWeights& b,
            float& depth, uint32_t& pixel) noexcept
        {
            if (!DepthTestAndSet(depth, InterpolateDepth(c, b))) return false;
            const float dist = 1.0f / InterpolateInvW(c, b);        // LA division perspective par pixel
            const uint8_t g = uint8_t((std::fmod(dist, 1.0f)) * 255.0f);   // bandes de 1 unité
            pixel = std::bit_cast<uint32_t>(MakeColor(g, g, g));
            return true;
        }
    };

    // DEBUG : distance à l'œil dans une plage d'affichage, avec gamma.
    struct ShaderDepth
    {
        static LV3_FORCEINLINE bool Shade(const FragmentContext& c, const BarycentricWeights& b,
            float& depth, uint32_t& pixel) noexcept
        {
            if (!DepthTestAndSet(depth, InterpolateDepth(c, b))) return false;
            const float dist = 1.0f / InterpolateInvW(c, b);        // w = d, et 1/w est linéaire en écran
            const float t = Saturate(dist / c.depthDisplayRange);   // plage de debug, pas la lentille
            const uint8_t g = uint8_t(std::pow(1.0f - t, 1.0f / 2.2f) * 255.0f);
            pixel = std::bit_cast<uint32_t>(MakeColor(g, g, g));
            return true;
        }
    };

    // FAUX EXPRÈS (TNR) : interpolation AFFINE de la distance au lieu de 1 / (1/w interpolé).
    struct ShaderLinearDepthWRONG
    {
        static LV3_FORCEINLINE bool Shade(const FragmentContext& c, const BarycentricWeights& b,
            float& depth, uint32_t& pixel) noexcept
        {
            if (!DepthTestAndSet(depth, InterpolateDepth(c, b))) return false;
            const float dist = b.w0 * (1.0f / c.invW0) + b.w1 * (1.0f / c.invW1) + b.w2 * (1.0f / c.invW2);
            const uint8_t g = uint8_t(std::fmod(dist, 1.0f) * 255.0f);
            pixel = std::bit_cast<uint32_t>(MakeColor(g, g, g));
            return true;
        }
    };

    // Puits des noyaux C1.
    // ctx est une COPIE, pas une référence : une écriture float dans depthRow[x]
    // pourrait, pour le compilateur, modifier ctx.z0 (même type, adresse inconnue).
    // Avec une référence, il relirait z0..z2 et invW0..2 après CHAQUE pixel écrit.
    // Une copie locale dont l'adresse ne fuit pas reste en registres. Coût : ~64 octets par triangle.
    template<class Shader>
    struct ShaderSink
    {
        const FragmentContext ctx;
        float* depthRow = nullptr;
        uint32_t* colorRow = nullptr;
        #if LV3_RASTER_STATS
            uint32_t depthPassed = 0;      // C0 : en registre, versé une fois par triangle
        #endif

        LV3_FORCEINLINE void BeginRow(int32_t y) noexcept
        {
            depthRow = ctx.db->Row(y);
            colorRow = ctx.fb->Row(y);
        }
        LV3_FORCEINLINE void Pixel(int32_t x, const BarycentricWeights& b) noexcept
        {
            [[maybe_unused]] const bool pass = Shader::Shade(ctx, b, depthRow[x], colorRow[x]);
            #if LV3_RASTER_STATS
                depthPassed += pass ? 1u : 0u;   // sans branche : setcc + add
            #endif
        }
    };

    // ── LE NOYAU C1 : une instance par shader, choisie UNE fois par vue (Renderer::SetMode).
    //    Un seul appel indirect par TRIANGLE (au lieu d'un par pixel couvert).
    //    ctx non const : seul le compteur C0 depthPassed y est écrit (RASTER_STATS),
    //    vidé ensuite par LV3_FLUSH_DEPTH_PASSED dans DrawTriangle, comme en C0.
    template<class Shader>
    void RasterizeTriangleT(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        const Viewport& vp, FragmentContext& ctx) noexcept
    {
        TriangleSetup s;
        if (!SetupTriangle(v0, v1, v2, vp, s)) { LV3_RASTER_RECORD(0u, 0u, 0u); return; }

        // Ce que SetPixel testait 4 fois par pixel, vérifié UNE fois par triangle, en Debug.
        LV3_ASSERT(ctx.fb != nullptr && ctx.db != nullptr && "RasterizeTriangleT : cibles non posees (BeginFrame ?)");
        LV3_ASSERT(s.bb.x0 >= 0 && s.bb.y0 >= 0 &&
            s.bb.x1 <= ctx.fb->Width() && s.bb.y1 <= ctx.fb->Height() &&
            "RasterizeTriangleT : boite hors du framebuffer (viewport plus grand que la cible ?)");

        #if LV3_ABLATION_PIXELS == 1
                AblationConsumeSetup(s);                                   // palier S : aucun pixel
        #elif LV3_ABLATION_PIXELS == 2
                AblationEdgeSink edges;                                    // palier E : arêtes, aucun fragment
                g_ablationSink = g_ablationSink + ForEachCoveredPixel(s, edges);
        #else
                ShaderSink<Shader> sink{ ctx };                            // COPIE du contexte (voir ShaderSink)
                [[maybe_unused]] const uint32_t covered = ForEachCoveredPixel(s, sink);
        #if LV3_RASTER_STATS
                ctx.depthPassed += sink.depthPassed;
        #endif
                LV3_RASTER_RECORD(s.bb.Area(), s.bb.Area(), covered);
        #endif
    }

    using TriangleKernel = void(*)(const Vec2f&, const Vec2f&, const Vec2f&,
        const Viewport&, FragmentContext&) noexcept;

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