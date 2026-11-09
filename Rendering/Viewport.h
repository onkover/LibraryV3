#pragma once
// ============================================================
//  Rendering/Viewport.h — Rectangle de destination à l'écran
//
//  Seul endroit du moteur qui connaît les PIXELS.
//  Seul endroit qui effectue le FLIP Y (NDC y vers le haut ->
//  raster y vers le bas). Si tu cherches un jour une image à
//  l'envers, c'est ici, et nulle part ailleurs.
//
//  Agrégat volontaire, mais construis-le par Viewport::FullScreen(w, h) :
//  les dimensions appartiennent à la cible de rendu, pas à ce fichier.
// ============================================================
#include "../Core/Compiler.h"
#include "../Maths/Vectorlib.h"
#include <immintrin.h>      // D-a1 (Giesen méthod) : _mm_round_ss (accrochage sous-pixel)
#include <cmath>            // D-a1d : std::fabs (domaine du nombre magique)


namespace LV3
{
    // ════════════════════════════════════════════════════════════
    //  D-a1 : GRILLE SOUS-PIXEL (virgule fixe, 8 bits de fraction)
    //  Un sommet raster vit sur la grille des 1/256 de pixel. Ses coordonnées
    //  restent des float, mais des float qui SONT des entiers / 256 : la
    //  conversion en virgule fixe (x * 256) est alors exacte, partout.
    // ════════════════════════════════════════════════════════════
    inline constexpr int32_t kSubpixelBits = 8;                         // 1/256 de pixel (Larrabee)
    inline constexpr float   kSubpixelScale = 256.0f;                   // 2^8
    inline constexpr float   kSubpixelInv = 1.0f / 256.0f;              // 2^-8 : exact
    inline constexpr float   kSubpixelArea = 1.0f / 65536.0f;           // 2^-16 : unité d'une aire en virgule fixe

    // GARDE : au-delà de 2^20 pixels (1 048 576), un sommet ne passe pas en virgule fixe.
    // En deçà : |X| < 2^28, différences < 2^29, produits < 2^58 : la préparation int64 ne
    // peut pas déborder (marge x 32). Au-delà, le triangle garde le chemin flottant C2
    // (compteur TrisFixedGuard). Aucune scène de banc ne l'atteint : il faudrait un sommet
    // à 270 m de l'axe de visée, posé sur le plan near (0,1 m).
    inline constexpr float   kFixedGuardPx = 1048576.0f;    // limite de la conversion float -> int32_t * 256 (D-a1), soit  2^20 pixels

    // ════════════════════════════════════════════════════════════
    //  D-a1d : ACCROCHAGE PAR NOMBRE MAGIQUE
    //  Pour |v| < 2^14, v + 1,5 x 2^15 tombe dans [2^15, 2^16) : dans cet intervalle, deux float
    //  voisins sont espaces de 2^(15-23) = 2^-8 = 1/256 px. L'ADDITION arrondit donc v au multiple
    //  de 1/256 le plus proche (au pair : mode MXCSR par defaut) ; la SOUSTRACTION est exacte.
    //  2 additions au lieu de mul + round (2 uops) + mul.
    // ════════════════════════════════════════════════════════════
    inline constexpr float kSnapMagic = 49152.0f;           // 1,5 x 2^15
    inline constexpr float kSnapMagicRange = 16384.0f;      // 2^14 : domaine |v| < 2^14 (borne exclue)

    // /fp:fast (ou -ffast-math) aurait le droit de simplifier (v + M) - M en v : l'accrochage
    // disparaitrait sans bruit. LV3 compile en /fp:precise (LV3.Common.props) : on le verrouille.
    #if defined(_M_FP_FAST) || defined(__FAST_MATH__)
       #error "D-a1d : /fp:fast interdit, (v + kSnapMagic) - kSnapMagic serait simplifie en v"
    #endif

