#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "../Maths/Vectorlib.h"
#include "../Maths/MatrixLib.h"
#include "FrameBuffer.h"
#include "Viewport.h"

namespace LV3
{
    struct BarycentricWeights { float w0, w1, w2; };   // normalisés, somme == 1

    using FragmentCallback = void(*)(int32_t x, int32_t y,
                                    const BarycentricWeights& bary,
                                    void* userData);

    // ════════════════════════════════════════════════════════════
    //  DÉFINIES ICI : appelées par pixel ou par sommet.
    //  Une fonction inline doit avoir son corps dans le header.
    // ════════════════════════════════════════════════════════════

   // ── Fonction d'arête de Pineda ────────────────────────────
    //  Elle ne travaille QUE sur x et y : le z n'intervient jamais.
    //  Trois services pour le prix d'un :
    //    * sur trois sommets  -> deux fois l'aire signée (backface, normalisation)
    //    * signe sur un pixel -> le pixel est-il du bon côté de l'arête
    //    * valeur / aire      -> coordonnée barycentrique

    // Le NOYAU. Tout le reste y délègue.
    // Forme ANTISYMÉTRIQUE : le point testé p sert de référence, donc
    //     E(b,a,p) == -E(a,b,p)  EXACTEMENT en IEEE 754
    // (les deux produits sont identiques, seul l'ordre de la soustraction change).
    //
    // Bénéfice secondaire : en soustrayant p d'abord, les magnitudes restent
    // locales au pixel au lieu d'être absolues. Supprime l'annulation
    // catastrophique sur les sommets très éloignés du viewport, ce qui arrive
    // systématiquement sur les intersections du plan near.
    //
    // NÉCESSAIRE, mesuré : sans cette forme, 124 trous sur la couture du quad
    // de Test_ClipCoverage. Avec, il en reste 4 — que ClipLess élimine.
    [[nodiscard]] LV3_FORCEINLINE constexpr float EdgeFunction(
        float ax, float ay, float bx, float by, float px, float py) noexcept
    {
        const float ux = ax - px, uy = ay - py;
        const float vx = bx - px, vy = by - py;
        return ux * vy - uy * vx;
    }

    // Arête (a,b) contre un troisième sommet. Idem, 2D ou 3D.
    // backface culling (EmitClipTriangle, EmitForTest) + area dans RasterizeTriangle
    template<typename V>
    [[nodiscard]] LV3_FORCEINLINE float EdgeFunction(const V& a, const V& b, const V& c) noexcept
    {
       return EdgeFunction(a.x, a.y, b.x, b.y, c.x, c.y);
    }

    // boucle pixel du rasterizer (w0, w1, w2 par pixel) 
    template<typename V>
    [[nodiscard]] LV3_FORCEINLINE float EdgeFunction(const V& a, const V& b, float px, float py) noexcept
    {
        return EdgeFunction(a.x, a.y, b.x, b.y, px, py);   // ← délégation RÉTABLIE
    }
    // Vecteur-ligne SANS division par w : le w sert au rejet du near.
    [[nodiscard]] LV3_FORCEINLINE Vec4f MulRow(const Matrix44f& m, const Vec3f& p) noexcept
    {
        return { p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
                 p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
                 p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2],
                 p.x * m[0][3] + p.y * m[1][3] + p.z * m[2][3] + m[3][3] };
    }

    // CONVENTION DE FACE — mesurée, pas devinée. Verrouillée par TNR §Winding.
    //   Front-face LV3 = CCW en espace VUE (main droite, regard -Z).
    //   Viewport::ToRaster inverse Y  =>  CW en espace RASTER
    //   =>  l'aire signée d'une FACE AVANT est NEGATIVE.
    //
    // Le >= rejette aussi l'aire nulle : triangle degenere, rien a dessiner.
    [[nodiscard]] LV3_FORCEINLINE bool IsBackFacing(float signedArea) noexcept
    {
        return signedArea >= 0.0f;
    }

    // ════════════════════════════════════════════════════════════
    //  DÉCLARÉES ICI, définies dans Rasterizer.cpp : elles bouclent.
    // ════════════════════════════════════════════════════════════

    [[nodiscard]] bool IsTopLeft(const Vec2f& a, const Vec2f& b) noexcept;

    [[nodiscard]] Color FaceColor(int i) noexcept;

    // Rasterisation en espace écran, avec règle top-left.
    // /!\ prend un VIEWPORT, plus une largeur/hauteur : c'est lui le scissor.
    //     Sans ça, une vue déborde sur l'autre en écran splitté.
    void RasterizeTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        const Viewport& vp,
        FragmentCallback onFragment, void* userData);

    void DrawLineClipped(FrameBuffer& fb, const Viewport& vp,
        const Vec3f& a, const Vec3f& b, Color c) noexcept;
}