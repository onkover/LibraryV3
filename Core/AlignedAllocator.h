#pragma once
// ============================================================
//  std::allocator ne garantit que 16 o sur x64. On aligne sur 64 o pour
//  qu'aucun acces SIMD ne chevauche deux lignes de cache.
//  ATTENTION : MSVC compile _mm256_load_ps / _store_ps en vmovups (forme NON
//  alignee) : une adresse mal alignee ne leve AUCUNE faute en Release, elle
//  coute seulement des lignes coupees. Seules les LV3_ASSERT d'alignement
//  (Debug) attrapent l'erreur.
//  EXIGENT 32 o : sinon faute materielle, l'erreur ne passe jamais inapercue.
// ============================================================
#include <cstddef>
#include <new>
#include <vector>

namespace LV3
{
    inline constexpr std::size_t kSimdAlign = 64;  // ligne de cache ; couvre AVX2 (32) et AVX-512 (64)
    inline constexpr std::size_t kSimdWidthF32 = 8;   // floats par registre AVX2

    template <class T, std::size_t Align>
    struct AlignedAllocator
    {
        static_assert(Align >= alignof(T) && (Align & (Align - 1)) == 0, "puissance de 2, >= alignof(T)");
        using value_type = T;

        // Obligatoire : allocator_traits ne sait pas "rebind" un parametre NON-type (Align).
        template <class U> struct rebind { using other = AlignedAllocator<U, Align>; };

        AlignedAllocator() noexcept = default;
        template <class U> AlignedAllocator(const AlignedAllocator<U, Align>&) noexcept {}

        [[nodiscard]] T* allocate(std::size_t n)
        {
            if (n > std::size_t(-1) / sizeof(T)) throw std::bad_array_new_length();
            return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{ Align }));
        }
        void deallocate(T* p, std::size_t) noexcept
        {
            ::operator delete(p, std::align_val_t{ Align });   // MEME align_val_t qu'a l'allocation
        }

        template <class U> bool operator==(const AlignedAllocator<U, Align>&) const noexcept { return true; }
        template <class U> bool operator!=(const AlignedAllocator<U, Align>&) const noexcept { return false; }
    };

    template <class T>
    using SimdVector = std::vector<T, AlignedAllocator<T, kSimdAlign>>;
}