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
      
        // D-a2 : bloc PLEIN (tuile acceptée). Les 8 voies sont dans le triangle ET dans la boîte :
        // lecture ordinaire de la profondeur, comparaison identique (NLE_UQ, NaN compris), écritures
        // masquées par le seul résultat du test Z. Même mémoire finale que Shade8(..., cover = tout).
        static LV3_FORCEINLINE uint32_t Shade8Full(const Uniforms8& u, __m256 b0, __m256 b1, __m256 b2, float* depth, uint32_t* pixel) noexcept
        {
            const __m256 z = InterpolateDepth8(u, b0, b1, b2);
            const __m256 dst = _mm256_loadu_ps(depth);                          // 8 voies valides : lecture simple
            const __m256 pass = _mm256_cmp_ps(z, dst, _CMP_NLE_UQ);             // même prédicat que DepthTestAndSet8
            _mm256_maskstore_ps(depth, _mm256_castps_si256(pass), z);
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

        // D-a2 : ligne suivante SANS recalcul (la hiérarchie revisite les lignes tuile par tuile).
        LV3_FORCEINLINE void NextRow() noexcept
        {
            depthRow += ctx.db->Width();
            colorRow = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(colorRow) + ctx.fb->PitchBytes());
        }

        LV3_FORCEINLINE void Pixel(int32_t x, const BarycentricWeights& b) noexcept
        {
            [[maybe_unused]] const bool pass = Shader::Shade(ctx, b, depthRow[x], colorRow[x]);
            #if LV3_RASTER_STATS
                depthPassed += pass ? 1u : 0u;   // sans branche : setcc + add
            #endif
        }
    };

   
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

        // D-a2 : pas d'une ligne, lus UNE fois par triangle (NextRow : 2 additions au lieu de BeginRow).
        const ptrdiff_t dStride;            // floats par ligne du Z-buffer
        const ptrdiff_t cStride;            // OCTETS par ligne du framebuffer (pitch SDL)

        explicit ShaderSink8(const FragmentContext& c) noexcept
            : ctx(c), u(MakeUniforms8(c)), dStride(c.db->Width()), cStride(c.fb->PitchBytes()) {}

        LV3_FORCEINLINE void BeginRow(int32_t y) noexcept
        {
            depthRow = ctx.db->Row(y);
            colorRow = ctx.fb->Row(y);
        }
        // D-a2 : la hiérarchie parcourt une tuile de 8 lignes : BeginRow une fois, puis NextRow.
        LV3_FORCEINLINE void NextRow() noexcept
        {
            depthRow += dStride;
            colorRow = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(colorRow) + cStride);
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

        // D-a2 : bloc PLEIN d'une tuile acceptée (8 voies couvertes, dans la boîte).
        //   Shader avec Shade8Full : lecture Z ordinaire, aucun masque de couverture.
        //   Sinon : le Block() normal, masque « tout », bits 0xFF (mêmes écritures).
        LV3_FORCEINLINE void BlockFull(int32_t x, __m256 allOn, __m256 b0, __m256 b1, __m256 b2) noexcept
        {
            if constexpr (requires { Shader::Shade8Full; })
            {
                (void)allOn;
                [[maybe_unused]] const uint32_t passBits = Shader::Shade8Full(u, b0, b1, b2, depthRow + x, colorRow + x);
       
                #if LV3_RASTER_STATS
                            depthPassed += uint32_t(_mm_popcnt_u32(passBits));
                #endif
            }
            else
            {
                Block(x, allOn, 0xFFu, b0, b1, b2);
            }
        }
    };

    // ── D-a1c : classe de LARGEUR de boîte d'un triangle dessiné par le noyau (RASTER_STATS).
    //    Classes : 1 | 2 | 3-4 | 5-8 | 9-16 | 17+. Sans RASTER_STATS, l'argument disparaît.
    #if LV3_RASTER_STATS
        LV3_FORCEINLINE void RecordBoxClass(int32_t w) noexcept
        {
            const uint32_t k = w <= 1 ? 0u : w == 2 ? 1u : w <= 4 ? 2u : w <= 8 ? 3u : w <= 16 ? 4u : 5u;
            LV3_PROF_COUNT(EProfCounter(uint32_t(EProfCounter::TrisBoxW1) + k), 1);
        }
        #define LV3_RASTER_BOXCLASS(w) ::LV3::RecordBoxClass(w)
    #else
        #define LV3_RASTER_BOXCLASS(w) ((void)0)
    #endif

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

    //// ── LE NOYAU C1 : une instance par shader, choisie UNE fois par vue (Renderer::SetMode).
    ////    Un seul appel indirect par TRIANGLE (au lieu d'un par pixel couvert).
    ////    ctx non const : seul le compteur C0 depthPassed y est écrit (RASTER_STATS),
    ////    vidé ensuite par LV3_FLUSH_DEPTH_PASSED dans DrawTriangle, comme en C0.
    ////    C2 : kMinSimdWidth choisit le chemin PAR TRIANGLE, d'après la largeur de sa boîte.
    ////    Défaut kSimdMinBoxWidth ; les TNR forcent kScalarOnly (= C1) ou kSimdAlways.
    //template<class Shader, int32_t kMinSimdWidth = kSimdMinBoxWidth>
    //void RasterizeTriangleT(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,const Viewport& vp, FragmentContext& ctx) noexcept
    //{


    // ── D-a1 : le chemin AVX2 en virgule fixe, hors ligne pour la même raison que C2.
    //    kWide choisit la largeur des voies À LA COMPILATION : deux instances par shader.
    //    PrepareLanesFx est faite ICI : les triangles étroits (boucle scalaire) ne la paient pas.
    //    kForce64 : TNR uniquement (les voies 64 bits doivent donner les mêmes pixels).
    template<class Shader, bool kForce64>
    LV3_NOINLINE uint32_t RasterizeBlocks8Fx(const TriangleSetup& s, EdgeSetupFx fx, FragmentContext& ctx) noexcept
    {
        PrepareLanesFx(s, fx);                                             // fx : COPIE locale, modifiée
        ShaderSink8<Shader> sink(ctx);                                     // COPIE + diffusions
        const uint32_t covered = (kForce64 || fx.wide) ? ForEachCoveredBlock8Fx64(s, fx, sink)
            : ForEachCoveredBlock8Fx32(s, fx, sink);
        #if LV3_RASTER_STATS
            ctx.depthPassed += sink.depthPassed;
        #endif

        return covered;
    }

    // ── D-a2 : la hiérarchie dans SA PROPRE fonction, hors ligne (leçon C2 et contre-exemple 2 de D-a1c) :
    //    dans RasterizeBlocks8Fx, ses registres ymm alourdiraient le prologue payé par TOUS les triangles
    //    Giesen (ABI Windows x64 : xmm6-15 sauvegardés à l'entrée), et changeraient le placement de la
    //    boucle Fx32. Mesuré sur V2 (aucun triangle tuilé) : +4 % quand la hiérarchie partageait la fonction.
    //    E' hors de int32 sur le domaine des tuiles (rare : longues arêtes clippées) : repli Giesen.
    template<class Shader>
    LV3_NOINLINE uint32_t RasterizeTiles8Fx(const TriangleSetup& s, const EdgeSetupFx& fx, FragmentContext& ctx) noexcept
    {
        if (!TilesFitInt32(s, fx)) return RasterizeBlocks8Fx<Shader, false>(s, fx, ctx);
        
        #if LV3_RASTER_STATS
                LV3_PROF_COUNT(EProfCounter::TrisTiled, 1);
        #endif

        ShaderSink8<Shader> sink(ctx);                                     // COPIE + diffusions
        const uint32_t covered = ForEachCoveredTile8Fx32(s, fx, sink);
        
        #if LV3_RASTER_STATS
                ctx.depthPassed += sink.depthPassed;
        #endif
        return covered;
    }

    //    D-a1c : kSmallPx = W* (pixels). Un triangle AVX2 PETIT (IsSmallFloatTri) prend les blocs
    //    flottants de C2 sur le TriangleSetup de la préparation entière : identique au bit à celui de
    //    SetupTriangle pour un petit triangle (sommets, échange, boîte, top-left, 1/aire exacts).
    //    Force64 (TNR des voies 64 bits) coupe l'aiguillage : chaque triangle doit y passer par Fx64.
    template<class Shader, int32_t kMinSimdWidth, EFixedLanes kLanes, int32_t kSmallPx, int32_t kTilePx>
    LV3_FORCEINLINE void RasterizeTriangleFx(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2, const Viewport& vp, FragmentContext& ctx) noexcept
    {
        TriangleSetup s;
        EdgeSetupFx   fx;
        if (!SetupTriangleFx(v0, v1, v2, vp, s, fx)) { LV3_RASTER_RECORD(0u, 0u, 0u); return; }

        LV3_ASSERT(ctx.fb != nullptr && ctx.db != nullptr && "RasterizeTriangleFx : cibles non posees (BeginFrame ?)");
        LV3_ASSERT(s.bb.x0 >= 0 && s.bb.y0 >= 0 &&
            s.bb.x1 <= ctx.fb->Width() && s.bb.y1 <= ctx.fb->Height() &&
            "RasterizeTriangleFx : boite hors du framebuffer (viewport plus grand que la cible ?)");

        constexpr bool kForce64 = (kLanes == EFixedLanes::Force64);
        constexpr int32_t kSmall = kForce64 ? 0 : kSmallPx;
        [[maybe_unused]] constexpr int32_t kTile = kForce64 ? 0 : kTilePx;  // D-a2 : Force64 coupe aussi la hiérarchie

        #if LV3_ABLATION_PIXELS == 1
            AblationConsumeSetupFx(s, fx);                             // palier S : aucun pixel
        #elif LV3_ABLATION_PIXELS == 2
            // palier E : couverture seule. Les arêtes FLOTTANTES ne servent plus qu'aux
            // barycentriques : jamais lues par ce puits, elles disparaissent à l'optimisation.
            // PrepareLanesFx compte dans E (comme dans le moteur : elle précède les voies).
            if (s.bb.x1 - s.bb.x0 >= kMinSimdWidth)
            {
                AblationEdgeSink8 edges;
                if (IsSmallFloatTri<kSmall>(fx))                        // D-a1c : MÊME aiguillage que le moteur
                {
                    g_ablationSink = g_ablationSink + ForEachCoveredBlock8(s, edges);
                }
                else
                {
                    if (IsTiledTri<kTile>(s) && TilesFitInt32(s, fx))      // D-a2 : MÊME aiguillage que le moteur
                    {
                        g_ablationSink = g_ablationSink + ForEachCoveredTile8Fx32(s, fx, edges);
                    }
                    else
                    {
                        PrepareLanesFx(s, fx);
                        g_ablationSink = g_ablationSink + ((kForce64 || fx.wide) ? ForEachCoveredBlock8Fx64(s, fx, edges)
                            : ForEachCoveredBlock8Fx32(s, fx, edges));
                    }
                }
            }
            else
            {
                AblationEdgeSink edges;
                g_ablationSink = g_ablationSink + (IsTiledTri<kTile>(s) ? ForEachCoveredPixelFxTiled(s, fx, edges)
                    : ForEachCoveredPixelFx(s, fx, edges));
            }
        #else
            uint32_t covered;
            LV3_RASTER_BOXCLASS(s.bb.x1 - s.bb.x0);                    // D-a1c : classe de largeur (RASTER_STATS)
            if (s.bb.x1 - s.bb.x0 >= kMinSimdWidth)                    // boîte assez large : AVX2
            {
                if (IsSmallFloatTri<kSmall>(fx))                       // D-a1c : petit -> blocs flottants C2
                {
                    // Les blocs flottants lisent s.p0..p2, les sommets REÇUS : ils doivent être sur la
                    // grille (contrat de l'étage sommets), sinon pixels différents de Giesen (contre-exemple 4).
                    LV3_ASSERT(IsOnSubpixelGrid(s.p0) && IsOnSubpixelGrid(s.p1) && IsOnSubpixelGrid(s.p2)
                        && "D-a1c : sommets non accroches au 1/256 px (contrat de l'etage sommets)");
                    covered = RasterizeBlocks8<Shader>(s, ctx);        // hors ligne, comme en C2
                #if LV3_RASTER_STATS
                    LV3_PROF_COUNT(EProfCounter::TrisSmallFloat, 1);
                #endif
                }
                else if (IsTiledTri<kTile>(s))                             // D-a2 : grand -> tuiles 8 x 8
                {
                    covered = RasterizeTiles8Fx<Shader>(s, fx, ctx);       // hors ligne : prologue inchangé
                }
                else
                {
                    covered = RasterizeBlocks8Fx<Shader, kForce64>(s, fx, ctx);    // hors ligne : prologue inchangé
                }
            }
            else                                                       // boîte étroite : scalaire int64
            {
            
                ShaderSink<Shader> sink{ ctx };
                // D-a2 : pendant scalaire de la hiérarchie (LV3_SIMD = 0 ou TNR kScalarOnly) pour les grandes boîtes.
                covered = IsTiledTri<kTile>(s) ? ForEachCoveredPixelFxTiled(s, fx, sink)
                    : ForEachCoveredPixelFx(s, fx, sink);
                    
                #if LV3_RASTER_STATS
                    ctx.depthPassed += sink.depthPassed;
                #endif
            }
            LV3_RASTER_RECORD(s.bb.Area(), s.bb.Area(), covered);
            (void)covered;
        #endif
    }

    // ── LE NOYAU C1 : une instance par shader, choisie UNE fois par vue (Renderer::SetMode).
    //    Un seul appel indirect par TRIANGLE (au lieu d'un par pixel couvert).
    //    ctx non const : seul le compteur C0 depthPassed y est écrit (RASTER_STATS),
    //    vidé ensuite par LV3_FLUSH_DEPTH_PASSED dans DrawTriangle, comme en C0.
    //    C2 : kMinSimdWidth choisit le chemin PAR TRIANGLE, d'après la largeur de sa boîte.
    //    Défaut kSimdMinBoxWidth ; les TNR forcent kScalarOnly (= C1) ou kSimdAlways.
    //    D-a1 : sous LV3_RASTER_FIXED, tout triangle dans la garde prend RasterizeTriangleFx ;
    //    le corps flottant ci-dessous ne sert plus qu'aux sommets hors garde (TrisFixedGuard).
   //    D-a1c : kSmallPx = W* (défaut kSmallFloatPx, config.h section 13) ; les TNR forcent
    //    kSmallFloatOff (Giesen pour tous, la référence) ou une valeur > 16 (contre-exemple).
    template<class Shader, int32_t kMinSimdWidth = kSimdMinBoxWidth, EFixedLanes kLanes = EFixedLanes::Auto, 
                int32_t kSmallPx = kSmallFloatPx, int32_t kTilePx = kTileMinPx>
    void RasterizeTriangleT(const Vec2f & v0, const Vec2f & v1, const Vec2f & v2, const Viewport & vp, FragmentContext & ctx) noexcept
    {
        #if LV3_RASTER_FIXED
            if (InFixedGuard(v0, v1, v2))
            {
                RasterizeTriangleFx<Shader, kMinSimdWidth, kLanes, kSmallPx, kTilePx>(v0, v1, v2, vp, ctx);
                return;
            }
            #if LV3_RASTER_STATS
                    LV3_PROF_COUNT(EProfCounter::TrisFixedGuard, 1);           // repli flottant C2
            #endif
        #endif

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
            LV3_RASTER_BOXCLASS(s.bb.x1 - s.bb.x0);                    // D-a1c : classe de largeur (RASTER_STATS)
            
            if (s.bb.x1 - s.bb.x0 >= kMinSimdWidth)
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