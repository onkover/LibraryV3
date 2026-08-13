#include "pch.h"
#include "Clipper.h"

namespace LV3
{
    // CLIPPING → 0, 1 ou 2 triangles
    // Selon la technique "Blinn - Newell" homogeneous clipping, un seul plan(near),
    // et via un parcours "Sutherland - Hodgman"
    int32_t ClipTriangleNear(const ClipVertex src[3],
        ClipVertex dst[kMaxClipVertices]) noexcept
    {
        const float d[3] = { NearDistance(src[0]),
                             NearDistance(src[1]),
                             NearDistance(src[2]) };

        // ── Rejet trivial (idée de Cyrus-Beck) ──
        if (d[0] < 0.0f && d[1] < 0.0f && d[2] < 0.0f)
            return 0;

        // ── Acceptation triviale ──
        if (d[0] >= 0.0f && d[1] >= 0.0f && d[2] >= 0.0f)
        {
            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
            return 3;
        }

        // ── Sutherland-Hodgman, un plan, parcours ORDONNÉ des arêtes ──
        int32_t count = 0;

        for (int32_t i = 0; i < 3; ++i)
        {
            const int32_t j = (i + 1) % 3;
            const float   di = d[i];
            const float   dj = d[j];

            // 1. le sommet AVANT l'intersection de son arête sortante.
            //    Inverser ces deux blocs produit un polygone auto-intersectant.
            if (di >= 0.0f)
                dst[count++] = src[i];

            // 2. changement de signe → l'arête traverse le plan
            if ((di >= 0.0f) != (dj >= 0.0f))
            {
                // di - dj est STRICTEMENT positif dans cette branche :
                // l'un des deux est >= 0, l'autre < 0. Aucun epsilon requis, aucune division par zéro possible.
                const float t = di / (di - dj);                     // Le t est exact.NearDistance est affine dans les coordonnées de clip, donc le paramètre d'intersection est une expression rationnelle exacte des distances — pas une approximation itérative. C'est tout l'intérêt de clipper en 4D homogène (Blinn-Newell) plutôt qu'après la division.
                dst[count++] = Lerp(src[i], src[j], t);
            }
        }

        LV3_ASSERT(count == 3 || count == 4);
        return count;
    }

} // namespace LV3