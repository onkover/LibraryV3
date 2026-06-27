#pragma once

// Alignement mémoire SIMD
#define LV2_ALIGN_SIMD  alignas(32)   // AVX2
#define LV2_ALIGN_CACHE alignas(64)   // Ligne de cache

// Force-inline
#if defined(_MSC_VER)
    #define LV2_FORCEINLINE __forceinline
#else
    #define LV2_FORCEINLINE __attribute__((always_inline)) inline
#endif

