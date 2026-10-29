#pragma once

// ============================================================
//  Rendering/FaceStage.h — l'etage FACES par lots de 8 (chantier B4, phase G)
//
//  ROLE : pour 8 triangles consecutifs, repondre SANS BRANCHE a deux questions :
//    1. la face est-elle AVANT ?             == !IsBackFacing(EdgeFunction(a, b, c))
//    2. sa boite serree est-elle non vide ?  == !TightPixelBox(a, b, c, vp).Empty()
//  Les 8 reponses tiennent dans deux octets de bits (FaceMasks8).
//  Le DESSIN des faces retenues reste scalaire (RenderSystem.cpp).
//
//  LECTURE DES COMMENTAIRES DE REGISTRES
//    Un __m256 contient 8 floats (ou 8 int32), ecrits ici dans l'ORDRE MEMOIRE :
//        [ c0 c1 c2 c3 | c4 c5 c6 c7 ]      c = couloir
//          moitie basse  moitie haute       (128 bits chacune)
//    La barre | rappelle que unpack et shuffle travaillent DANS chaque moitie,
//    jamais a travers. ATTENTION : la doc Intel dessine l'inverse (couloir 7 a gauche).
//
//  REGLE DU BIT PRES
//    Chaque intrinseque reproduit UNE operation du scalaire, dans le MEME ordre :
//    pas de FMA, pas de rcp, min/max dans l'ordre (droite, gauche).
// 
// /!\ Une convention en tête de fichier sert à tout lire : 
// * les registres sont écrits dans l’ordre mémoire, couloir 0 à gauche, et | marque la frontière des 128 bits. 
// * La doc Intel fait l’inverse.
// ============================================================

#include <immintrin.h>    // AVX2 : __m256 (8 float), __m256i (8 int32), _mm256_*
#include <cstdint>
#include "VertexStage.h"  // RasterVertex { x, y, z, invW } : 16 octets, AoS
#include "Viewport.h"
#include <cmath>         // B5 : std::sqrt, std::fabs (ObjBackThreshold)
#include "PixelStage8.h"  // C3 : EdgeFunction8, InsideEdge8 ; et via Rasterizer.h : SetupTriangle

namespace LV3
{
    // Resultat d'un lot. Bit k <-> face k du lot (bit 0 = premiere face du lot).
    struct FaceMasks8
    {
        uint32_t front;     // bit k = 1 : face AVANT (aire < 0, ou NaN)
        uint32_t survive;   // bit k = 1 : face AVANT ET boite non vide (survive ⊆ front)
    };

