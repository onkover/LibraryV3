#pragma once
// ════════════════════════════════════════════════════════════
//  C2 : ÉTAGE PIXELS PAR BLOCS DE 8 (AVX2)
//
//  Les 3 fonctions d'arête sont évaluées DIRECTEMENT pour 8 pixels voisins
//  d'une même ligne, avec les MÊMES opérations que le scalaire de C1 :
//      ux = ax - px ; uy = ay - py ; vx = bx - px ; vy = by - py
//      w  = ux*vy - uy*vx
//  Chaque voie refait exactement le calcul d'un pixel : résultat identique
//  au bit près. JAMAIS d'incrémental (C0), JAMAIS de FMA (B3), jamais de rcp.
//
//  Organisation, comme FaceStage.h (B4) : ce fichier ne connaît ni les
//  shaders ni les tampons. Il produit, par bloc, un MASQUE de couverture et
//  les barycentriques ; le puits (Fragment.h) fait le reste.
// ════════════════════════════════════════════════════════════
#include <immintrin.h>    // AVX2 : __m256 (8 float), __m256i (8 int32), _mm256_*
#include <cstdint>
#include "Rasterizer.h"   // TriangleSetup, BarycentricWeights
#if LV3_RASTER_STATS
    #include "../Core/Profiler.h"
#endif

namespace LV3
{
    // ── Seuil de largeur de boîte (pixels) à partir duquel un triangle prend
    //    le chemin AVX2. En dessous, la boucle scalaire de C1 : un bloc de 8
    //    voies pour 1 ou 2 pixels utiles coûte plus qu'il ne rapporte
    //    (V1 : TOUTES les boîtes font 1 pixel de large).
    //    Valeur de départ, à régler par mesure (compteurs TrisSimd8 / LaneBlocks8).
    //    LV3_SIMD = 0 (config.h, section 11) : pendant scalaire pour TOUS les triangles.
    #if LV3_SIMD        
        inline constexpr int32_t kSimdMinBoxWidth = 3;
    #else
        inline constexpr int32_t kSimdMinBoxWidth = INT32_MAX;
    #endif

    // Valeurs de TEST du paramètre de template de RasterizeTriangleT :
    inline constexpr int32_t kScalarOnly = INT32_MAX;   // force le chemin C1 (scalaire)
    inline constexpr int32_t kSimdAlways = 0;           // force le chemin C2 (AVX2), même pour 1 pixel

    // bool -> masque de voie : vrai = 0xFFFFFFFF sur les 8 voies, faux = 0.
    [[nodiscard]] LV3_FORCEINLINE __m256 BoolMask8(bool b) noexcept
    {
        return _mm256_castsi256_ps(_mm256_set1_epi32(-int32_t(b)));
    }

    // EdgeFunction voie par voie. MÊME ordre d'opérations que la version scalaire
    // (Rasterizer.h) : 4 soustractions, 2 multiplications, 1 soustraction.
    // _mm256_mul_ps puis _mm256_sub_ps : DEUX arrondis, comme le scalaire.
    // Un _mm256_fmsub_ps n'en ferait qu'un : bits différents (contre-exemple C2).
    [[nodiscard]] LV3_FORCEINLINE __m256 EdgeFunction8(__m256 ax, __m256 ay, __m256 bx, __m256 by,
        __m256 px, __m256 py) noexcept
    {
        const __m256 ux = _mm256_sub_ps(ax, px);      // ax - px
        const __m256 uy = _mm256_sub_ps(ay, py);      // ay - py
        const __m256 vx = _mm256_sub_ps(bx, px);      // bx - px
        const __m256 vy = _mm256_sub_ps(by, py);      // by - py
        return _mm256_sub_ps(_mm256_mul_ps(ux, vy),   // ux*vy - uy*vx
            _mm256_mul_ps(uy, vx));
    }

