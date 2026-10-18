#include "pch.h"
#include "VertexStage.h"
#include "Rasterizer.h"      // MulRow
#include <immintrin.h>       // B3 : AVX2
#include <cstdint>           // uintptr_t

namespace LV3
{
    void TransformPositions(const Matrix44f& mvp, const Vec3f* src, size_t count, Vec4f* dst) noexcept
    {
        LV3_ASSERT(count <= ClipSpaceBuffer::Capacity());
        LV3_ASSERT(src != nullptr || count == 0);

        // Copie LOCALE, volontaire. Par reference, chaque ecriture float dans dst
        // pourrait (pour le compilateur) modifier un coefficient de mvp : il
        // rechargerait les 16 coefficients a CHAQUE sommet. Une locale dont
        // l'adresse ne s'echappe pas ne peut pas etre aliasee.
        const Matrix44f m = mvp;

        for (size_t i = 0; i < count; ++i)
            dst[i] = MulRow(m, src[i]);
    }

    //*******************************************************************************

    void ProjectPositions(const Matrix44f& mvp, const Viewport& vp, const Vec3f* src, size_t count, RasterVertex* dst) noexcept
    {
        LV3_ASSERT(count <= RasterSpaceBuffer::Capacity());
        LV3_ASSERT(src != nullptr || count == 0);

        // Copies LOCALES volontaire, meme raison que TransformPositions : les ecritures
        // dans dst ne peuvent pas aliaser une locale dont l'adresse ne s'echappe pas.
        const Matrix44f m = mvp;
        const Viewport  v = vp;

        for (size_t i = 0; i < count; ++i)
        {
            const Vec4f c = MulRow(m, src[i]);
            LV3_ASSERT(c.w > 0.0f);   // contrat Inside, PROUVE sur donnees reelles en RelWithAsserts
            dst[i] = ProjectClip(c, v);
        }
    }

