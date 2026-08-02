#pragma once
// ============================================================
//  Maths/Geometry/Frustum.h — Volume de vision : 6 plans
//
//  Un frustum n'est QUE six plans. Il ne connaît ni la lentille
//  (fov, focale -> CameraComponent), ni l'écran (aspect, taille
//  -> Viewport), ni la géométrie (Mesh, Poly).
//  Il est le RÉSULTAT de View·Projection, jamais son producteur.
// ============================================================
#include <cstdint>
#include "Plane.h"
#include "AABB3d.h"
#include "../MatrixLib.h"

namespace LV3
{
    // Résultat ternaire. Remplace PointVSPlane, FrustumInside et PlaneSide.
    enum class EIntersect : uint8_t { Outside = 0, Intersect = 1, Inside = 2 };

    class Frustum
    {
    public:
        // /!\ Far DOIT rester en dernier : en far infini on tronque à 5 plans.
        enum EPlane : int { Left = 0, Right, Bottom, Top, Near, Far, PlaneCount };

        // viewProj = view * projection   (vecteur-ligne : v' = v·V·P)
        // reverseZ    : true  si la projection mappe near->1, far->0
        // infiniteFar : true  si far = infini (le plan Far est alors dégénéré)
        void Build(const Matrix44f& viewProj, bool reverseZ, bool infiniteFar) noexcept;

        // Test complet, trois états. C'est LA fonction de culling.
        [[nodiscard]] EIntersect Classify(const AABB3d& box) const noexcept;

        // Raccourci booléen : « potentiellement visible » (Inside OU Intersect).
        [[nodiscard]] LV3_FORCEINLINE bool IsVisible(const AABB3d& box) const noexcept
        {
            return Classify(box) != EIntersect::Outside;
        }

        [[nodiscard]] bool Contains(const Vec3f& point) const noexcept;
        [[nodiscard]] bool Intersects(const Vec3f& center, float radius) const noexcept;

        [[nodiscard]] LV3_FORCEINLINE const Plane& operator[](int i) const noexcept { return m_planes[i]; }
        [[nodiscard]] LV3_FORCEINLINE int           Count()          const noexcept { return m_count; }

    private:
        Plane m_planes[PlaneCount];
        int   m_count = PlaneCount;      // 5 si far infini
    };
}