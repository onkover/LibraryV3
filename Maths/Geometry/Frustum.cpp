#include "pch.h"
#include "Frustum.h"

namespace LV3
{
    /* ============================================================
       Extraction Gribb-Hartmann.

       Convention LV3 : vecteur-ligne, v' = v·M.
           clip.j = Σ_i v[i]·M[i][j] + M[3][j]
       Donc la COLONNE j de M porte le composant j du vecteur clip :
           normale = ( M[0][j], M[1][j], M[2][j] )    d = M[3][j]

       Volume canonique, NDC z ∈ [0,1] :
           -w ≤ x ≤ w      -w ≤ y ≤ w      0 ≤ z ≤ w

       Les deux conditions en profondeur sont  (z ≥ 0)  et  (w − z ≥ 0).
           Standard   :  z ≥ 0  est le plan NEAR
           Reverse-Z  :  z ≥ 0  est le plan FAR
       -> on échange simplement les deux étiquettes.

       Les normales pointent vers l'INTÉRIEUR par construction :
       SignedDistance > 0 signifie « du bon côté ».
       ============================================================ */
    void Frustum::Build(const Matrix44f& M, bool reverseZ, bool infiniteFar) noexcept
    {
        auto col = [&M](int j) noexcept -> Plane
            {
                return Plane(Vec3f(M[0][j], M[1][j], M[2][j]), M[3][j]);
            };
        auto add = [](const Plane& a, const Plane& b) noexcept -> Plane
            {
                return Plane(a.normal + b.normal, a.d + b.d);
            };
        auto sub = [](const Plane& a, const Plane& b) noexcept -> Plane
            {
                return Plane(a.normal - b.normal, a.d - b.d);
            };

        const Plane cx = col(0), cy = col(1), cz = col(2), cw = col(3);

        m_planes[Left] = add(cw, cx);      //  x + w ≥ 0
        m_planes[Right] = sub(cw, cx);      //  w − x ≥ 0
        m_planes[Bottom] = add(cw, cy);      //  y + w ≥ 0
        m_planes[Top] = sub(cw, cy);      //  w − y ≥ 0

        if (reverseZ) { m_planes[Near] = sub(cw, cz); m_planes[Far] = cz; }
        else { m_planes[Near] = cz;          m_planes[Far] = sub(cw, cz); }

        // Far infini : m[2][2] = 0 -> la colonne z donne une normale nulle.
        // Le plan est dégénéré, on ne le teste pas. (Far est le dernier de l'enum.)
        m_count = infiniteFar ? static_cast<int>(Far) : static_cast<int>(PlaneCount);

        for (int i = 0; i < m_count; ++i) m_planes[i].Normalize();
    }

    /* ------------------------------------------------------------
       Test p-vertex / n-vertex : 1 à 2 produits scalaires par plan,
       jamais 8 coins.
           p-vertex derrière un plan  -> les 8 le sont  -> Outside
           n-vertex devant tous       -> Inside complet
       Un mesh Inside n'a AUCUN triangle à tester ni à clipper.
       ------------------------------------------------------------ */
    EIntersect Frustum::Classify(const AABB3d& box) const noexcept
    {
        EIntersect result = EIntersect::Inside;

        for (int i = 0; i < m_count; ++i)
        {
            const Plane& p = m_planes[i];

            if (p.SignedDistance(box.PVertex(p.normal)) < 0.0f)
                return EIntersect::Outside;                  // sortie immédiate

            if (p.SignedDistance(box.NVertex(p.normal)) < 0.0f)
                result = EIntersect::Intersect;              // à confirmer sur les autres plans
        }
        return result;
    }

    bool Frustum::Contains(const Vec3f& pt) const noexcept
    {
        for (int i = 0; i < m_count; ++i)
            if (m_planes[i].SignedDistance(pt) < 0.0f) return false;
        return true;
    }

    bool Frustum::Intersects(const Vec3f& center, float radius) const noexcept
    {
        for (int i = 0; i < m_count; ++i)
            if (m_planes[i].SignedDistance(center) < -radius) return false;
        return true;
    }
}