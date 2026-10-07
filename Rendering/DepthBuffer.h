#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include "ClearKernels.h"

namespace LV3
{
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


        // SONDE 2a : noinline + debugbreak, lecture du desassemblage. A RETIRER.
        //__declspec(noinline) void Clear() noexcept
        //{
        // //   __debugbreak();
        // ....
        //}

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
            float& dst = m_Data[size_t(y) * size_t(m_Width) + size_t(x)];
            if (z <= dst) return false;          // REVERSE-Z : test GREATER
            dst = z;
            return true;
        }

        int32_t Width()  const noexcept { return m_Width; }
        int32_t Height() const noexcept { return m_Height; }

    private:
        std::vector<float> m_Data;
        int32_t m_Width = 0, m_Height = 0;
    };
}