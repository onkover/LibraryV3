#pragma once
#include <cstdint>
#include <cstring>
#include"rendering/viewport.h"

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


    class FrameBuffer
    {
    public:
        void Bind(void* lockedPixels, int32_t pitchBytes, int32_t w, int32_t h)
        {
			m_Pixels = lockedPixels;
            m_Pitch = pitchBytes;
            m_Width = w;
            m_Height = h;

            assert(lockedPixels != nullptr && "FrameBuffer::Bind — pixels null");
            assert(pitchBytes > 0 && "FrameBuffer::Bind — pitch invalide (SDL_LockTexture a-t-il réussi ?)");

        }
        void Unbind() noexcept { m_Pixels = nullptr; m_Pitch = 0; }
        [[nodiscard]] bool IsBound() const noexcept { return m_Pixels != nullptr; }



        // Avec test de bornes. Pour le fil de fer, le debug, tout ce qui
           // peut deborder.
        inline void SetPixel(int32_t x, int32_t y, Color color) noexcept
        {
            if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
            SetPixelUnchecked(x, y, color);
        }

        inline void SetPixelUnchecked(int32_t x, int32_t y, Color color)
        {
            uint8_t* row = reinterpret_cast<uint8_t*>(m_Pixels) + y * m_Pitch;
            reinterpret_cast<uint32_t*>(row)[x] = std::bit_cast<uint32_t>(color);
        }

        // Efface ligne par ligne EN UTILISANT LE PITCH.
                // Jamais un memset global : les lignes peuvent etre espacees.
        void Clear(Color color) noexcept
        {
            const uint32_t v = std::bit_cast<uint32_t>(color);
            for (int32_t y = 0; y < m_Height; ++y)
            {
                uint32_t* row = reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uint8_t*>(m_Pixels) + y * m_Pitch);
                std::fill(row, row + m_Width, v);
            }
        }

        //LV3_FORCEINLINE float& DepthAt(int x, int y) noexcept
        //{
        //    return m_depth[size_t(y) * size_t(m_Width) + size_t(x)];
        //}

        // Le viewport DERIVE de la cible. Une seule source de verite.
        //[[nodiscard]] Viewport GetViewport() const noexcept
        //{
        //    return Viewport::FullScreen(m_width, m_height);
        //}

        [[nodiscard]] int32_t Width()  const { return m_Width; }
        [[nodiscard]] int32_t Height() const { return m_Height; }


//    private:
        void* m_Pixels = nullptr;       // adr de l'écran rendu par SDL_LockTexture
        int32_t m_Pitch = 0;            // celui que rend SDL_LockTexture, en OCTETS.
        int32_t m_Width = 0;
        int32_t m_Height = 0;
    };

} // namespace LV3