    // InsideEdge voie par voie, SANS branche :
    //     w > 0  OU  (w == 0  ET  tl)
    // _CMP_GT_OQ et _CMP_EQ_OQ : « ordonnés », faux si w est NaN, comme w > 0 et w == 0
    // en C++. -0.0 == 0 est vrai : identique au scalaire.
    [[nodiscard]] LV3_FORCEINLINE __m256 InsideEdge8(__m256 w, __m256 tlMask) noexcept
    {
        const __m256 zero = _mm256_setzero_ps();
        const __m256 gt = _mm256_cmp_ps(w, zero, _CMP_GT_OQ);              // w > 0
        const __m256 eq = _mm256_cmp_ps(w, zero, _CMP_EQ_OQ);              // w == 0
        return _mm256_or_ps(gt, _mm256_and_ps(eq, tlMask));                // gt | (eq & tl)
    }

    // Parcourt la boîte par blocs de 8 pixels d'une même ligne.
    //   sink.BeginRow(y)                          : une fois par ligne
    //   sink.Block(x, cover, bits, b0, b1, b2)    : par bloc ayant AU MOINS un pixel couvert
    //       x     : premier pixel du bloc (voie 0)
    //       cover : masque 8 voies (0xFFFFFFFF = couvert ET dans la boîte)
    //       bits  : le même masque en 8 bits (bit i = voie i)
    //       b0..2 : barycentriques dans l'ordre du triangle D'ORIGINE (échange p1<->p2 défait)
    // Rend le nombre de pixels couverts (même valeur que ForEachCoveredPixel).
    template<class Sink8>
    [[nodiscard]] LV3_FORCEINLINE uint32_t ForEachCoveredBlock8(const TriangleSetup& s, Sink8& sink) noexcept
    {
        // ── C2b : les champs de s LUS DANS LES BOUCLES sont copiés dans des locales.
        //    s arrive par RÉFÉRENCE : pour le compilateur, une écriture d'entier
        //    (couleur, vpmaskmovd) ou de float (profondeur) dans le tampon pourrait
        //    tomber sur s.bb.x1 ou s.flipped. Il les relisait donc en mémoire à CHAQUE
        //    bloc (asm C2 : [r9+32], [r9+44]). Une locale dont l'adresse n'est jamais
        //    prise ne peut être modifiée par aucun pointeur : elle reste en registre.
        const int32_t bx0 = s.bb.x0, bx1 = s.bb.x1;
        const int32_t by0 = s.bb.y0, by1 = s.bb.y1;
        const bool    flipped = s.flipped;


        // ── Diffusions : UNE fois par triangle (≈ 14 instructions).
        const __m256  half = _mm256_set1_ps(0.5f);
        const __m256i lane = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);    // décalage de chaque voie
        const __m256i x1 = _mm256_set1_epi32(bx1);                   // borne EXCLUSIVE de la boîte
        const __m256  p0x = _mm256_set1_ps(s.p0.x), p0y = _mm256_set1_ps(s.p0.y);
        const __m256  p1x = _mm256_set1_ps(s.p1.x), p1y = _mm256_set1_ps(s.p1.y);
        const __m256  p2x = _mm256_set1_ps(s.p2.x), p2y = _mm256_set1_ps(s.p2.y);
        const __m256  tl0 = BoolMask8(s.tl0), tl1 = BoolMask8(s.tl1), tl2 = BoolMask8(s.tl2);
        const __m256  inv = _mm256_set1_ps(s.invArea);

        uint32_t covered = 0;

