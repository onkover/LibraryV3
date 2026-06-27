#pragma once
#include "Plane.h"
#include "AABB3d.h"
#include "Matrix44f.h"
namespace LibV2 {
    struct Frustum {
        Plane planes[6];
        enum PlaneIdx : int { Near=0, Far=1, Left=2, Right=3, Top=4, Bottom=5 };
        float fovDeg=60.f, aspect=16.f/9.f, nearDist=0.1f, farDist=1000.f;
        float focalX=1.f, focalY=1.f;
        void Rebuild(const Matrix44f& proj, const Matrix44f& view) noexcept;
        bool Contains(const Vec3f& point)   const noexcept;
        bool Contains(const AABB3d& aabb)   const noexcept;
        bool Intersects(const AABB3d& aabb) const noexcept;
        void UpdateFocalFactors(int screenW, int screenH) noexcept;
    };
} // namespace LibV2
