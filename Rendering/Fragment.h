#pragma once
#include "Rasterizer.h"   // BarycentricWeights, FragmentCallback
#include "FrameBuffer.h"  // FrameBuffer, Color
#include "DepthBuffer.h"  // DepthBuffer
#include "Viewport.h"
#include "../Maths/MathUtils.h"   // Saturate (ShaderDepth)
#include "PixelStage8.h"          // C2 : ForEachCoveredBlock8, kSimdMinBoxWidth
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

    //struct ShaderSolid
    //{
    //    static LV3_FORCEINLINE bool Shade(const FragmentContext& c, const BarycentricWeights& b,
    //        float& depth, uint32_t& pixel) noexcept
    //    {
    //        // 1. PROFONDEUR affine, 2. EARLY-Z avant tout autre calcul (reverse-Z, GREATER)
    //        if (!DepthTestAndSet(depth, InterpolateDepth(c, b))) return false;
    //        pixel = std::bit_cast<uint32_t>(c.color);
    //        return true;
    //    }
    //};
    // ── C2 : constantes d'un triangle diffusées sur 8 voies, UNE fois par triangle.
    struct Uniforms8
    {
        __m256  z0, z1, z2;     // profondeurs des 3 sommets
        __m256i color;          // couleur plate (ShaderSolid)
    };

    [[nodiscard]] LV3_FORCEINLINE Uniforms8 MakeUniforms8(const FragmentContext& c) noexcept
    {
        return { _mm256_set1_ps(c.z0), _mm256_set1_ps(c.z1), _mm256_set1_ps(c.z2),
                 _mm256_set1_epi32(int32_t(std::bit_cast<uint32_t>(c.color))) };
    }

    // InterpolateDepth voie par voie. MÊME parenthésage que le scalaire :
    // (w0·z0 + w1·z1) + w2·z2. Ni FMA, ni réassociation.
    [[nodiscard]] LV3_FORCEINLINE __m256 InterpolateDepth8(const Uniforms8& u,
        __m256 b0, __m256 b1, __m256 b2) noexcept
    {
        return _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(b0, u.z0), _mm256_mul_ps(b1, u.z1)),
            _mm256_mul_ps(b2, u.z2));
    }

    // DepthTestAndSet voie par voie. Rend le masque des voies qui PASSENT.
    //   scalaire : if (z <= dst) return false;   donc   passe  <=>  NON (z <= dst)
    //   _CMP_NLE_UQ = « non (inférieur ou égal) », VRAI si une valeur est NaN :
    //   exactement le comportement scalaire, NaN compris (dette relevée en C1).
    //   _CMP_GT_OQ serait FAUX pour NaN : bits différents (contre-exemple C2).
    // Chargement et écritures MASQUÉS : une voie hors couverture n'est ni lue ni
    // écrite, donc jamais d'accès au-delà de la fin de ligne ou du tampon.
    [[nodiscard]] LV3_FORCEINLINE __m256 DepthTestAndSet8(float* depth, __m256 z, __m256 cover) noexcept
    {
        const __m256 dst = _mm256_maskload_ps(depth, _mm256_castps_si256(cover));   // voies masquées : 0
        const __m256 pass = _mm256_and_ps(cover, _mm256_cmp_ps(z, dst, _CMP_NLE_UQ));
        _mm256_maskstore_ps(depth, _mm256_castps_si256(pass), z);
        return pass;
    }

    struct ShaderSolid
    {
        static LV3_FORCEINLINE bool Shade(const FragmentContext& c, const BarycentricWeights& b, float& depth, uint32_t& pixel) noexcept
        {
            // 1. PROFONDEUR affine, 2. EARLY-Z avant tout autre calcul (reverse-Z, GREATER)
            if (!DepthTestAndSet(depth, InterpolateDepth(c, b))) return false;
            pixel = std::bit_cast<uint32_t>(c.color);
            return true;
        }

        // C2 : les 8 voies d'un bloc. Même calcul que Shade(), voie par voie.
        static constexpr bool kHasShade8 = true;
        static LV3_FORCEINLINE uint32_t Shade8(const Uniforms8& u, __m256 b0, __m256 b1, __m256 b2, float* depth, uint32_t* pixel, __m256 cover) noexcept
        {
            const __m256 pass = DepthTestAndSet8(depth, InterpolateDepth8(u, b0, b1, b2), cover);
            _mm256_maskstore_epi32(reinterpret_cast<int*>(pixel), _mm256_castps_si256(pass), u.color);
            return uint32_t(_mm256_movemask_ps(pass));
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

    //// ── LE NOYAU C1 : une instance par shader, choisie UNE fois par vue (Renderer::SetMode).
    ////    Un seul appel indirect par TRIANGLE (au lieu d'un par pixel couvert).
    ////    ctx non const : seul le compteur C0 depthPassed y est écrit (RASTER_STATS),
    ////    vidé ensuite par LV3_FLUSH_DEPTH_PASSED dans DrawTriangle, comme en C0.
    //template<class Shader>
    //void RasterizeTriangleT(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
    //    const Viewport& vp, FragmentContext& ctx) noexcept
    //{
    //    TriangleSetup s;
    //    if (!SetupTriangle(v0, v1, v2, vp, s)) { LV3_RASTER_RECORD(0u, 0u, 0u); return; }

    //    // Ce que SetPixel testait 4 fois par pixel, vérifié UNE fois par triangle, en Debug.
    //    LV3_ASSERT(ctx.fb != nullptr && ctx.db != nullptr && "RasterizeTriangleT : cibles non posees (BeginFrame ?)");
    //    LV3_ASSERT(s.bb.x0 >= 0 && s.bb.y0 >= 0 &&
    //        s.bb.x1 <= ctx.fb->Width() && s.bb.y1 <= ctx.fb->Height() &&
    //        "RasterizeTriangleT : boite hors du framebuffer (viewport plus grand que la cible ?)");

    //    #if LV3_ABLATION_PIXELS == 1
    //            AblationConsumeSetup(s);                                   // palier S : aucun pixel
    //    #elif LV3_ABLATION_PIXELS == 2
    //            AblationEdgeSink edges;                                    // palier E : arêtes, aucun fragment
    //            g_ablationSink = g_ablationSink + ForEachCoveredPixel(s, edges);
    //    #else
    //            ShaderSink<Shader> sink{ ctx };                            // COPIE du contexte (voir ShaderSink)
    //            [[maybe_unused]] const uint32_t covered = ForEachCoveredPixel(s, sink);
    //    #if LV3_RASTER_STATS
    //            ctx.depthPassed += sink.depthPassed;
    //    #endif
    //            LV3_RASTER_RECORD(s.bb.Area(), s.bb.Area(), covered);
    //    #endif
    //}
    
    // ── C2 : puits du chemin AVX2. Reçoit un BLOC de 8 pixels d'une ligne.
    //    Shader avec Shade8 (ShaderSolid) : 8 voies d'un coup, chargements et écritures masqués.
    //    Shader sans Shade8 (shaders de debug : fmod, pow, division) : repli voie par voie
    //    sur le Shade() scalaire. Même résultat au bit près, sans vectoriser pow ni fmod.
    template<class Shader>
    struct ShaderSink8
    {
        const FragmentContext ctx;          // COPIE, comme ShaderSink (aliasing)
        const Uniforms8       u;            // diffusions : une fois par triangle
        float* depthRow = nullptr;
        uint32_t* colorRow = nullptr;
        #if LV3_RASTER_STATS
                uint32_t depthPassed = 0;
        #endif

        explicit ShaderSink8(const FragmentContext& c) noexcept : ctx(c), u(MakeUniforms8(c)) {}

        LV3_FORCEINLINE void BeginRow(int32_t y) noexcept
        {
            depthRow = ctx.db->Row(y);
            colorRow = ctx.fb->Row(y);
        }

        LV3_FORCEINLINE void Block(int32_t x, __m256 cover, uint32_t bits,
            __m256 b0, __m256 b1, __m256 b2) noexcept
        {
            uint32_t passBits = 0;
            if constexpr (requires { Shader::kHasShade8; })
            {
                (void)bits;
                passBits = Shader::Shade8(u, b0, b1, b2, depthRow + x, colorRow + x, cover);
            }
            else
            {
                (void)cover;
                alignas(32) float a0[8], a1[8], a2[8];
                _mm256_store_ps(a0, b0); _mm256_store_ps(a1, b1); _mm256_store_ps(a2, b2);
                for (uint32_t m = bits; m != 0; m &= m - 1)          // une itération par voie couverte
                {
                    const uint32_t i = _tzcnt_u32(m);                // indice de la voie la plus basse
                    if (Shader::Shade(ctx, BarycentricWeights{ a0[i], a1[i], a2[i] },
                        depthRow[x + int32_t(i)], colorRow[x + int32_t(i)]))
                        passBits |= 1u << i;
                }
            }
            #if LV3_RASTER_STATS
                        depthPassed += uint32_t(_mm_popcnt_u32(passBits));
            #else
                        (void)passBits;
            #endif
        }
    };

    // ── C2 : le chemin AVX2 d'un triangle, dans SA PROPRE fonction (jamais inline).
    //    Pourquoi : le prologue d'une fonction sauvegarde TOUS les registres xmm6-15
    //    qu'elle utilise, quel que soit le chemin pris (ABI Windows x64). Inliné dans
    //    le noyau, ce chemin alourdirait le prologue payé par CHAQUE triangle, même
    //    ceux qui prennent la boucle scalaire (V1 : 18 120 par frame, ≈ 4 ns déjà
    //    ajoutées par C1). Hors ligne, seul le triangle large paie un appel direct.
    template<class Shader>
    LV3_NOINLINE uint32_t RasterizeBlocks8(const TriangleSetup& s, FragmentContext& ctx) noexcept
    {
        ShaderSink8<Shader> sink(ctx);                                     // COPIE + diffusions
        const uint32_t covered = ForEachCoveredBlock8(s, sink);
        #if LV3_RASTER_STATS
                ctx.depthPassed += sink.depthPassed;
        #endif
        return covered;
    }

    // ── LE NOYAU C1 : une instance par shader, choisie UNE fois par vue (Renderer::SetMode).
    //    Un seul appel indirect par TRIANGLE (au lieu d'un par pixel couvert).
    //    ctx non const : seul le compteur C0 depthPassed y est écrit (RASTER_STATS),
    //    vidé ensuite par LV3_FLUSH_DEPTH_PASSED dans DrawTriangle, comme en C0.
    //    C2 : kMinSimdWidth choisit le chemin PAR TRIANGLE, d'après la largeur de sa boîte.
    //    Défaut kSimdMinBoxWidth ; les TNR forcent kScalarOnly (= C1) ou kSimdAlways.
    template<class Shader, int32_t kMinSimdWidth = kSimdMinBoxWidth>
    void RasterizeTriangleT(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,const Viewport& vp, FragmentContext& ctx) noexcept
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
            // palier E : arêtes, aucun fragment, MÊME aiguillage que le moteur normal
            if (s.bb.x1 - s.bb.x0 >= kMinSimdWidth)
            {
                AblationEdgeSink8 edges;
                g_ablationSink = g_ablationSink + ForEachCoveredBlock8(s, edges);
            }
            else
            {
                AblationEdgeSink edges;
                g_ablationSink = g_ablationSink + ForEachCoveredPixel(s, edges);
            }
        #else
            uint32_t covered;
            if (s.bb.x1 - s.bb.x0 >= kMinSimdWidth)                    // C2 : boîte assez large
            {
                covered = RasterizeBlocks8<Shader>(s, ctx);            // hors ligne : prologue du noyau inchangé
            }
            else                                                       // C1 : boîte étroite
            {
                ShaderSink<Shader> sink{ ctx };                        // COPIE du contexte (voir ShaderSink)
                covered = ForEachCoveredPixel(s, sink);
            #if LV3_RASTER_STATS
                        ctx.depthPassed += sink.depthPassed;
            #endif
            }
            LV3_RASTER_RECORD(s.bb.Area(), s.bb.Area(), covered);
            (void)covered;
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