    //*******************************************************************************
    // B3 — Convention de lecture (identique a FaceStage.h) : registres en ORDRE MEMOIRE,
    // couloir 0 a gauche, | = frontiere des 128 bits. Couloir j = sommet i + j.
    // REGLE DU BIT PRES : chaque intrinseque = UNE operation du scalaire, MEME ordre.
    // Pas de FMA, pas de rcp.
    void ProjectPositions8(const Matrix44f& mvp, const Viewport& vp,
        const float* px, const float* py, const float* pz, size_t paddedCount, RasterVertex* dst) noexcept
    {
        LV3_ASSERT(paddedCount % 8 == 0);
        LV3_ASSERT(paddedCount <= RasterSpaceBuffer::Capacity());
        LV3_ASSERT(((uintptr_t(px) | uintptr_t(py) | uintptr_t(pz) | uintptr_t(dst)) & 31u) == 0);

        // ── Constantes HORS de la boucle : chaque coefficient diffuse dans les 8 couloirs.
        // m[r][c] : r = ligne (x, y, z, translation), c = composante produite (x, y, z, w).
        // 22 constantes > 16 registres ymm : le compilateur en laisse en memoire et les lit
        // en operande (vmulps ymm, ymm, [mem]), ce qui ne coute rien de plus.
        const __m256 m00 = _mm256_set1_ps(mvp[0][0]), m01 = _mm256_set1_ps(mvp[0][1]),
            m02 = _mm256_set1_ps(mvp[0][2]), m03 = _mm256_set1_ps(mvp[0][3]);
        const __m256 m10 = _mm256_set1_ps(mvp[1][0]), m11 = _mm256_set1_ps(mvp[1][1]),
            m12 = _mm256_set1_ps(mvp[1][2]), m13 = _mm256_set1_ps(mvp[1][3]);
        const __m256 m20 = _mm256_set1_ps(mvp[2][0]), m21 = _mm256_set1_ps(mvp[2][1]),
            m22 = _mm256_set1_ps(mvp[2][2]), m23 = _mm256_set1_ps(mvp[2][3]);
        const __m256 m30 = _mm256_set1_ps(mvp[3][0]), m31 = _mm256_set1_ps(mvp[3][1]),
            m32 = _mm256_set1_ps(mvp[3][2]), m33 = _mm256_set1_ps(mvp[3][3]);

        const __m256 one = _mm256_set1_ps(1.0f);
        const __m256 half = _mm256_set1_ps(0.5f);
        // Viewport::ToRaster convertit les int en float : on fait la meme conversion, une fois.
        const __m256 vpx = _mm256_set1_ps(static_cast<float>(vp.x));
        const __m256 vpy = _mm256_set1_ps(static_cast<float>(vp.y));
        const __m256 vpw = _mm256_set1_ps(static_cast<float>(vp.width));
        const __m256 vph = _mm256_set1_ps(static_cast<float>(vp.height));

        for (size_t i = 0; i < paddedCount; i += 8)
        {
            // ── 1. CHARGER : 3 lectures, aucune transposition(la SoA l'a faite au chargement).
            // MSVC emet vmovups (verifie a l'asm) : l'alignement n'est controle que par
            // l'assertion d'entree, il sert la performance (pas de ligne coupee), pas la securite.            const __m256 x = _mm256_load_ps(px + i);     // [x0 x1 x2 x3 | x4 x5 x6 x7]
            const __m256 y = _mm256_load_ps(py + i);     // [y0 .. y7]
            const __m256 z = _mm256_load_ps(pz + i);     // [z0 .. z7]

            // ── 2. MulRow : composante c = ((x*m0c + y*m1c) + z*m2c) + m3c
            // Le scalaire "a + b + c + d" s'evalue de GAUCHE a DROITE : memes parentheses ici.
            // Fusionner une mul et une add (vfmadd) changerait l'arrondi : interdit.
            const __m256 cx = _mm256_add_ps(_mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(x, m00), _mm256_mul_ps(y, m10)), _mm256_mul_ps(z, m20)), m30);
            const __m256 cy = _mm256_add_ps(_mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(x, m01), _mm256_mul_ps(y, m11)), _mm256_mul_ps(z, m21)), m31);
            const __m256 cz = _mm256_add_ps(_mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(x, m02), _mm256_mul_ps(y, m12)), _mm256_mul_ps(z, m22)), m32);
            const __m256 cw = _mm256_add_ps(_mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(x, m03), _mm256_mul_ps(y, m13)), _mm256_mul_ps(z, m23)), m33);

            // Contrat Inside : w > 0 dans les 8 couloirs. cmp -> 0xFFFFFFFF par couloir vrai,
            // movemask -> 1 bit par couloir : 0xFF = les 8. NaN -> faux -> assertion (voulu).
            LV3_ASSERT(_mm256_movemask_ps(_mm256_cmp_ps(cw, _mm256_setzero_ps(), _CMP_GT_OQ)) == 0xFF);

            // ── 3. DIVISION : invW = 1 / w. vdivps est IEEE exact, comme le divss du scalaire.
            // Jamais _mm256_rcp_ps (12 bits justes) : l'image ne serait plus celle du scalaire.
            const __m256 invW = _mm256_div_ps(one, cw);

            // ── 4. NDC : ProjectClip MULTIPLIE par invW (il ne divise pas par w) : idem.
            const __m256 xn = _mm256_mul_ps(cx, invW);
            const __m256 yn = _mm256_mul_ps(cy, invW);
            const __m256 zn = _mm256_mul_ps(cz, invW);   // reverse-Z, transmis tel quel

            // ── 5. ToRaster, copie de Viewport::ToRaster (a+b = b+a et a*b = b*a sont exacts) :
            //   xr = vp.x + (xn*0.5 + 0.5) * width
            //   yr = vp.y + (0.5 - yn*0.5) * height       <- le flip Y
            const __m256 xr = _mm256_add_ps(vpx,
                _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(xn, half), half), vpw));
            const __m256 yr = _mm256_add_ps(vpy,
                _mm256_mul_ps(_mm256_sub_ps(half, _mm256_mul_ps(yn, half)), vph));

            // ── 6. TRANSPOSITION 4x8 -> 8 RasterVertex { x, y, z, invW } (AoS, 16 o)
            // L'etage faces lit les sommets PAR INDICE : il lui faut l'AoS (un _mm_load_ps par coin).
            // unpacklo(A, B), dans CHAQUE moitie : [A0 B0 A1 B1] ; unpackhi : [A2 B2 A3 B3].
            const __m256 t0 = _mm256_unpacklo_ps(xr, yr);     // [x0 y0 x1 y1 | x4 y4 x5 y5]
            const __m256 t1 = _mm256_unpackhi_ps(xr, yr);     // [x2 y2 x3 y3 | x6 y6 x7 y7]
            const __m256 t2 = _mm256_unpacklo_ps(zn, invW);   // [z0 w0 z1 w1 | z4 w4 z5 w5]
            const __m256 t3 = _mm256_unpackhi_ps(zn, invW);   // [z2 w2 z3 w3 | z6 w6 z7 w7]

            // shuffle(A, B, _MM_SHUFFLE(d,c,b,a)), dans CHAQUE moitie : [A[a] A[b] B[c] B[d]]
            //   (1,0,1,0) -> A0 A1 B0 B1 : x y du 1er sommet de la paire, puis son z w
            //   (3,2,3,2) -> A2 A3 B2 B3 : idem pour le 2e sommet
            const __m256 v04 = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(1, 0, 1, 0)); // [S0 | S4]
            const __m256 v15 = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(3, 2, 3, 2)); // [S1 | S5]
            const __m256 v26 = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(1, 0, 1, 0)); // [S2 | S6]
            const __m256 v37 = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(3, 2, 3, 2)); // [S3 | S7]
            // (Sk = sommet k complet : xr yr zn invW)

            // permute2f128 : SEULE instruction qui traverse la frontiere 128 bits.
            //   0x20 -> [bas de A | bas de B]     0x31 -> [haut de A | haut de B]
            // 4 stores alignes de 32 o = 128 o contigus = sommets i .. i+7 dans l'ordre.
            _mm256_store_ps(&dst[i + 0].x, _mm256_permute2f128_ps(v04, v15, 0x20)); // [S0 | S1]
            _mm256_store_ps(&dst[i + 2].x, _mm256_permute2f128_ps(v26, v37, 0x20)); // [S2 | S3]
            _mm256_store_ps(&dst[i + 4].x, _mm256_permute2f128_ps(v04, v15, 0x31)); // [S4 | S5]
            _mm256_store_ps(&dst[i + 6].x, _mm256_permute2f128_ps(v26, v37, 0x31)); // [S6 | S7]
        }
    }

}