    // v -> multiple de 1/256 le plus proche (au pair en cas d'égalité), EN FLOAT.
    //   |v| < 2^14 : nombre magique, MEME formule que ProjectPositions8 (etape 5b) ;
    //                scalaire et AVX2 rendent le meme float au bit pres, +0 compris.
    //   sinon      : v * 256 (exact) -> round explicite -> * 1/256 (exact). Chemin clippe
    //                seulement (sommets hors ecran, jusqu'a la garde 2^20 px).
    // Ne rend JAMAIS -0 : un negatif minuscule (-1/512 <= v <= -0) donne +0 (ToFixed : 0 dans les deux cas).
    // Idempotent. NaN reste NaN (comparaison fausse -> branche round). Exige MXCSR au plus proche.
    // Au-delà de |v| = 2^15, tout float est déjà un multiple de 1/256 : Snap ne change rien.
    [[nodiscard]] LV3_FORCEINLINE float SnapToSubpixel(float v) noexcept
    {
        if (std::fabs(v) < kSnapMagicRange)
        {
            //   v            =  123.4567032   (float, ulp 2^-17)
            //   v + 49152    =  49275.45703125  <- la somme ne peut garder que des multiples de 1/256 : ARRONDI
            //   ... - 49152  =  123.45703125    <- EXACT : le resultat tient en 22 bits significatifs
            const __m128 m = _mm_set_ss(kSnapMagic);
            return _mm_cvtss_f32(_mm_sub_ss(_mm_add_ss(_mm_set_ss(v), m), m));   // arrondi, puis exact
        }
        const __m128 s = _mm_mul_ss(_mm_set_ss(v), _mm_set_ss(kSubpixelScale));
        const __m128 r = _mm_round_ss(s, s, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        return _mm_cvtss_f32(_mm_mul_ss(r, _mm_set_ss(kSubpixelInv)));
    }

    // ***********************************************************

    struct Viewport
    {
        int x = 0, y = 0;
        int width = 0, height = 0;

        // --- Métriques ------------------------------------------
        [[nodiscard]] LV3_FORCEINLINE float Aspect() const noexcept
        {
            return IsValid() ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        }

        [[nodiscard]] LV3_FORCEINLINE bool IsValid() const noexcept
        {
            return width > 0 && height > 0;
        }

        // Fabrique : la taille vient TOUJOURS de la cible de rendu,
        // jamais d'un defaut ecrit dans ce header.
        //[[nodiscard]] void Resize(int xx, int yy, int w, int h) noexcept
        //{
        //    x=xx, y=yy, width=w, height=h;
        //}
        [[nodiscard]] static constexpr Viewport FullScreen(int w, int h) noexcept
        {
            return { 0, 0, w, h };
        }

        // --- NDC -> RASTER --------------------------------------
        //  x_ndc = -1 -> bord gauche      x_ndc = +1 -> bord droit
        //  y_ndc = +1 -> bord HAUT        y_ndc = -1 -> bord BAS
        //  Le raster a son origine en HAUT à gauche : d'où le flip.
        LV3_FORCEINLINE void ToRaster(float xn, float yn,
            float& xr, float& yr) const noexcept
        {
            xr = static_cast<float>(x) + (xn * 0.5f + 0.5f) * static_cast<float>(width);
            yr = static_cast<float>(y) + (0.5f - yn * 0.5f) * static_cast<float>(height);
        }

        // Variante Vec3 : z (profondeur NDC ∈ [0,1]) est transmis tel quel.
        [[nodiscard]] LV3_FORCEINLINE Vec3f ToRaster(const Vec3f& ndc) const noexcept
        {
            float xr, yr;
            ToRaster(ndc.x, ndc.y, xr, yr);
            return { xr, yr, ndc.z };
        }

        // --- RASTER -> NDC  (picking, ScreenPointToRay) ----------
        LV3_FORCEINLINE void ToNDC(float xr, float yr,
            float& xn, float& yn) const noexcept
        {
            xn = ((xr - static_cast<float>(x)) / static_cast<float>(width)) * 2.0f - 1.0f;
            yn = 1.0f - ((yr - static_cast<float>(y)) / static_cast<float>(height)) * 2.0f;
        }

        // --- Scissor --------------------------------------------
        [[nodiscard]] LV3_FORCEINLINE bool Contains(int px, int py) const noexcept
        {
            return px >= x && px < x + width && py >= y && py < y + height;
        }

        // Bornes entières d'un rectangle, rognées au viewport.
        // Utilisé par le rasterizer à la place du clipping latéral.
        LV3_FORCEINLINE void ClampBox(int& x0, int& y0, int& x1, int& y1) const noexcept
        {
            if (x0 < x)              x0 = x;
            if (y0 < y)              y0 = y;
            if (x1 > x + width)      x1 = x + width;
            if (y1 > y + height)     y1 = y + height;
        }
    };
}