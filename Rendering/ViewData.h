#pragma once
#include "../Maths/MatrixLib.h"
#include "../Maths/Vectorlib.h"
#include "../Maths/geometry/Frustum.h"
#include "Viewport.h"
#include "../Rendering/RenderTypes.h"

namespace LV3
{
    struct ScreenSizeParams
    {
        float kPx;    // P[1][1] * H/2 : pixels par unite monde a w = 1
        float wGrad;  // |colonne 3 de VP| : 1 en perspective, 0 en ortho
        float wNear;  // en perspective, une sphere qui atteint le near => taille infinie
    };

    struct ViewData
    {
        // --- Matrices ---
        Matrix44f viewMatrix;              // Monde -> Vue
        Matrix44f projectionMatrix;        // Vue   -> Clip
        Matrix44f viewProjectionMatrix;    // Monde -> Clip

        // --- Frustum et Culling ---
        Frustum   frustum;                 // 6 plans, espace MONDE
        float     nearPlane = 0.1f;
        float     farPlane = 1000.0f;       // sans objet si hasFarPlane == false

        // hasFarPlane == false : le volume de vue est infini (PerspectiveInfinite).
        // Tout calcul necessitant un far borne doit alors substituer une valeur d'affichage explicite -- jamais lire farPlane.
        bool      hasFarPlane = true;

        bool      reverseZ = true;

        // --- Camera dans le monde ---
        Vec3f     position;
        Vec3f     forward;

        // --- Camera 
        Entity m_sourceCamera = NULL_ENTITY;   // entite d'ou provient cette vue

        // --- Mode de rendu
        ERenderMode mode = ERenderMode::Solid;
        
        // --- Debug : affichage ---
        float depthDisplayRange = 80.0f;   // plage de lisibilité pour ERenderMode::Depth — PAS une donnée géométrique (R20)

        // --- LOD (A13 bis) : resolus UNE fois par vue dans BuildViewData ---
        float            lodTolerancePx = 0.5f;   // tau effectif (camera, sinon EngineConfig)
        ScreenSizeParams lodParams{};             // comme MakeScreenSizeParams, mais kPx DEJA divise par tau :
                                                  // ProjectedRadiusPx(lodParams, ...) rend directement une grandeur / tau

        // --- Destination ---
        Viewport  viewport;

        // --- Inverse : PARESSEUX (Gauss-Jordan, ne sert qu'au picking) ---
        [[nodiscard]] const Matrix44f& InvViewProjection() const noexcept
        {
            if (!invValid) { invVP = viewProjectionMatrix.inverse(); invValid = true; }
            return invVP;
        }
        void InvalidateCache() noexcept { invValid = false; }

        // --- Monde -> CLIP (4D, AVANT la division) ---
        [[nodiscard]] LV3_FORCEINLINE Vec4f WorldToClip(const Vec3f& p) const noexcept
        {
            const Matrix44f& M = viewProjectionMatrix;
            return { p.x * M[0][0] + p.y * M[1][0] + p.z * M[2][0] + M[3][0],
                     p.x * M[0][1] + p.y * M[1][1] + p.z * M[2][1] + M[3][1],
                     p.x * M[0][2] + p.y * M[1][2] + p.z * M[2][2] + M[3][2],
                     p.x * M[0][3] + p.y * M[1][3] + p.z * M[2][3] + M[3][3] };
        }

        // --- Monde -> NDC. false si DERRIERE l'oeil (w <= 0) ---
        [[nodiscard]] bool WorldToNDC(const Vec3f& p, Vec3f& outNdc) const noexcept
        {
            const Vec4f c = WorldToClip(p);
            if (c.w <= 0.0f) return false;
            const float inv = 1.0f / c.w;
            outNdc = { c.x * inv, c.y * inv, c.z * inv };
            return true;
        }

