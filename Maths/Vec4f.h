#pragma once
#include "Vec3f.h"
namespace LibV3 {
    struct Vec4f {
        float x=0.f, y=0.f, z=0.f, w=1.f;
        constexpr Vec4f() noexcept = default;
        constexpr Vec4f(float x_,float y_,float z_,float w_) noexcept : x(x_),y(y_),z(z_),w(w_) {}
        explicit constexpr Vec4f(const Vec3f& v, float w_=1.f) noexcept : x(v.x),y(v.y),z(v.z),w(w_) {}

        LV2_FORCEINLINE Vec3f xyz() const noexcept { return {x,y,z}; }
        LV2_FORCEINLINE Vec4f operator+(const Vec4f& o) const noexcept { return {x+o.x,y+o.y,z+o.z,w+o.w}; }
        LV2_FORCEINLINE Vec4f operator-(const Vec4f& o) const noexcept { return {x-o.x,y-o.y,z-o.z,w-o.w}; }
        LV2_FORCEINLINE Vec4f operator*(float s)        const noexcept { return {x*s,y*s,z*s,w*s}; }
    };
} // namespace LibV3
