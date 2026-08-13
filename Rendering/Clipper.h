#pragma once
#include "ClipVertex.h"

namespace LV3
{

    // Un triangle (convexe, 3 sommets) coupé par UN plan donne au plus 4 sommets.
    // Garantie mathématique, pas une estimation : un convexe coupé par un plan gagne au plus un sommet.
    inline constexpr int32_t kMaxClipVertices = 4;

    // Retourne le nombre de sommets du polygone clippé : 0, 3 ou 4.
    // dst doit contenir kMaxClipVertices éléments.
    [[nodiscard]] int32_t ClipTriangleNear(const ClipVertex src[3],
        ClipVertex dst[kMaxClipVertices]) noexcept;

} // namespace LV3