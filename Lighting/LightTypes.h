#pragma once
#include <cstdint>

// ============================================================
//  Lighting/LightTypes.h — Enums du système d'éclairage
//  Inclure manuellement dans les .cpp qui gèrent les lumières
// ============================================================

namespace LV3
{

    // Type de lumière — quel type de source lumineuse est utilisé
    enum class ELightType : uint8_t
    {
        Directional,   // Soleil — direction infinie, pas de position
        Point,         // Ampoule — position, atténuation sphérique
        Spot,          // Projecteur — position + cône (innerAngle / outerAngle)
        Ambient        // Lumière globale — aucune direction, affecte tout
    };

    // Comportement des ombres portées
    enum class EShadowMode : uint8_t
    {
        None,          // Pas d'ombre (perf max)
        Hard,          // Ombre franche — shadow map basique
        Soft           // Ombre douce — PCF (Percentage Closer Filtering)
    };

    // Loi d'atténuation de l'intensité avec la distance
    enum class EAttenuationMode : uint8_t
    {
        None,          // Pas d'atténuation (débogage)
        Linear,        // f = 1 - (d / range)         simple, peu réaliste
        Quadratic,     // f = 1 / (1 + k*d²)          physiquement correct
        InverseSquare  // f = (range / max(d, 0.01))²  standard Unreal
    };

} // namespace LV3