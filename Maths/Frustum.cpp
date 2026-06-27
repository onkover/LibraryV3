#include "pch.h"
#include "Frustum.h"
namespace LibV2 {
    void Frustum::Rebuild(const Matrix44f& proj, const Matrix44f& view) noexcept {
        Matrix44f vp = proj * view;
        const float* M = vp.data();
        auto norm=[&](Plane& p){ float l=p.normal.length(); if(l>1e-6f){p.normal=p.normal/l;p.d/=l;} };
        // Gribb-Hartmann
        planes[Left  ]={{M[3]+M[0],M[7]+M[4],M[11]+M[8]},  M[15]+M[12]};
        planes[Right ]={{M[3]-M[0],M[7]-M[4],M[11]-M[8]},  M[15]-M[12]};
        planes[Bottom]={{M[3]+M[1],M[7]+M[5],M[11]+M[9]},  M[15]+M[13]};
        planes[Top   ]={{M[3]-M[1],M[7]-M[5],M[11]-M[9]},  M[15]-M[13]};
        planes[Near  ]={{M[3]+M[2],M[7]+M[6],M[11]+M[10]}, M[15]+M[14]};
        planes[Far   ]={{M[3]-M[2],M[7]-M[6],M[11]-M[10]}, M[15]-M[14]};
        for (auto& p : planes) norm(p);
    }
    bool Frustum::Contains(const Vec3f& p) const noexcept {
        for (const auto& pl : planes) if (pl.DistanceTo(p)<0.f) return false;
        return true;
    }
    bool Frustum::Contains(const AABB3d& aabb) const noexcept {
        for (const auto& pl : planes) {
            Vec3f pv{pl.normal.x>=0.f?aabb.max.x:aabb.min.x,
                     pl.normal.y>=0.f?aabb.max.y:aabb.min.y,
                     pl.normal.z>=0.f?aabb.max.z:aabb.min.z};
            if (pl.DistanceTo(pv)<0.f) return false;
        }
        return true;
    }
    bool Frustum::Intersects(const AABB3d& aabb) const noexcept { return Contains(aabb); }
    void Frustum::UpdateFocalFactors(int screenW, int screenH) noexcept {
        aspect = (screenH>0) ? float(screenW)/float(screenH) : 16.f/9.f;
        float f = 1.f/std::tan(fovDeg*(3.14159265f/180.f)*0.5f);
        focalX = f/aspect; focalY = f;
    }
} // namespace LibV2
