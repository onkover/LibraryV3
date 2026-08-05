#pragma once
#include <cstdint>
#include <cstring>

namespace LV3
{

    struct Color { uint8_t b, g, r, a; }; // ordre mémoire pour SDL_PIXELFORMAT_ARGB8888 (little-endian)

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

        inline void SetPixel(int32_t x, int32_t y, Color color)
        {
            if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;

            uint8_t* row = reinterpret_cast<uint8_t*>(m_Pixels) + y * m_Pitch;
            reinterpret_cast<uint32_t*>(row)[x] = std::bit_cast<uint32_t>(color);
        }

        int32_t Width()  const { return m_Width; }
        int32_t Height() const { return m_Height; }

    private:
        void* m_Pixels = nullptr;
        int32_t m_Pitch = 0;
        int32_t m_Width = 0;
        int32_t m_Height = 0;
    };

} // namespace LV3