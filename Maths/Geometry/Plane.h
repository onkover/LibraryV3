#pragma once
// ============================================================
//  Maths/Geometry/Plane.h — Plan orienté du moteur LV3
//  Équation : normal·p + d = 0
//  Par convention moteur, la normale d'un plan de frustum pointe
//  vers l'INTÉRIEUR du volume  ->  SignedDistance > 0 = dedans.
// ============================================================
#include <cstdint>
#include <cmath>
#include "../Vectorlib.h"
#include "../../Core/config.h"     // LV3::EPSILON_FLOAT

namespace LV3
{
    enum class EPlaneSide : uint8_t { Front, Back, OnPlane };

    struct Plane
    {
        Vec3f normal{ 0.0f, 1.0f, 0.0f };
        float d = 0.0f;

        constexpr Plane() noexcept = default;
        constexpr Plane(const Vec3f& n, float dd) noexcept : normal(n), d(dd) {}

        // --- Requêtes -------------------------------------------
        // > 0 : devant (côté de la normale)   < 0 : derrière
        LV3_FORCEINLINE float SignedDistance(const Vec3f& p) const noexcept
        {
            return normal.dotProduct(p) + d;
        }

        LV3_FORCEINLINE EPlaneSide Classify(const Vec3f& p) const noexcept
        {
            const float dist = SignedDistance(p);
            if (dist > EPSILON_FLOAT) return EPlaneSide::Front;
            if (dist < -EPSILON_FLOAT) return EPlaneSide::Back;
            return EPlaneSide::OnPlane;
        }

        // --- Normalisation --------------------------------------
        // SUR PLACE : c'est la version qu'utilise Frustum::Build().
        LV3_FORCEINLINE void Normalize() noexcept
        {
            const float len2 = normal.norm();          // longueur AU CARRÉ (pas de sqrt inutile)

            // Garde anti-division uniquement : seul le zéro véritable est rejeté.
            // Cas légitime : plan Far dégénéré quand far = infini.
            if (len2 > NEAR_ZERO_SQ)
            {
                const float inv = 1.0f / std::sqrt(len2);
                normal *= inv;
                d *= inv;
            }
        }
        // PAR COPIE : pour les expressions.
        [[nodiscard]] LV3_FORCEINLINE Plane Normalized() const noexcept
        {
            Plane p = *this;
            p.Normalize();
            return p;
        }

        LV3_FORCEINLINE void Flip() noexcept { normal = -normal; d = -d; }

        // --- Fabriques (nommées : aucune ambiguïté possible) -----
        [[nodiscard]] static Plane FromPointAndNormal(const Vec3f& pt, const Vec3f& n) noexcept
        {
            const Vec3f nn = n.Normalized();          // UN seul sqrt
            return Plane(nn, -nn.dotProduct(pt));
        }

        // Triangle (a,b,c). La normale suit le sens de parcours
        // via le produit vectoriel main droite de Vectorlib.
        [[nodiscard]] static Plane FromPoints(const Vec3f& a, const Vec3f& b, const Vec3f& c) noexcept
        {
            const Vec3f n = (b - a).crossProduct(c - a);   // prvalue conservée par valeur
            return FromPointAndNormal(a, n);
        }

        // --- Intersection segment / plan (clipping, portails) ---
        // Renvoie false si le segment est parallèle au plan.
        // t ∈ [0,1] au point de coupe.
        [[nodiscard]] bool IntersectSegment(const Vec3f& a, const Vec3f& b,
            float& t, Vec3f& hit) const noexcept
        {
            const float da = SignedDistance(a);
            const float db = SignedDistance(b);
            const float den = da - db;
            if (std::fabs(den) < NEAR_ZERO) return false;
            t = da / den;
            hit = a + (b - a) * t;
            return true;
        }
    };
}