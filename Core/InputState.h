#pragma once
// ============================================================
//  Core/InputState.h — État des entrées pour une frame
//
//  Défini par le MOTEUR, rempli par l'APPLICATION.
//  Le moteur ignore SDL, Win32 et tout périphérique : il ne
//  voit que des intentions déjà traduites.
//  POD pur : copiable, enregistrable (replay), rejouable (tests).
// ============================================================
#include "Compiler.h"

namespace LV3
{
    struct InputState
    {
        // --- Souris : DÉPLACEMENT de la frame, en pixels ---
        //     Ce n'est PAS une vitesse : ne jamais multiplier par deltaTime.
        int mouseDeltaX = 0;
        int mouseDeltaY = 0;
        int wheelDelta = 0;         // molette : zoom / vitesse

        // --- Intentions de déplacement (touches maintenues) ---
        bool moveForward = false;
        bool moveBackward = false;
        bool strafeLeft = false;
        bool strafeRight = false;
        bool moveUp = false;   // vol libre uniquement
        bool moveDown = false;

        // --- Modificateurs ---
        bool sprint = false;         // Shift

        // --- Actions (front montant, pas maintien) ---
        bool toggleCameraMode = false;

        LV3_FORCEINLINE void Reset() noexcept { *this = InputState{}; }
    };
}