    // ════════════════════════════════════════════════════════════
    //  ETAPE 1 — RASSEMBLER x et y du coin k (0 = a, 1 = b, 2 = c) des 8 faces
    // ════════════════════════════════════════════════════════════
    //  En memoire (AoS), un sommet = 16 octets : x y z invW.
    //  On veut deux registres VERTICAUX :  x = [x0 .. x7]   y = [y0 .. y7]
    //  ou xj = abscisse du coin k de la face j.
    //
    //  Recette : 4 registres de 2 sommets -> 2 unpack -> 2 shuffle.
    //  Les faces sont appariees (0,4) (1,5) (2,6) (3,7) : face j en moitie basse,
    //  face j+4 en moitie haute. Comme unpack/shuffle ne traversent pas la frontiere,
    //  chaque moitie fabrique 4 couloirs du resultat, independamment de l'autre.
    LV3_FORCEINLINE void LoadCornerXY8(const RasterVertex* rv, const uint32_t* idx, int k,
        __m256& x, __m256& y) noexcept
    {
        // idx[j*3 + k] = indice du coin k de la face j.
        //   _mm_load_ps            : lit 1 sommet ENTIER (16 o alignes) -> registre 128 bits
        //   _mm256_castps128_ps256 : le pose en moitie BASSE d'un 256 bits (0 instruction)
        //   _mm256_insertf128_ps   : pose un 2e sommet en moitie HAUTE (le dernier argument 1)
        const __m256 v04 = _mm256_insertf128_ps(
            _mm256_castps128_ps256(_mm_load_ps(&rv[idx[0 * 3 + k]].x)),    // face 0 -> bas
            _mm_load_ps(&rv[idx[4 * 3 + k]].x), 1); // face 4 -> haut
        const __m256 v15 = _mm256_insertf128_ps(
            _mm256_castps128_ps256(_mm_load_ps(&rv[idx[1 * 3 + k]].x)),    // face 1 -> bas
            _mm_load_ps(&rv[idx[5 * 3 + k]].x), 1); // face 5 -> haut
        const __m256 v26 = _mm256_insertf128_ps(
            _mm256_castps128_ps256(_mm_load_ps(&rv[idx[2 * 3 + k]].x)),    // face 2 -> bas
            _mm_load_ps(&rv[idx[6 * 3 + k]].x), 1); // face 6 -> haut
        const __m256 v37 = _mm256_insertf128_ps(
            _mm256_castps128_ps256(_mm_load_ps(&rv[idx[3 * 3 + k]].x)),    // face 3 -> bas
            _mm_load_ps(&rv[idx[7 * 3 + k]].x), 1); // face 7 -> haut
        // v04 = [x0 y0 z0 w0 | x4 y4 z4 w4]      (w = invW)
        // v15 = [x1 y1 z1 w1 | x5 y5 z5 w5]
        // v26 = [x2 y2 z2 w2 | x6 y6 z6 w6]
        // v37 = [x3 y3 z3 w3 | x7 y7 z7 w7]

        // unpacklo(A, B), dans CHAQUE moitie : [ A0 B0 A1 B1 ]
        // -> entrelace les x de deux faces, puis leurs y. z et invW sont abandonnes.
        const __m256 t0 = _mm256_unpacklo_ps(v04, v15);   // [x0 x1 y0 y1 | x4 x5 y4 y5]
        const __m256 t1 = _mm256_unpacklo_ps(v26, v37);   // [x2 x3 y2 y3 | x6 x7 y6 y7]

        // shuffle(A, B, imm), dans CHAQUE moitie : [ A[i0] A[i1] B[i2] B[i3] ]
        // _MM_SHUFFLE(d, c, b, a) se lit A L'ENVERS : i0 = a, i1 = b, i2 = c, i3 = d.
        //   (1,0,1,0) -> A[0] A[1] B[0] B[1] : les 2 x de t0, puis les 2 x de t1
        //   (3,2,3,2) -> A[2] A[3] B[2] B[3] : les 2 y de t0, puis les 2 y de t1
        x = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(1, 0, 1, 0));   // [x0 x1 x2 x3 | x4 x5 x6 x7]
        y = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(3, 2, 3, 2));   // [y0 y1 y2 y3 | y4 y5 y6 y7]

        // A partir d'ici : couloir j == face j. Tout le reste est du SIMD vertical.
    }

    // ════════════════════════════════════════════════════════════
    //  C3 — CHEMIN « BOITE D'UN PIXEL »
    //  Une face survivante dont la boite serree fait 1 x 1 pixel n'a qu'UN centre
    //  a tester. ClassifyFacesT8<true> fait ce test pour les 8 faces du lot, avec
    //  EXACTEMENT les operations du noyau (SetupTriangle + ForEachCoveredPixel) :
    //  echange p1 <-> p2, trois aretes, regle top-left, barycentriques.
    //  Le dessin (fragment direct, ou rien) est fait par Renderer::DrawBatch8.
    // ════════════════════════════════════════════════════════════

    // Chemin AVX2 : suit LV3_SIMD (config.h, section 11), comme B3, B4, B5 et C2.
    // Desactive sous ablation des pixels : les paliers S et E mesurent le NOYAU.
    inline constexpr bool kPix1Path = (LV3_SIMD != 0) && (LV3_ABLATION_PIXELS == 0);

    // Resultat C3 d'un lot. Bit k / couloir k <-> face k du lot.
    struct Pixel1Lanes8
    {
        uint32_t pix1 = 0;     // bit k : face a boite 1 x 1 et aire < 0 (ordonnee) -> chemin C3
        uint32_t covered = 0;     // bit k (sous-ensemble de pix1) : l'unique centre est couvert
        alignas(32) int32_t x[8];                 // le pixel de la boite (valide si bit pix1)
        alignas(32) int32_t y[8];
        alignas(32) float   b0[8], b1[8], b2[8];  // barycentriques, ordre d'ORIGINE (valides si bit covered)
    };

    // IsTopLeft voie par voie. Copie de Rasterizer.cpp :
    //   edge = b - a ; isTop = (edge.y == 0 && edge.x < 0) ; isLeft = (edge.y > 0)
    // Comparaisons _OQ : fausses pour NaN, comme en C++.
    [[nodiscard]] LV3_FORCEINLINE __m256 IsTopLeft8(__m256 ax, __m256 ay, __m256 bx, __m256 by) noexcept
    {
        const __m256 zero = _mm256_setzero_ps();
        const __m256 ex = _mm256_sub_ps(bx, ax), ey = _mm256_sub_ps(by, ay);
        const __m256 isTop = _mm256_and_ps(_mm256_cmp_ps(ey, zero, _CMP_EQ_OQ), _mm256_cmp_ps(ex, zero, _CMP_LT_OQ));
        const __m256 isLeft = _mm256_cmp_ps(ey, zero, _CMP_GT_OQ);
        return _mm256_or_ps(isTop, isLeft);
    }


    // ════════════════════════════════════════════════════════════
    //  ClassifyFaces8 — les deux tests pour 8 triangles
    //  idx : 24 indices (3 par face, face j en idx[j*3 .. j*3+2])
    //  rv  : sommets projetes, alignes sur 16 octets (_mm_load_ps)
    // ════════════════════════════════════════════════════════════
    //  C3 : kPix1 = true remplit aussi p1 (chemin « boite d'un pixel »).
    template<bool kPix1>
    [[nodiscard]] LV3_FORCEINLINE FaceMasks8 ClassifyFacesT8(const RasterVertex* rv, const uint32_t* idx, const Viewport& vp, Pixel1Lanes8* p1) noexcept
    {
        // ── ETAPE 1 : six registres verticaux, couloir j = face j ──────────
        __m256 ax, ay, bx, by, cx, cy;
        LoadCornerXY8(rv, idx, 0, ax, ay);    // coin a des 8 faces
        LoadCornerXY8(rv, idx, 1, bx, by);    // coin b
        LoadCornerXY8(rv, idx, 2, cx, cy);    // coin c

        // ── ETAPE 2 : AIRE SIGNEE — copie de EdgeFunction(a, b, c) ─────────
        // Forme antisymetrique : le point c sert de reference, on le soustrait d'abord.
        //   ux = ax - cx   uy = ay - cy   vx = bx - cx   vy = by - cy
        const __m256 ux = _mm256_sub_ps(ax, cx), uy = _mm256_sub_ps(ay, cy);
        const __m256 vx = _mm256_sub_ps(bx, cx), vy = _mm256_sub_ps(by, cy);

        // area = ux*vy - uy*vx : deux mul (2 arrondis) puis une sub (1 arrondi),
        // exactement comme le scalaire sous /fp:precise. PAS de _mm256_fmsub_ps.
        const __m256 area = _mm256_sub_ps(_mm256_mul_ps(ux, vy), _mm256_mul_ps(uy, vx));

        // ── ETAPE 3 : MASQUE FRONT ─────────────────────────────────────────
        // Scalaire : IsBackFacing(area) == (area >= 0)  ->  face AVANT == !(area >= 0)
        // _CMP_NGE_UQ = Not Greater-or-Equal, Unordered -> vrai, Quiet (pas d'exception).
        // Donc NaN = AVANT, comme le scalaire. (_CMP_LT_OQ rendrait NaN = ARRIERE : faux.)
        // Resultat par couloir : 0xFFFFFFFF (vrai) ou 0x00000000 (faux).
        const __m256 front = _mm256_cmp_ps(area, _mm256_setzero_ps(), _CMP_NGE_UQ);

        // ── ETAPE 4 : BOITE FLOTTANTE — copie de TightPixelBox ─────────────
        // MSVC : std::min(l, r) == (r < l) ? r : l
        //        _mm256_min_ps(A, B) == (A < B) ? A : B, et rend B si NaN ou egalite.
        // => std::min(l, r) == _mm256_min_ps(r, l)   : TOUJOURS (droite, gauche).
        //   std::min(b.x, c.x)          -> _mm256_min_ps(cx, bx)
        //   std::min(a.x, <ci-dessus>)  -> _mm256_min_ps(<ci-dessus>, ax)
        const __m256 minX = _mm256_min_ps(_mm256_min_ps(cx, bx), ax);
        const __m256 minY = _mm256_min_ps(_mm256_min_ps(cy, by), ay);
        // MSVC : std::max(l, r) == (l < r) ? r : l == _mm256_max_ps(r, l) : meme regle.
        const __m256 maxX = _mm256_max_ps(_mm256_max_ps(cx, bx), ax);
        const __m256 maxY = _mm256_max_ps(_mm256_max_ps(cy, by), ay);

        // ── ETAPE 5 : CENTRES DE PIXELS COUVERTS, en entiers ───────────────
        // Centre du pixel x = x + 0.5.
        //   1er centre  >= minX : ceil (minX - 0.5)  -> x0 (inclusif)
        //   dernier     <= maxX : floor(maxX - 0.5)  -> x1 = floor + 1 (EXCLUSIF)
        const __m256  half = _mm256_set1_ps(0.5f);       // 0.5 dans les 8 couloirs
        const __m256i one = _mm256_set1_epi32(1);       // 1 dans les 8 couloirs (int32)
        constexpr int kCeil = _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC;   // == std::ceil
        constexpr int kFloor = _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC;   // == std::floor

        // Chaque ligne se lit de l'interieur vers l'exterieur :
        //   sub (- 0.5)  ->  round (ceil ou floor)  ->  cvtt (troncature en int32)
        // cvttps_epi32 : hors plage ou NaN -> INT_MIN (0x80000000), comme le cvttss2si
        // du scalaire : meme les cas absurdes donnent le meme entier.
        __m256i x0 = _mm256_cvttps_epi32(_mm256_round_ps(_mm256_sub_ps(minX, half), kCeil));
        __m256i y0 = _mm256_cvttps_epi32(_mm256_round_ps(_mm256_sub_ps(minY, half), kCeil));
        __m256i x1 = _mm256_add_epi32(                                              // ... + 1
            _mm256_cvttps_epi32(_mm256_round_ps(_mm256_sub_ps(maxX, half), kFloor)), one);
        __m256i y1 = _mm256_add_epi32(
            _mm256_cvttps_epi32(_mm256_round_ps(_mm256_sub_ps(maxY, half), kFloor)), one);

        // ── ETAPE 6 : CLAMP — copie de Viewport::ClampBox, en entiers (exact) ──
        //   if (x0 < vp.x)         x0 = vp.x;          == max(x0, vp.x)
        //   if (x1 > vp.x + width) x1 = vp.x + width;  == min(x1, vp.x + width)
        x0 = _mm256_max_epi32(x0, _mm256_set1_epi32(vp.x));
        y0 = _mm256_max_epi32(y0, _mm256_set1_epi32(vp.y));
        x1 = _mm256_min_epi32(x1, _mm256_set1_epi32(vp.x + vp.width));
        y1 = _mm256_min_epi32(y1, _mm256_set1_epi32(vp.y + vp.height));

        // ── ETAPE 7 : MASQUE NON VIDE ──────────────────────────────────────
        // PixelBox::Empty() == (x0 >= x1 || y0 >= y1)
        // non vide          == (x1 >  x0 && y1 >  y0)  : 2 comparaisons + un ET bit a bit
        const __m256i nonEmpty = _mm256_and_si256(_mm256_cmpgt_epi32(x1, x0),
            _mm256_cmpgt_epi32(y1, y0));

        // ── ETAPE 8 : 8 COULOIRS -> 8 BITS ─────────────────────────────────
        // movemask_ps : bit j = bit de signe du couloir j (0xFFFFFFFF -> 1, 0 -> 0).
        // castsi256_ps : simple reinterpretation int -> float, AUCUNE instruction emise.
    //    const uint32_t f = uint32_t(_mm256_movemask_ps(front));
    //    const uint32_t s = f & uint32_t(_mm256_movemask_ps(_mm256_castsi256_ps(nonEmpty)));
    //    return { f, s };
    //}
        const uint32_t f = uint32_t(_mm256_movemask_ps(front));
        const uint32_t s = f & uint32_t(_mm256_movemask_ps(_mm256_castsi256_ps(nonEmpty)));

        // ── ETAPE 9 (C3) : boite d'UN pixel -> test du centre unique ───────
        if constexpr (kPix1)
        {
            // 9a. Qui prend le chemin C3 ? survivante, boite 1 x 1, ET aire < 0 ordonnee.
            //     Aire < 0 : SetupTriangle echange alors p1 et p2 a coup sur (face avant).
            //     NaN : _CMP_LT_OQ faux -> la face garde le chemin du noyau, qui sait la traiter.
            const __m256i one1 = _mm256_and_si256(
                _mm256_cmpeq_epi32(x1, _mm256_add_epi32(x0, one)),          // x1 == x0 + 1
                _mm256_cmpeq_epi32(y1, _mm256_add_epi32(y0, one)));         // y1 == y0 + 1
            const __m256 neg = _mm256_cmp_ps(area, _mm256_setzero_ps(), _CMP_LT_OQ);
            const uint32_t pix = s & uint32_t(_mm256_movemask_ps(
                _mm256_and_ps(_mm256_castsi256_ps(one1), neg)));
            p1->pix1 = pix;
            p1->covered = 0;
            if (pix != 0)                               // V2 : la plupart des lots n'en ont aucune
            {
                _mm256_store_si256(reinterpret_cast<__m256i*>(p1->x), x0);
                _mm256_store_si256(reinterpret_cast<__m256i*>(p1->y), y0);

                // 9b. Centre du pixel : float(x0) + 0.5f, comme ForEachCoveredPixel.
                const __m256 px = _mm256_add_ps(_mm256_cvtepi32_ps(x0), half);
                const __m256 py = _mm256_add_ps(_mm256_cvtepi32_ps(y0), half);

                // 9c. APRES ECHANGE : p0 = a, p1 = c, p2 = b. Aretes dans l'ordre du noyau :
                //     w0 = E(p1, p2) = E(c, b)   w1 = E(p2, p0) = E(b, a)   w2 = E(p0, p1) = E(a, c)
                const __m256 w0 = EdgeFunction8(cx, cy, bx, by, px, py);
                const __m256 w1 = EdgeFunction8(bx, by, ax, ay, px, py);
                const __m256 w2 = EdgeFunction8(ax, ay, cx, cy, px, py);

                // 9d. Regle top-left des memes aretes : tl0 = TL(c, b), tl1 = TL(b, a), tl2 = TL(a, c).
                const __m256 cov = _mm256_and_ps(
                    _mm256_and_ps(InsideEdge8(w0, IsTopLeft8(cx, cy, bx, by)),
                        InsideEdge8(w1, IsTopLeft8(bx, by, ax, ay))),
                    InsideEdge8(w2, IsTopLeft8(ax, ay, cx, cy)));
                p1->covered = pix & uint32_t(_mm256_movemask_ps(cov));

                // 9e. Barycentriques : invArea = 1 / (-area), puis echange defait :
                //     { w0 * inv, w2 * inv, w1 * inv }. -area par inversion du bit de signe : exact.
                //     Une vraie division (vdivps), jamais rcp.
                const __m256 negArea = _mm256_xor_ps(area, _mm256_set1_ps(-0.0f));
                const __m256 inv = _mm256_div_ps(_mm256_set1_ps(1.0f), negArea);
                _mm256_store_ps(p1->b0, _mm256_mul_ps(w0, inv));
                _mm256_store_ps(p1->b1, _mm256_mul_ps(w2, inv));
                _mm256_store_ps(p1->b2, _mm256_mul_ps(w1, inv));
            }
        }
        else
        {
            (void)p1;
        }
        return { f, s };
    }

    // B4 inchange pour les appelants : sans le chemin C3.
    [[nodiscard]] LV3_FORCEINLINE FaceMasks8 ClassifyFaces8(const RasterVertex* rv,
        const uint32_t* idx, const Viewport& vp) noexcept
    {
        return ClassifyFacesT8<false>(rv, idx, vp, nullptr);
    }

    // ── C3 : REFERENCE SCALAIRE d'une face (filets Debug et TNR). Elle n'est faite que du
    //    code du noyau : SetupTriangle puis ForEachCoveredPixel. Aucune formule recopiee.
    struct Pixel1Ref
    {
        bool               pix1 = false;    // la face prendrait le chemin C3
        bool               covered = false; // son centre est couvert
        int32_t            x = 0, y = 0;    // le pixel
        BarycentricWeights b{};             // barycentriques du noyau (valides si covered)
    };

    [[nodiscard]] inline Pixel1Ref Pixel1Reference(const RasterVertex& a, const RasterVertex& b,
        const RasterVertex& c, const Viewport& vp) noexcept
    {
        Pixel1Ref r;
        if (!(EdgeFunction(a, b, c) < 0.0f)) return r;           // meme critere que 9a
        TriangleSetup s;
        if (!SetupTriangle({ a.x, a.y }, { b.x, b.y }, { c.x, c.y }, vp, s)) return r;
        if (s.bb.x1 - s.bb.x0 != 1 || s.bb.y1 - s.bb.y0 != 1) return r;
        r.pix1 = true; r.x = s.bb.x0; r.y = s.bb.y0;
        struct Sink
        {
            Pixel1Ref& r;
            void BeginRow(int32_t) noexcept {}
            void Pixel(int32_t, const BarycentricWeights& w) noexcept { r.covered = true; r.b = w; }
        } sink{ r };
        (void)ForEachCoveredPixel(s, sink);
        return r;
    }

    // ════════════════════════════════════════════════════════════
    //  CHANTIER B5 — face arriere en espace OBJET
    //  IDENTITE (Cauchy-Binet) : aire_raster(a, b, c) = -K * (L . E) / (w0 * w1 * w2),  K > 0
    //    L = (n, d) : plan de la face (cuit) ; E : oeil homogene, tire de mvp
    //  Inside : w > 0  =>  face AVANT  <=>  s = L . E > 0.
    //  Ortho (E.w = 0) et miroir (signe de E) : aucun code special.
    // ════════════════════════════════════════════════════════════

    // E_k = (-1)^k * det(C prive de sa ligne k), C = colonnes 0, 1 et 3 de mvp (x, y, w du clip).
    // r_i = (m[i][0], m[i][1], m[i][3]), det(a, b, c) = a . (b x c) :
    //   E = ( r1.(r2 x r3), -r0.(r2 x r3), r0.(r1 x r3), -r0.(r1 x r2) )
    // P * mvp a x = y = w = 0 pour P = E : c'est le CENTRE de projection.
    //   perspective : E = lambda * (oeil_objet, 1)    ortho : E = lambda * (direction vers la camera, 0)
    [[nodiscard]] LV3_FORCEINLINE Vec4f HomogeneousEye(const Matrix44f& m) noexcept
    {
        const Vec3f r0{ m[0][0], m[0][1], m[0][3] };
        const Vec3f r1{ m[1][0], m[1][1], m[1][3] };
        const Vec3f r2{ m[2][0], m[2][1], m[2][3] };
        const Vec3f r3{ m[3][0], m[3][1], m[3][3] };
        const Vec3f c23 = r2.crossProduct(r3);
        return { r1.dotProduct(c23),
                 -r0.dotProduct(c23),
                  r0.dotProduct(r1.crossProduct(r3)),
                 -r0.dotProduct(r1.crossProduct(r2)) };
    }

    // ***********************************************************************************
    // Rejet en objet seulement si s < -tau, tau = kObjBackMargin * (|E.xyz| + |E.w| * maxAbsD).
    // |n| = 1 et |d| <= maxAbsD => tau est une fraction de la plus grande valeur possible de s :
    // sin(angle rasant) > 1/64 (0,9 degre). Plus rasant : l'ecran decide, comme avant.
    inline constexpr float kObjBackMargin = 1.0f / 64.0f;

    // VERITE des filets B5 (Debug et test hors moteur) : aire raster des 3 sommets float,
    // en DOUBLE de bout en bout (MulRow, /w, ToRaster, EdgeFunction). Jamais appelee en Release.
    [[nodiscard]] inline double ExactRasterArea(const Matrix44f& m, const Viewport& vp,
        const Vec3f& a, const Vec3f& b, const Vec3f& c) noexcept
    {
        const Vec3f* p[3] = { &a, &b, &c };
        double X[3], Y[3];
        for (int i = 0; i < 3; ++i)
        {
            double r[4];
            for (int j = 0; j < 4; ++j)
                r[j] = double(p[i]->x) * m[0][j] + double(p[i]->y) * m[1][j]
                + double(p[i]->z) * m[2][j] + double(m[3][j]);
            X[i] = vp.x + (r[0] / r[3] * 0.5 + 0.5) * vp.width;
            Y[i] = vp.y + (0.5 - r[1] / r[3] * 0.5) * vp.height;
        }
        return (X[0] - X[2]) * (Y[1] - Y[2]) - (Y[0] - Y[2]) * (X[1] - X[2]);   // EdgeFunction(a, b, c)
    }

    // ***********************************************************************************
    [[nodiscard]] LV3_FORCEINLINE float ObjBackThreshold(const Vec4f& e, float maxAbsD) noexcept
    {
        const float exyz = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
        return kObjBackMargin * (exyz + std::fabs(e.w) * maxAbsD);
    }
    // ***********************************************************************************
    // 8 faces consecutives : bit k = 1 <=> face k SUREMENT arriere (s < -tau).
    // nx..d : 1re face du lot, alignes 32 o. ex..ew, negTau : diffuses HORS de la boucle par l'appelant.
    // Meme ordre que le scalaire du filet : ((nx*ex + ny*ey) + nz*ez) + d*ew. Pas de FMA.
    [[nodiscard]] LV3_FORCEINLINE uint32_t ObjBackMask8(
        const float* nx, const float* ny, const float* nz, const float* d,
        __m256 ex, __m256 ey, __m256 ez, __m256 ew, __m256 negTau) noexcept
    {
        // 4 lectures CONTIGUES : aucun gather (la SoA a ete faite au chargement)
        const __m256 s = _mm256_add_ps(_mm256_add_ps(_mm256_add_ps(
            _mm256_mul_ps(_mm256_load_ps(nx), ex),          // nx*ex
            _mm256_mul_ps(_mm256_load_ps(ny), ey)),         // + ny*ey
            _mm256_mul_ps(_mm256_load_ps(nz), ez)),         // + nz*ez
            _mm256_mul_ps(_mm256_load_ps(d), ew));         // + d*ew   -> s = [s0 .. s7]

        // _CMP_LT_OQ : NaN -> FAUX = "pas sur", l'ecran decide (un doute ne rejette JAMAIS).
        return uint32_t(_mm256_movemask_ps(_mm256_cmp_ps(s, negTau, _CMP_LT_OQ)));
    }
}