        // --- Monde -> pixels ---
        [[nodiscard]] bool WorldToRaster(const Vec3f& p, Vec3f& outRaster) const noexcept
        {
            Vec3f ndc;
            if (!WorldToNDC(p, ndc))          return false;
            if (ndc.z < 0.0f || ndc.z > 1.0f) return false;
            outRaster = viewport.ToRaster(ndc);
            return true;
        }

        // --- Pixel -> rayon monde (picking) ---
        void RasterToRay(float px, float py, Vec3f& outOrigin, Vec3f& outDir) const noexcept
        {
            float xn, yn;
            viewport.ToNDC(px, py, xn, yn);
            const Matrix44f& inv = InvViewProjection();

            Vec3f pNear, pMid;
            inv.multVecMatrix(Vec3f(xn, yn, reverseZ ? 1.0f : 0.0f), pNear);
            inv.multVecMatrix(Vec3f(xn, yn, 0.5f), pMid);

            outOrigin = pNear;
            outDir = (pMid - pNear).Normalized();
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return viewport.IsValid() && nearPlane > 0.0f && farPlane > nearPlane;
        }

    private:
        mutable Matrix44f invVP;
        mutable bool      invValid = false;
    };

    // ════════════════════════════════════════════════════════════
//  TAILLE APPARENTE : SOURCE UNIQUE. Statistiques de la phase G
//  aujourd'hui, selection du LOD demain : meme fonction (cf. TightPixelBox).
//
//  w_clip est AFFINE en position monde : w(p) = p.c + M[3][3],
//  c = colonne 3 de VP.  Perspective : |c| = 1 (w = profondeur de vue).
//  Ortho : c = 0 (w = 1). UNE formule, aucun drapeau de mode.
// ════════════════════════════════════════════════════════════
    //struct ScreenSizeParams
    //{
    //    float kPx;    // P[1][1] * H/2 : pixels par unite monde a w = 1
    //    float wGrad;  // |colonne 3 de VP| : 1 en perspective, 0 en ortho
    //    float wNear;  // en perspective, une sphere qui atteint le near => taille infinie
    //};

    // Une fois PAR VUE, jamais par instance.
    [[nodiscard]] inline ScreenSizeParams MakeScreenSizeParams(const ViewData& v) noexcept
    {
        const Matrix44f& M = v.viewProjectionMatrix;
        return { v.projectionMatrix[1][1] * 0.5f * float(v.viewport.height),
                 std::sqrt(M[0][3] * M[0][3] + M[1][3] * M[1][3] + M[2][3] * M[2][3]),
                 v.nearPlane };
    }

    // Rayon apparent (pixels) d'une sphere : rayon monde rWorld, w_clip de son centre.
    // Profondeur prise au point le PLUS PROCHE : jamais sous-estimee en profondeur.
    // Limite connue : l'etirement hors axe (ellipse de perspective) n'est pas modelise.
    [[nodiscard]] LV3_FORCEINLINE float ProjectedRadiusPx(const ScreenSizeParams& p,
        float wCenter, float rWorld) noexcept
    {
        const float wMin = wCenter - rWorld * p.wGrad;
        if (p.wGrad > 0.0f && wMin <= p.wNear)
            return std::numeric_limits<float>::infinity();   // touche le near : detail maximal
        return rWorld * p.kPx / wMin;
    }

    // Sphere circonscrite a l'AABB LOCALE, en unites monde : |demi-diagonale| x plus
    // grande echelle d'axe (lignes 0..2 : convention vecteur-ligne). Surestime au plus
    // d'un facteur sqrt(3) : c'est le sens SUR pour un LOD.
    [[nodiscard]] inline float BoundingRadiusWorld(const AABB3d& local, const Matrix44f& world) noexcept
    {
        float s2 = 0.0f;
        for (int i = 0; i < 3; ++i)
            s2 = std::max(s2, world[i][0] * world[i][0] + world[i][1] * world[i][1] + world[i][2] * world[i][2]);
        return local.Extent().length() * std::sqrt(s2);
    }
}