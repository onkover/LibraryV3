#pragma once
#include "../Maths/MatrixLib.h"
#include "../Maths/Vectorlib.h"
#include "../Maths/geometry/Frustum.h"
#include "Viewport.h"
#include "../Rendering/RenderTypes.h"

namespace LV3
{
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
}