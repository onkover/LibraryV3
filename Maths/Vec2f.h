#pragma once
//#include "../Core/Platform.h"
#include "../Core/Compiler.h"
#include <cmath>
namespace LibV3 {
    struct Vec2f {
        float x = 0.f, y = 0.f;
        constexpr Vec2f() noexcept = default;
        constexpr Vec2f(float x_, float y_) noexcept : x(x_), y(y_) {}

        // Accesseurs alias UV
        LV2_FORCEINLINE float& u() noexcept { return x; }
        LV2_FORCEINLINE float& v() noexcept { return y; }
        LV2_FORCEINLINE float  u() const noexcept { return x; }
        LV2_FORCEINLINE float  v() const noexcept { return y; }

        LV2_FORCEINLINE Vec2f  operator+(const Vec2f& o) const noexcept { return {x+o.x, y+o.y}; }
        LV2_FORCEINLINE Vec2f  operator-(const Vec2f& o) const noexcept { return {x-o.x, y-o.y}; }
        LV2_FORCEINLINE Vec2f  operator*(float s)        const noexcept { return {x*s, y*s}; }
        LV2_FORCEINLINE Vec2f& operator+=(const Vec2f& o) noexcept { x+=o.x; y+=o.y; return *this; }
        LV2_FORCEINLINE float  dot(const Vec2f& o)       const noexcept { return x*o.x + y*o.y; }
        LV2_FORCEINLINE float  length()                  const noexcept { return std::sqrt(x*x + y*y); }

        static constexpr Vec2f Zero() noexcept { return {0,0}; }
        static constexpr Vec2f One()  noexcept { return {1,1}; }
    };
} // namespace LibV3
