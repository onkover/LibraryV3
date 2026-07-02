#pragma once
#include "Vectorlib.h"
#include <algorithm>
namespace LV3
{
    struct AABB3d {
        Vec3f min{ 1e30f, 1e30f, 1e30f};
        Vec3f max{-1e30f,-1e30f,-1e30f};

        constexpr AABB3d() noexcept = default;
        AABB3d(const Vec3f& mn, const Vec3f& mx) noexcept : min(mn), max(mx) {}

        void Reset() noexcept { min={1e30f,1e30f,1e30f}; max={-1e30f,-1e30f,-1e30f}; }

        LV3_FORCEINLINE bool IsValid() const noexcept { return min.x <= max.x; }
        LV3_FORCEINLINE Vec3f Center() const noexcept { return (min+max)*0.5f; }
        LV3_FORCEINLINE Vec3f Extent() const noexcept { return (max-min)*0.5f; }

        LV3_FORCEINLINE void Expand(const Vec3f& p) noexcept {
            if (p.x<min.x) min.x=p.x; if (p.y<min.y) min.y=p.y; if (p.z<min.z) min.z=p.z;
            if (p.x>max.x) max.x=p.x; if (p.y>max.y) max.y=p.y; if (p.z>max.z) max.z=p.z;
        }
        LV3_FORCEINLINE bool Contains(const Vec3f& p) const noexcept {
            return p.x>=min.x&&p.x<=max.x&&p.y>=min.y&&p.y<=max.y&&p.z>=min.z&&p.z<=max.z;
        }
        LV3_FORCEINLINE bool Intersects(const AABB3d& b) const noexcept {
            return max.x>=b.min.x&&min.x<=b.max.x&&max.y>=b.min.y&&min.y<=b.max.y&&max.z>=b.min.z&&min.z<=b.max.z;
        }
        LV3_FORCEINLINE AABB3d Union(const AABB3d& b) const noexcept {
            return {
                Vec3f(std::min(min.x,b.min.x),std::min(min.y,b.min.y),std::min(min.z,b.min.z)),
                Vec3f(std::max(max.x,b.max.x),std::max(max.y,b.max.y),std::max(max.z,b.max.z))
            };
        }
    };
} // namespace 
