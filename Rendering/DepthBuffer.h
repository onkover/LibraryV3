#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include "ClearKernels.h"

namespace LV3
{
    // C1 : SOURCE UNIQUE du test de profondeur (reverse-Z, GREATER).
    // Utilise par TestAndSet (chemin generique) ET par les shaders C1 (Fragment.h).
    [[nodiscard]] LV3_FORCEINLINE bool DepthTestAndSet(float& dst, float z) noexcept
    {
        if (z <= dst) return false;          // REVERSE-Z : on garde le PLUS GRAND
        dst = z;
        return true;
    }


    // Z-buffer REVERSE-Z : 0 = far, 1 = near. On garde le PLUS GRAND.
    // Possede sa memoire, contrairement au FrameBuffer.
    class DepthBuffer
    {
    public:
        void Resize(int32_t w, int32_t h)
        {
            m_Width = w; m_Height = h;
            m_Data.assign(size_t(w) * size_t(h), 0.0f);
        }


        // Variante 0 : std::fill(float*, 0.0f) -> memset -> rep stosb, comme avant.
        // REVERSE-Z : 0 = far. Stores non temporels : voir ClearKernels.h.
        void Clear() noexcept
        {
            ClearKernel::StreamFill(m_Data.data(), m_Data.size(), 0.0f);
            ClearKernel::Fence();
        }

        // Test ET ecriture en une operation : un seul calcul d'index,
        // et impossible d'oublier l'ecriture apres un test reussi.
        [[nodiscard]] inline bool TestAndSet(int32_t x, int32_t y, float z) noexcept
        {
            return DepthTestAndSet(m_Data[size_t(y) * size_t(m_Width) + size_t(x)], z);
        }

        // C1 : debut de la ligne y. La boucle pixels le calcule UNE fois par ligne,
        // puis indexe par x : plus de multiplication y * largeur par pixel.
        [[nodiscard]] LV3_FORCEINLINE float* Row(int32_t y) noexcept
        {
            LV3_ASSERT(y >= 0 && y < m_Height && "DepthBuffer::Row : ligne hors du tampon");
            return m_Data.data() + size_t(y) * size_t(m_Width);
        }

        int32_t Width()  const noexcept { return m_Width; }
        int32_t Height() const noexcept { return m_Height; }

        // Adresse du premier float : sonde C0b (4K-aliasing) uniquement. Lecture seule.
        [[nodiscard]] const float* Data() const noexcept { return m_Data.data(); }


    private:
        std::vector<float> m_Data;
        int32_t m_Width = 0, m_Height = 0;
    };
}