#pragma once
// ============================================================
//  Maths/Geometry/AABB3d.h — Boîte englobante alignée sur les axes
//  Convention LV3 : row-major, vecteur-ligne (v' = v·M)
// ============================================================
#include "../Vectorlib.h"
#include <algorithm>
#include <limits>

namespace LV3
{
    // Déclaration anticipée : évite d'inclure MatrixLib ici.
    template<typename T> class Matrix44;
    using Matrix44f = Matrix44<float>;

    struct AABB3d
    {
        static constexpr float kInf = std::numeric_limits<float>::max();

        Vec3f min{ kInf,  kInf,  kInf };
        Vec3f max{ -kInf, -kInf, -kInf };

        constexpr AABB3d() noexcept = default;
        constexpr AABB3d(const Vec3f& mn, const Vec3f& mx) noexcept : min(mn), max(mx) {}

        // --- État ------------------------------------------------
        LV3_FORCEINLINE void Reset() noexcept
        {
            min = { kInf,  kInf,  kInf };
            max = { -kInf, -kInf, -kInf };
        }
        LV3_FORCEINLINE bool  IsValid() const noexcept { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
        LV3_FORCEINLINE Vec3f Center()  const noexcept { return (min + max) * 0.5f; }
        LV3_FORCEINLINE Vec3f Extent()  const noexcept { return (max - min) * 0.5f; }   // demi-dimensions
        LV3_FORCEINLINE Vec3f Size()    const noexcept { return  max - min; }
        LV3_FORCEINLINE float Radius()  const noexcept { return Extent().length(); }    // sphère englobante

        // --- Construction ---------------------------------------
        LV3_FORCEINLINE void Expand(const Vec3f& p) noexcept
        {
            if (p.x < min.x) min.x = p.x;
            if (p.y < min.y) min.y = p.y;
            if (p.z < min.z) min.z = p.z;
            if (p.x > max.x) max.x = p.x;
            if (p.y > max.y) max.y = p.y;
            if (p.z > max.z) max.z = p.z;
        }
        LV3_FORCEINLINE void Expand(const AABB3d& b) noexcept
        {
            if (!b.IsValid()) return;
            Expand(b.min);
            Expand(b.max);
        }
        LV3_FORCEINLINE void Grow(float margin) noexcept          // marge de sécurité
        {
            min -= Vec3f(margin); max += Vec3f(margin);
        }

        // --- Requêtes -------------------------------------------
        LV3_FORCEINLINE bool Contains(const Vec3f& p) const noexcept
        {
            return p.x >= min.x && p.x <= max.x
                && p.y >= min.y && p.y <= max.y
                && p.z >= min.z && p.z <= max.z;
        }
        LV3_FORCEINLINE bool Intersects(const AABB3d& b) const noexcept
        {
            return max.x >= b.min.x && min.x <= b.max.x
                && max.y >= b.min.y && min.y <= b.max.y
                && max.z >= b.min.z && min.z <= b.max.z;
        }
        LV3_FORCEINLINE AABB3d Union(const AABB3d& b) const noexcept
        {
            return { Vec3f(std::min(min.x, b.min.x), std::min(min.y, b.min.y), std::min(min.z, b.min.z)),
                     Vec3f(std::max(max.x, b.max.x), std::max(max.y, b.max.y), std::max(max.z, b.max.z)) };
        }

        // ========================================================
        //  p-vertex / n-vertex — le cœur du culling par frustum
        //
        //  p-vertex (positive / far vertex) : le coin le plus AVANCÉ
        //      dans la direction de n. S'il est derrière le plan,
        //      les 8 coins le sont  ->  Outside immédiat.
        //
        //  n-vertex (negative / near vertex) : le coin le plus RECULÉ.
        //      S'il est devant tous les plans  ->  Inside complet.
        //
        //  Coût : 1 à 2 produits scalaires par plan, jamais 8.
        // ========================================================
        LV3_FORCEINLINE Vec3f PVertex(const Vec3f& n) const noexcept
        {
            return { n.x >= 0.0f ? max.x : min.x,
                     n.y >= 0.0f ? max.y : min.y,
                     n.z >= 0.0f ? max.z : min.z };
        }
        LV3_FORCEINLINE Vec3f NVertex(const Vec3f& n) const noexcept
        {
            return { n.x >= 0.0f ? min.x : max.x,
                     n.y >= 0.0f ? min.y : max.y,
                     n.z >= 0.0f ? min.z : max.z };
        }

        // --- Debug / cascades d'ombres --------------------------
        // i sur 3 bits : bit0 = X, bit1 = Y, bit2 = Z  (0 = min, 1 = max)
        LV3_FORCEINLINE Vec3f Corner(int i) const noexcept
        {
            return { (i & 1) ? max.x : min.x,
                     (i & 2) ? max.y : min.y,
                     (i & 4) ? max.z : min.z };
        }

        // Transforme l'AABB locale en AABB monde (méthode centre/extent
        // d'Arvo : 9 mult au lieu de 8 points transformés).
        // Défini dans AABB3d.inl pour ne pas tirer MatrixLib dans ce header.
       // [[nodiscard]] AABB3d Transformed(const Matrix44f& m) const noexcept;
        
       // ── Transformation locale -> monde ────────────────────
        // Methode centre/extent d'Arvo : 9 multiplications au lieu de
        // 8 points transformes puis re-englobes.
        // Vecteur-ligne : v'[j] = sum_i v[i]*m[i][j] + m[3][j]
        [[nodiscard]] AABB3d Transformed(const Matrix44f& m) const noexcept
        {
            if (!IsValid()) return AABB3d{};

            const Vec3f c = Center();
            const Vec3f e = Extent();

            Vec3f nc, ne;
            for (int j = 0; j < 3; ++j)
            {
                nc[j] = c.x * m[0][j] + c.y * m[1][j] + c.z * m[2][j] + m[3][j];
                ne[j] = e.x * std::fabs(m[0][j])
                    + e.y * std::fabs(m[1][j])
                    + e.z * std::fabs(m[2][j]);
            }
            return { nc - ne, nc + ne };
        }
    };
}