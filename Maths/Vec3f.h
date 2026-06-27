#pragma once
#include <cmath>
//#include "../Core/Platform.h"
#include "../Core/Compiler.h"
namespace LibV3
{
    struct Vec3f {
        float x = 0.f, y = 0.f, z = 0.f;
        constexpr Vec3f() noexcept = default;
        constexpr Vec3f(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

        LV2_FORCEINLINE Vec3f  operator+(const Vec3f& v) const noexcept { return {x+v.x, y+v.y, z+v.z}; }
        LV2_FORCEINLINE Vec3f  operator-(const Vec3f& v) const noexcept { return {x-v.x, y-v.y, z-v.z}; }
        LV2_FORCEINLINE Vec3f  operator*(float s)        const noexcept { return {x*s, y*s, z*s}; }
        LV2_FORCEINLINE Vec3f  operator/(float s)        const noexcept { float i=1.f/s; return {x*i,y*i,z*i}; }
        LV2_FORCEINLINE Vec3f  operator-()               const noexcept { return {-x,-y,-z}; }
        LV2_FORCEINLINE Vec3f& operator+=(const Vec3f& v) noexcept { x+=v.x; y+=v.y; z+=v.z; return *this; }
        LV2_FORCEINLINE Vec3f& operator-=(const Vec3f& v) noexcept { x-=v.x; y-=v.y; z-=v.z; return *this; }
        LV2_FORCEINLINE Vec3f& operator*=(float s)        noexcept { x*=s; y*=s; z*=s; return *this; }

        LV2_FORCEINLINE float dot(const Vec3f& v)  const noexcept { return x*v.x+y*v.y+z*v.z; }
        LV2_FORCEINLINE Vec3f cross(const Vec3f& v) const noexcept {
            return {y*v.z-z*v.y, z*v.x-x*v.z, x*v.y-y*v.x};
        }
        LV2_FORCEINLINE float lengthSq() const noexcept { return x*x+y*y+z*z; }
        LV2_FORCEINLINE float length()   const noexcept { return std::sqrt(lengthSq()); }
        LV2_FORCEINLINE Vec3f normalize() const noexcept {
            float i = 1.f / length(); return {x*i,y*i,z*i};
        }
        LV2_FORCEINLINE bool operator==(const Vec3f& o) const noexcept = default;

        static constexpr Vec3f Zero()    noexcept { return {0,0,0}; }
        static constexpr Vec3f One()     noexcept { return {1,1,1}; }
        static constexpr Vec3f Up()      noexcept { return {0,1,0}; }
        static constexpr Vec3f Forward() noexcept { return {0,0,1}; }
        static constexpr Vec3f Right()   noexcept { return {1,0,0}; }
        static Vec3f lerp(const Vec3f& a, const Vec3f& b, float t) noexcept {
            return a + (b - a) * t;
        }
    };
    LV2_FORCEINLINE Vec3f operator*(float s, const Vec3f& v) noexcept { return v * s; }

} // namespace LibV3
