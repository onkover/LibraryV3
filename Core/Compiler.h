//#pragma once
//
//// Alignement mémoire SIMD
//#define LV2_ALIGN_SIMD  alignas(32)   // AVX2
//#define LV2_ALIGN_CACHE alignas(64)   // Ligne de cache
//
//// Force-inline
//#if defined(_MSC_VER)
//    #define LV3_FORCEINLINE __forceinline
//#else
//    #define LV3_FORCEINLINE __attribute__((always_inline)) inline
//#endif


#pragma once
// ============================================================
//  Core/Compiler.h — Macros compilateur (DANS le PCH)
//  Zéro pollution, zéro include lourd. Utilisé partout.
// ============================================================

// --- Alignement mémoire SIMD ---
#define LV3_ALIGN_SIMD  alignas(32)   // AVX2 (256 bits)
#define LV3_ALIGN_CACHE alignas(64)   // Ligne de cache

// --- Force-inline ---
#if defined(_MSC_VER)
	#define LV3_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
	#define LV3_FORCEINLINE __attribute__((always_inline)) inline
#else
	#define LV3_FORCEINLINE inline
#endif
