#pragma once
// ============================================================
//  Platform.h — Couche d'abstraction OS
//  À inclure UNIQUEMENT dans les .cpp qui en ont besoin directement.
//  NE PAS inclure dans pch.h (pollution des macros Win32).
// ============================================================

#include "Compiler.h"   // réexporte LV2_FORCEINLINE

#if defined(_WIN32)
    #define LV2_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define VC_EXTRALEAN
    #include <windows.h>
    using WindowHandle  = HWND;
    using DeviceContext = HDC;
#elif defined(__linux__)
    #define LV2_PLATFORM_LINUX
    using WindowHandle  = unsigned long;
    using DeviceContext = void*;
#elif defined(__APPLE__)
    #define LV2_PLATFORM_MACOS
    using WindowHandle  = void*;
    using DeviceContext = void*;
#else
    #error "Plateforme non supportée"
#endif

//// Alignement mémoire SIMD
//#define LV2_ALIGN_SIMD  alignas(32)   // AVX2
//#define LV2_ALIGN_CACHE alignas(64)   // Ligne de cache
//
//// Force-inline
//#if defined(_MSC_VER)
//    #define LV2_FORCEINLINE __forceinline
//#else
//    #define LV2_FORCEINLINE __attribute__((always_inline)) inline
//#endif
