#pragma once
// ============================================================
//  Rendering/ViewData.h — Point de vue résolu pour une frame
//
//  Contient UNIQUEMENT des données dérivées. Rien ici n'est
//  une donnée d'auteur : tout est recalculé chaque frame par
//  BuildViewData() à partir du Transform, de la CameraComponent
//  et du Viewport.
//
//  Un ViewData par POINT DE VUE : caméra joueur, minimap,
//  face de cubemap, cascade d'ombre...
// ============================================================
#include "../Maths/MatrixLib.h"
#include "../Maths/Vectorlib.h"
#include "../Maths/Geometry/Frustum.h"
#include "Viewport.h"

namespace LV3
{
    struct ViewData
    {
        // --- Matrices ------------------------------------------------
        Matrix44f viewMatrix;              // Monde -> Vue   (inverse rigide de la matrice monde)
        Matrix44f projectionMatrix;        // Vue   -> Clip
        Matrix44f viewProjectionMatrix;    // Monde -> Clip  = view * projection (vecteur-ligne)

        // --- Culling -------------------------------------------------
        Frustum   frustum;           // 6 plans, espace MONDE

        // --- Caméra dans le monde ------------------------------------
        Vec3f     position;          // l'œil (éclairage spéculaire, LOD, tri par distance)
        Vec3f     forward;           // direction de visée (main droite : -Z de la matrice monde)

        // --- Profondeur ----------------------------------------------
        float     nearPlane = 0.1f;
        float     farPlane = 1000.0f;
        bool      reverseZ = true;  // near -> 1, far -> 0

        // --- Destination ---------------------------------------------
        Viewport  viewport;

        // =============================================================
        //  Inverse de viewProjection : PARESSEUX.
        //  C'est le seul calcul coûteux (Gauss-Jordan 4x4) et il ne
        //  sert qu'au picking et aux effets écran. Inutile de le payer
        //  sur chaque frame d'un jeu qui n'en fait pas.
        // =============================================================
        [[nodiscard]] const Matrix44f& InvViewProjection() const noexcept
        {
            if (!m_invValid) { m_invVP = viewProjectionMatrix.inverse(); m_invValid = true; }
            return m_invVP;
        }
        void InvalidateCache() noexcept { m_invValid = false; }

        // =============================================================
        //  Transformations utilitaires
        // =============================================================

        // Monde -> CLIP (4D, AVANT la division). w porte la distance à l'œil.
        [[nodiscard]] LV3_FORCEINLINE Vec4f WorldToClip(const Vec3f& p) const noexcept
        {
            const Matrix44f& M = viewProjectionMatrix;
            return {
                p.x * M[0][0] + p.y * M[1][0] + p.z * M[2][0] + M[3][0],
                p.x * M[0][1] + p.y * M[1][1] + p.z * M[2][1] + M[3][1],
                p.x * M[0][2] + p.y * M[1][2] + p.z * M[2][2] + M[3][2],
                p.x * M[0][3] + p.y * M[1][3] + p.z * M[2][3] + M[3][3]
            };
        }

        // Monde -> NDC. false si le point est DERRIÈRE l'œil (w <= 0).
        // Toujours tester le retour : sans ça, les points arrière
        // réapparaissent en miroir à l'écran.
        [[nodiscard]] bool WorldToNDC(const Vec3f& p, Vec3f& outNdc) const noexcept
        {
            const Vec4f c = WorldToClip(p);
            if (c.w <= 0.0f) return false;  // Un point derrière l'œil a w < 0 ; diviser par ce w négatif inverse le signe et le point réapparaît en miroir de l'autre côté de l'écran. C'est le bug du « triangle qui explose quand on recule dedans »
            const float inv = 1.0f / c.w;
            outNdc = { c.x * inv, c.y * inv, c.z * inv };
            return true;
        }

        // Monde -> pixels. false si derrière l'œil ou hors de [near, far].
        [[nodiscard]] bool WorldToRaster(const Vec3f& p, Vec3f& outRaster) const noexcept
        {
            Vec3f ndc;
            if (!WorldToNDC(p, ndc))             return false;
            if (ndc.z < 0.0f || ndc.z > 1.0f)    return false;
            outRaster = viewport.ToRaster(ndc);
            return true;
        }

        // Pixel -> rayon monde (picking, raycast à la souris).
        void RasterToRay(float px, float py, Vec3f& outOrigin, Vec3f& outDir) const noexcept
        {
            float xn, yn;
            viewport.ToNDC(px, py, xn, yn);
            const Matrix44f& inv = InvViewProjection();

            // Plan near : z_ndc = 1 en reverse-Z, 0 sinon.
            // Second point à 0.5 : toujours fini, même avec un far infini.
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
        // mutable n'est pas de la triche. ViewData circule en const ViewData& dans tout le rendu. Le cache paresseux doit pouvoir s'écrire malgré ce const : c'est exactement le cas d'usage canonique de mutable — un état interne qui ne change pas la valeur observable de l'objet.
        mutable Matrix44f m_invVP;
        mutable bool      m_invValid = false;
    };
}