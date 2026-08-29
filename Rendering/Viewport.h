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

namespace LV3
{
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