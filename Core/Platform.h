#pragma once
// ============================================================
//  Platform.h — Couche d'abstraction OS
//  Inclure manuellement uniquement là où on crée/gère des fenêtres
// 
//  À inclure UNIQUEMENT dans les .cpp qui en ont besoin directement.
//  NE PAS inclure dans pch.h (pollution des macros Win32).
// ============================================================


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

