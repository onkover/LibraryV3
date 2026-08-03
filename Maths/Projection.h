#pragma once
// ============================================================
//  Maths/Projection.h — Fabriques de matrices de projection LV3
//
//  Fonctions LIBRES et PURES : entrée = scalaires, sortie = matrice.
//  Aucune dépendance à Camera, Frustum, Viewport ni à l'ECS.
//  -> utilisable pour une caméra, une shadow map, une face de
//     cubemap, un portail, une sonde de réflexion.
//
//  Convention LV3 :
//      main DROITE (la vue regarde vers -Z)
//      vecteur-ligne, v' = v·M, row-major x[row][col]
//      NDC  x,y ∈ [-1,+1]
//      NDC  z   ∈ [0,1]  REVERSE-Z : near -> 1, far -> 0
// ============================================================
#include <cstdint>
#include <cmath>
#include "MatrixLib.h"
#include "../Core/config.h"      // TO_RADIAN

namespace LV3
{
	// Type de projection : perspective ou orthographique.
    enum class EProjectionType : uint8_t { Perspective, Orthographic };

    // Comment paramétrer la perspective : angle simple, ou modèle sténopé.
    enum class ELensModel : uint8_t
    {
        FieldOfView,   // fovY en degrés — vocabulaire jeu (Unity, Godot)
        Filmback       // focale + pellicule — vocabulaire film (Maya, CineCamera)
    };

    // Comment la fenêtre "coupe" la pellicule quand leurs ratios diffèrent. Terminologie Maya
    enum class EGateFit : uint8_t
    {
        Fill,       // la pellicule tient dans la fenêtre (on rogne)
        Overscan    // la pellicule déborde de la fenêtre (on remplit)
    };
}

//**********************************************
namespace LV3::Projection
{
    // --- Perspective ----------------------------------------------------
    // Frustum asymétrique : la primitive. Sert aux cascades d'ombres, à la VR (décentrement par œil) et aux portails.
    [[nodiscard]] Matrix44f PerspectiveOffCenter(float l, float r, float b, float t,
        float nearZ, float farZ) noexcept;

    // Cas symétrique. fovYRad = champ de vision VERTICAL, en radians.
    [[nodiscard]] Matrix44f Perspective(float fovYRad, float aspect,
        float nearZ, float farZ) noexcept;

    // Far infini : z_ndc = near / distance. Aucun plan far.
    [[nodiscard]] Matrix44f PerspectiveInfinite(float fovYRad, float aspect,
        float nearZ) noexcept;

    // Paramétrage "film" (sténopé) : focale + pellicule + ajustement de gate.
    [[nodiscard]] Matrix44f PerspectiveFilmback(float focalMm,
        float filmWidthMm, float filmHeightMm,
        float deviceAspect,
        float nearZ, float farZ,
        EGateFit fit = EGateFit::Fill) noexcept;


    // --- Orthographique -------------------------------------------------
    [[nodiscard]] Matrix44f Orthographic(float l, float r, float b, float t,
        float nearZ, float farZ) noexcept;

    [[nodiscard]] Matrix44f OrthographicCentered(float height, float aspect,
        float nearZ, float farZ) noexcept;


    // --- Conversions sténopé (inline, sans .cpp) ------------------------
    [[nodiscard]] inline float FovYFromFocal(float focalMm, float filmHeightMm) noexcept
    {
        return 2.0f * std::atan((filmHeightMm * 0.5f) / focalMm);
    }
    [[nodiscard]] inline float FocalFromFovY(float fovYRad, float filmHeightMm) noexcept
    {
        return (filmHeightMm * 0.5f) / std::tan(fovYRad * 0.5f);
    }
    // FOV horizontal <-> vertical (le moteur raisonne toujours en VERTICAL)
    [[nodiscard]] inline float FovXFromFovY(float fovYRad, float aspect) noexcept
    {
        return 2.0f * std::atan(std::tan(fovYRad * 0.5f) * aspect);
    }
    [[nodiscard]] inline float FovYFromFovX(float fovXRad, float aspect) noexcept
    {
        return 2.0f * std::atan(std::tan(fovXRad * 0.5f) / aspect);
    }
}