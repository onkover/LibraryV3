#pragma once
#include <algorithm>

namespace LV3
{

    // Ramène une valeur dans [0,1]. Terme standard des langages de shading.
    [[nodiscard]] LV3_FORCEINLINE constexpr float Saturate(float v) noexcept
    {
        return (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
    }
    
    [[nodiscard]] LV3_FORCEINLINE constexpr float Lerp(float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }


} // namespace LV3