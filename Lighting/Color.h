#pragma once
#include <cstdint>
//#include <cassert>
#include <bit>
namespace LV3
{

    struct Color { uint8_t b, g, r, a; }; // ordre mémoire pour SDL_PIXELFORMAT_ARGB8888 (little-endian)
    static_assert(sizeof(Color) == 4, "Color doit faire exactement 4 octets pour le bit_cast");
    static_assert(std::is_trivially_copyable_v<Color>, "Color doit rester trivialement copiable");

    // Construit une couleur depuis r, g, b — dans l'ordre naturel.
    // /!\ NE PAS l'appeler RGB : c'est une macro de wingdi.h.
    [[nodiscard]] constexpr Color MakeColor(uint8_t r, uint8_t g, uint8_t b,
        uint8_t a = 255) noexcept
    {
        return { b, g, r, a };
    }

}