#pragma once
#include "../Maths/MatrixLib.h"

namespace LV3
{

    // Sommet en espace de CLIP, avant division par w.
    //
    // ⚠️ CONTRAT DE MAINTENANCE — À LIRE AVANT D'AJOUTER UN CHAMP
    // Tout attribut ajouté ici DOIT être ajouté dans Lerp() ci-dessous.
    // Un attribut oublié vaut zéro UNIQUEMENT sur les triangles clippés,
    // donc uniquement quand la caméra frôle l'objet. Bug fantôme garanti.
    struct ClipVertex
    {
        Vec4f clip;      // (x, y, z, w) — sortie de MulRow(mvp, position)
        // uv     : différé — dette MeshClass::GetFaceView
        // normal : différé — idem
    };

    [[nodiscard]] LV3_FORCEINLINE ClipVertex Lerp(const ClipVertex& a,
        const ClipVertex& b,
        float t) noexcept
    {
        ClipVertex r;
        r.clip = a.clip + (b.clip - a.clip) * t;
        // AJOUTER ICI tout nouvel attribut de ClipVertex.
        return r;
    }

    // Distance signée au plan near, forme CANONIQUE du volume de clip.
    // Ne connaît ni near, ni far, ni le type de projection (perspective/ortho).
    // ≥ 0  →  conservé.
    [[nodiscard]] LV3_FORCEINLINE float NearDistance(const ClipVertex& v) noexcept
    {
        return v.clip.w - v.clip.z;      // reverse-Z : w - z est le plan NEAR
    }

} // namespace LV3