        #if LV3_RASTER_STATS
                uint32_t blocks = 0;
        #endif
        for (int32_t y = by0; y < by1; ++y)
        {
            // py calculé en SCALAIRE puis diffusé : exactement le float(y) + 0.5f de C1.
            const __m256 py = _mm256_set1_ps(float(y) + 0.5f);
            sink.BeginRow(y);

            for (int32_t x = bx0; x < bx1; x += 8)
            {
                // Abscisses des 8 voies : x, x+1, ..., x+7 (entiers), puis centres x+i+0.5.
                // cvtepi32 est EXACT (|x| < 2^24) : même float que float(x+i) en scalaire.
                const __m256i xi = _mm256_add_epi32(_mm256_set1_epi32(x), lane);
                const __m256  px = _mm256_add_ps(_mm256_cvtepi32_ps(xi), half);

                // Voies au-delà de la boîte (x+i >= x1) : hors du triangle PAR CONSTRUCTION,
                // et surtout hors du tampon possible en fin de ligne. Masquées ici.
                const __m256 inBox = _mm256_castsi256_ps(_mm256_cmpgt_epi32(x1, xi));   // x1 > x+i

                // Les 3 arêtes, dans l'ordre de C1 : (p1,p2), (p2,p0), (p0,p1).
                const __m256 w0 = EdgeFunction8(p1x, p1y, p2x, p2y, px, py);
                const __m256 w1 = EdgeFunction8(p2x, p2y, p0x, p0y, px, py);
                const __m256 w2 = EdgeFunction8(p0x, p0y, p1x, p1y, px, py);

                // Couverture : ET des 3 tests et de la boîte. Pas de sortie anticipée :
                // en SIMD, les 3 arêtes coûtent le même prix quelle que soit la réponse.
                const __m256 cover = _mm256_and_ps(
                    _mm256_and_ps(InsideEdge8(w0, tl0), InsideEdge8(w1, tl1)),
                    _mm256_and_ps(InsideEdge8(w2, tl2), inBox));

                #if LV3_RASTER_STATS
                    ++blocks;
                #endif

                // movemask : bit i = signe de la voie i = 1 si couverte.
                const uint32_t bits = uint32_t(_mm256_movemask_ps(cover));
                if (bits == 0) continue;                 // bloc vide : aucun accès mémoire
                covered += uint32_t(_mm_popcnt_u32(bits)); // popcnt direct (pas std::popcount : test CPU à l'exécution chez MSVC)

                // Barycentriques : w * (1/aire), voie par voie, puis échange défait.
                // Identique au scalaire : { w0*inv, flipped ? w2*inv : w1*inv, ... }.
                const __m256 b0 = _mm256_mul_ps(w0, inv);
                const __m256 b1 = _mm256_mul_ps(flipped ? w2 : w1, inv);
                const __m256 b2 = _mm256_mul_ps(flipped ? w1 : w2, inv);

                sink.Block(x, cover, bits, b0, b1, b2);
            }
        }
        #if LV3_RASTER_STATS
                LV3_PROF_COUNT(EProfCounter::TrisSimd8, 1);
                LV3_PROF_COUNT(EProfCounter::LaneBlocks8, blocks);
        #endif
        return covered;
    }

    // ════════════════════════════════════════════════════════════
    //  D-a1 : BLOCS DE 8 PIXELS, ARÊTES ENTIÈRES AVANCÉES PAR ADDITIONS
    // 
    //  Les barycentriques : la formule directe de C2, sur les blocs non vides
    // 
    //  Par bloc, au lieu des 3 x 7 opérations flottantes de C2 + 3 x 4 comparaisons :
    //      tester  : OU des 3 arêtes (+ la boîte), 1 movemask      -> 3 OU + 1 movemask
    //      avancer : une addition par arête                         -> 3 (ou 4) additions
    //  Les arêtes flottantes ne sont plus calculées QUE pour les barycentriques,
    //  et seulement dans les blocs qui ont au moins un pixel couvert.
    // ════════════════════════════════════════════════════════════

    // Barycentriques d'un bloc : la formule DIRECTE de C2 (EdgeFunction8 * 1/aire),
    // sur les sommets accrochés de s. Partagée par les deux largeurs de voie.
    struct BaryInputs8
    {
        __m256 p0x, p0y, p1x, p1y, p2x, p2y, inv, half;
        __m256i lane;
        bool flipped;
    };

    [[nodiscard]] LV3_FORCEINLINE BaryInputs8 MakeBaryInputs8(const TriangleSetup& s) noexcept
    {
        return { _mm256_set1_ps(s.p0.x), _mm256_set1_ps(s.p0.y),
                 _mm256_set1_ps(s.p1.x), _mm256_set1_ps(s.p1.y),
                 _mm256_set1_ps(s.p2.x), _mm256_set1_ps(s.p2.y),
                 _mm256_set1_ps(s.invArea), _mm256_set1_ps(0.5f),
                 _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7), s.flipped };
    }

    LV3_FORCEINLINE void BlockBary8(const BaryInputs8& bi, int32_t x, __m256 py,
        __m256& b0, __m256& b1, __m256& b2) noexcept
    {
        // Centres x+i+0.5 : même calcul que C2 (cvtepi32 exact, puis + 0.5).
        const __m256 px = _mm256_add_ps(_mm256_cvtepi32_ps(_mm256_add_epi32(_mm256_set1_epi32(x), bi.lane)), bi.half);
        const __m256 w0 = EdgeFunction8(bi.p1x, bi.p1y, bi.p2x, bi.p2y, px, py);
        const __m256 w1 = EdgeFunction8(bi.p2x, bi.p2y, bi.p0x, bi.p0y, px, py);
        const __m256 w2 = EdgeFunction8(bi.p0x, bi.p0y, bi.p1x, bi.p1y, px, py);
        b0 = _mm256_mul_ps(w0, bi.inv);
        b1 = _mm256_mul_ps(bi.flipped ? w2 : w1, bi.inv);
        b2 = _mm256_mul_ps(bi.flipped ? w1 : w2, bi.inv);
    }



    // ── VOIES 32 BITS : voies 32 bits : 8 pixels, 4 OU, 1 movemask 
    //    le cas courant. Précondition : !fx.wide (préparation).
    //    Registre d'arête k : [E'(x) E'(x+1) ... E'(x+7)] en int32.
    template<class Sink8>
    [[nodiscard]] LV3_FORCEINLINE uint32_t ForEachCoveredBlock8Fx32(const TriangleSetup& s, const EdgeSetupFx& fx, Sink8& sink) noexcept
    {
        const int32_t bx0 = s.bb.x0, bx1 = s.bb.x1;           // C2b : locales, jamais relues en mémoire
        const int32_t by0 = s.bb.y0, by1 = s.bb.y1;
        const BaryInputs8 bi = MakeBaryInputs8(s);

        // ── Une fois par triangle. Les conversions int64 -> int32 sont EXACTES :
        //    la préparation a prouvé que E', 7 A' et 8 A' tiennent sur la boîte étendue.
        const __m256i lane = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
        const __m256i a0 = _mm256_set1_epi32(int32_t(fx.a[0]));
        const __m256i a1 = _mm256_set1_epi32(int32_t(fx.a[1]));
        const __m256i a2 = _mm256_set1_epi32(int32_t(fx.a[2]));
        const __m256i off0 = _mm256_mullo_epi32(lane, a0);    // [0  A'  2A' ... 7A'] : décalage de chaque voie
        const __m256i off1 = _mm256_mullo_epi32(lane, a1);
        const __m256i off2 = _mm256_mullo_epi32(lane, a2);
        const __m256i step0 = _mm256_slli_epi32(a0, 3);       // 8 A' : d'un bloc au suivant
        const __m256i step1 = _mm256_slli_epi32(a1, 3);
        const __m256i step2 = _mm256_slli_epi32(a2, 3);

        // 4e « arête » : la BOÎTE. eb = (x1 - 1) - (x + i) >= 0  <=>  la voie est dans la boîte.
        // Elle entre dans le même OU que les trois autres : plus de comparaison de boîte.
        const __m256i boxRow = _mm256_sub_epi32(_mm256_set1_epi32(bx1 - 1 - bx0), lane);
        const __m256i boxStep = _mm256_set1_epi32(-8);
        const __m256i minus1 = _mm256_set1_epi32(-1);

        int32_t r0 = int32_t(fx.e[0]), r1 = int32_t(fx.e[1]), r2 = int32_t(fx.e[2]);
        const int32_t rb0 = int32_t(fx.b[0]), rb1 = int32_t(fx.b[1]), rb2 = int32_t(fx.b[2]);

        uint32_t covered = 0;

        #if LV3_RASTER_STATS
            uint32_t blocks = 0;
        #endif

        for (int32_t y = by0; y < by1; ++y)
        {
            const __m256 py = _mm256_set1_ps(float(y) + 0.5f);
            sink.BeginRow(y);

            // Début de ligne : E' du pixel x0, plus le décalage de chaque voie.
            __m256i e0 = _mm256_add_epi32(_mm256_set1_epi32(r0), off0);
            __m256i e1 = _mm256_add_epi32(_mm256_set1_epi32(r1), off1);
            __m256i e2 = _mm256_add_epi32(_mm256_set1_epi32(r2), off2);
            __m256i eb = boxRow;

            for (int32_t x = bx0; x < bx1; x += 8)
            {
                // ── TESTER : le bit de signe de (e0 | e1 | e2 | eb) vaut 1 dès qu'UNE valeur est < 0.
                const __m256i any = _mm256_or_si256(_mm256_or_si256(e0, e1), _mm256_or_si256(e2, eb));
                const uint32_t bits = ~uint32_t(_mm256_movemask_ps(_mm256_castsi256_ps(any))) & 0xFFu;

                // ── AVANCER : 8 pixels à droite. AVANT le continue : un bloc vide avance aussi.
                e0 = _mm256_add_epi32(e0, step0);
                e1 = _mm256_add_epi32(e1, step1);
                e2 = _mm256_add_epi32(e2, step2);
                eb = _mm256_add_epi32(eb, boxStep);

                #if LV3_RASTER_STATS
                    ++blocks;
                #endif
                if (bits == 0) continue;                      // bloc vide : 4 OU, 1 movemask, 4 additions
                covered += uint32_t(_mm_popcnt_u32(bits));

                // Masque de voie complet (0xFFFFFFFF / 0) : any > -1  <=>  any >= 0.
                const __m256 cover = _mm256_castsi256_ps(_mm256_cmpgt_epi32(any, minus1));

                __m256 b0, b1, b2;
                BlockBary8(bi, x, py, b0, b1, b2);
                sink.Block(x, cover, bits, b0, b1, b2);
            }
            r0 += rb0; r1 += rb1; r2 += rb2;                  // une ligne plus bas : 3 additions scalaires
        }
        #if LV3_RASTER_STATS
            LV3_PROF_COUNT(EProfCounter::TrisSimd8, 1);
            LV3_PROF_COUNT(EProfCounter::LaneBlocks8, blocks);
        #endif
        return covered;
    }

    // ── VOIES 64 BITS :  voies 64 bits : les longues arêtes 
    //   triangles dont E' sort de int32 sur la boîte (longues arêtes
    //    de triangles clippés au near). Même algorithme, 8 pixels = 2 registres de 4 int64.
    //    Mêmes entiers, donc EXACTEMENT les mêmes pixels que la voie 32 bits (TNR).
    template<class Sink8>
    [[nodiscard]] LV3_FORCEINLINE uint32_t ForEachCoveredBlock8Fx64(const TriangleSetup& s, const EdgeSetupFx& fx, Sink8& sink) noexcept
    {
        const int32_t bx0 = s.bb.x0, bx1 = s.bb.x1;
        const int32_t by0 = s.bb.y0, by1 = s.bb.y1;
        const BaryInputs8 bi = MakeBaryInputs8(s);

        // Décalages par voie, calculés en scalaire int64 : [0 A' 2A' 3A'] et [4A' 5A' 6A' 7A'].
        const int64_t A0 = fx.a[0], A1 = fx.a[1], A2 = fx.a[2];
        const __m256i lo0 = _mm256_setr_epi64x(0, A0, 2 * A0, 3 * A0), hi0 = _mm256_setr_epi64x(4 * A0, 5 * A0, 6 * A0, 7 * A0);
        const __m256i lo1 = _mm256_setr_epi64x(0, A1, 2 * A1, 3 * A1), hi1 = _mm256_setr_epi64x(4 * A1, 5 * A1, 6 * A1, 7 * A1);
        const __m256i lo2 = _mm256_setr_epi64x(0, A2, 2 * A2, 3 * A2), hi2 = _mm256_setr_epi64x(4 * A2, 5 * A2, 6 * A2, 7 * A2);
        const __m256i st0 = _mm256_set1_epi64x(8 * A0), st1 = _mm256_set1_epi64x(8 * A1), st2 = _mm256_set1_epi64x(8 * A2);

        // Bit i de la voie i : pour reconstruire un masque de voie depuis les bits du bloc.
        const __m256i laneBit = _mm256_setr_epi32(1, 2, 4, 8, 16, 32, 64, 128);

        int64_t r0 = fx.e[0], r1 = fx.e[1], r2 = fx.e[2];

        uint32_t covered = 0;
        #if LV3_RASTER_STATS
            uint32_t blocks = 0;
        #endif

        for (int32_t y = by0; y < by1; ++y)
        {
            const __m256 py = _mm256_set1_ps(float(y) + 0.5f);
            sink.BeginRow(y);

            __m256i e0l = _mm256_add_epi64(_mm256_set1_epi64x(r0), lo0), e0h = _mm256_add_epi64(_mm256_set1_epi64x(r0), hi0);
            __m256i e1l = _mm256_add_epi64(_mm256_set1_epi64x(r1), lo1), e1h = _mm256_add_epi64(_mm256_set1_epi64x(r1), hi1);
            __m256i e2l = _mm256_add_epi64(_mm256_set1_epi64x(r2), lo2), e2h = _mm256_add_epi64(_mm256_set1_epi64x(r2), hi2);

            for (int32_t x = bx0; x < bx1; x += 8)
            {
                // Signe des 3 arêtes : OU, puis movemask_pd = 1 bit par voie de 64 bits.
                const __m256i anyL = _mm256_or_si256(_mm256_or_si256(e0l, e1l), e2l);   // voies 0..3
                const __m256i anyH = _mm256_or_si256(_mm256_or_si256(e0h, e1h), e2h);   // voies 4..7
                const uint32_t neg = uint32_t(_mm256_movemask_pd(_mm256_castsi256_pd(anyL)))
                    | (uint32_t(_mm256_movemask_pd(_mm256_castsi256_pd(anyH))) << 4);

                // Boîte en SCALAIRE : n voies utiles dans ce bloc.
                const int32_t n = bx1 - x;
                const uint32_t box = n >= 8 ? 0xFFu : ((1u << n) - 1u);
                const uint32_t bits = ~neg & box;

                e0l = _mm256_add_epi64(e0l, st0); e0h = _mm256_add_epi64(e0h, st0);
                e1l = _mm256_add_epi64(e1l, st1); e1h = _mm256_add_epi64(e1h, st1);
                e2l = _mm256_add_epi64(e2l, st2); e2h = _mm256_add_epi64(e2h, st2);

                #if LV3_RASTER_STATS
                    ++blocks;
                #endif
                if (bits == 0) continue;
                covered += uint32_t(_mm_popcnt_u32(bits));

                // bits -> masque de voie : (bits & bit_i) == bit_i.
                const __m256i bv = _mm256_and_si256(_mm256_set1_epi32(int32_t(bits)), laneBit);
                const __m256 cover = _mm256_castsi256_ps(_mm256_cmpeq_epi32(bv, laneBit));

                __m256 b0, b1, b2;
                BlockBary8(bi, x, py, b0, b1, b2);
                sink.Block(x, cover, bits, b0, b1, b2);
            }
            r0 += fx.b[0]; r1 += fx.b[1]; r2 += fx.b[2];
        }
        #if LV3_RASTER_STATS
            LV3_PROF_COUNT(EProfCounter::TrisSimd8, 1);
            LV3_PROF_COUNT(EProfCounter::LaneBlocks8, blocks);
            LV3_PROF_COUNT(EProfCounter::TrisFixedWide, 1);
        #endif

        return covered;
    }


    #if LV3_ABLATION_PIXELS
        // Palier E (C0) pour le chemin AVX2 : arêtes et masques, AUCUN fragment.
        struct AblationEdgeSink8
        {
            LV3_FORCEINLINE void BeginRow(int32_t) noexcept {}
            LV3_FORCEINLINE void Block(int32_t, __m256, uint32_t, __m256, __m256, __m256) noexcept {}
        };
    #endif
}