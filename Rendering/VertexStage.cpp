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

    //*******************************************************************************

    void ProjectPositions(const Matrix44f& mvp, const Viewport& vp, const Vec3f* src, size_t count, RasterVertex* dst) noexcept
    {
        LV3_ASSERT(count <= RasterSpaceBuffer::Capacity());
        LV3_ASSERT(src != nullptr || count == 0);

        // Copies LOCALES volontaire, meme raison que TransformPositions : les ecritures
        // dans dst ne peuvent pas aliaser une locale dont l'adresse ne s'echappe pas.
        const Matrix44f m = mvp;
        const Viewport  v = vp;

        for (size_t i = 0; i < count; ++i)
        {
            const Vec4f c = MulRow(m, src[i]);
            LV3_ASSERT(c.w > 0.0f);   // contrat Inside, PROUVE sur donnees reelles en RelWithAsserts
            dst[i] = ProjectClip(c, v);
        }
    }
}