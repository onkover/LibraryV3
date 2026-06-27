#pragma once
#include "Vec3f.h"
namespace LibV2 {
    enum class PlaneSide : uint8_t { Front, Back, OnPlane };
    struct Plane {
        Vec3f normal = Vec3f::Up();
        float d = 0.f;
        constexpr Plane() noexcept = default;
        Plane(const Vec3f& n, float d_)          noexcept : normal(n), d(d_) {}
        Plane(const Vec3f& n, const Vec3f& pt)   noexcept
            : normal(n.normalize()), d(-n.normalize().dot(pt)) {}
        LV2_FORCEINLINE float DistanceTo(const Vec3f& p) const noexcept { return normal.dot(p)+d; }
        LV2_FORCEINLINE PlaneSide Classify(const Vec3f& p) const noexcept {
            float dist = DistanceTo(p);
            if (dist >  1e-5f) return PlaneSide::Front;
            if (dist < -1e-5f) return PlaneSide::Back;
            return PlaneSide::OnPlane;
        }
        static Plane FromPoints(const Vec3f& a, const Vec3f& b, const Vec3f& c) noexcept {
            return Plane{(b-a).cross(c-a).normalize(), a};
        }
        [[nodiscard]] Plane Normalized() const noexcept {
            float len = normal.length();
            if (len < 1e-6f) return *this;
            return Plane{normal/len, d/len};
        }
    };
} // namespace LibV2
