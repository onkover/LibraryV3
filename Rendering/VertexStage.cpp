#include "pch.h"
#include "VertexStage.h"
#include "Rasterizer.h"      // MulRow

namespace LV3
{
    void TransformPositions(const Matrix44f& mvp, const Vec3f* src, size_t count, Vec4f* dst) noexcept
    {
        LV3_ASSERT(count <= ClipSpaceBuffer::Capacity());
        LV3_ASSERT(src != nullptr || count == 0);

        // Copie LOCALE, volontaire. Par reference, chaque ecriture float dans dst
        // pourrait (pour le compilateur) modifier un coefficient de mvp : il
        // rechargerait les 16 coefficients a CHAQUE sommet. Une locale dont
        // l'adresse ne s'echappe pas ne peut pas etre aliasee.
        const Matrix44f m = mvp;

        for (size_t i = 0; i < count; ++i)
            dst[i] = MulRow(m, src[i]);
    }
}