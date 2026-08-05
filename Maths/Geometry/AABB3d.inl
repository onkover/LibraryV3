#pragma once
#include "AABB3d.h"
#include "../MatrixLib.h"

namespace LV3
{
    /// <summary>
    /// Transforms the AABB by the given matrix.
    /// </summary>
    /// <param name="m"></param>
    /// <returns></returns>
    LV3_FORCEINLINE AABB3d AABB3d::Transformed(const Matrix44f& m) const noexcept
    {
        if (!IsValid()) return AABB3d{};

        const Vec3f c = Center();
        const Vec3f e = Extent();

        // Vecteur-ligne : v'[j] = Σ_i v[i]·m[i][j] + m[3][j]
        Vec3f nc, ne;
        for (int j = 0; j < 3; ++j)
        {
            nc[j] = c.x * m[0][j] + c.y * m[1][j] + c.z * m[2][j] + m[3][j];
            ne[j] = e.x * std::fabs(m[0][j]) + e.y * std::fabs(m[1][j]) + e.z * std::fabs(m[2][j]);
        }
        return { nc - ne, nc + ne };
    }
}