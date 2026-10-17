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
    //  ClassifyFaces8 — les deux tests pour 8 triangles
    //  idx : 24 indices (3 par face, face j en idx[j*3 .. j*3+2])
    //  rv  : sommets projetes, alignes sur 16 octets (_mm_load_ps)
    // ════════════════════════════════════════════════════════════
    [[nodiscard]] LV3_FORCEINLINE FaceMasks8 ClassifyFaces8(const RasterVertex* rv,
        const uint32_t* idx, const Viewport& vp) noexcept
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
        const uint32_t f = uint32_t(_mm256_movemask_ps(front));
        const uint32_t s = f & uint32_t(_mm256_movemask_ps(_mm256_castsi256_ps(nonEmpty)));
        return { f, s };
    }
}