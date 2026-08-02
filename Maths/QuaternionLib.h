#pragma once
#define QUAT_LIB
// ============================================================
//  Maths/QuaternionLib.h — Quaternions templatés du moteur LibraryV3
//  Convention : right-handed. q = r + v  (r = scalaire, v = Vec3).
//  ToMatrix44() produit une matrice ROW-MAJOR (vecteur-ligne),
//  cohérente avec MatrixLib.h.
//  Standardisée Leçon 02 : scoping LV3, bugs corrigés, code mort
//  retiré, ajout de conjugate/inverse/rotate/Identity.
//  Base : scratchapixel. Dépend de Vectorlib.h et MatrixLib.h.
// ============================================================

#include "../Core/config.h"     // LV3::TO_RADIAN
#include "Vectorlib.h"
#include "MatrixLib.h"          // Matrix44<T> (était utilisé sans être inclus)
#include <cmath>

namespace LV3
{

    template<typename T>
    class Quat
    {
    public:
        T       r{ T(1) };            // partie scalaire (w)
        Vec3<T> v{ T(0), T(0), T(0) };// partie vectorielle (x, y, z)

        constexpr Quat() noexcept = default;
        constexpr Quat(T s, T i, T j, T k) noexcept : r(s), v(i, j, k) {}
        constexpr Quat(T s, const Vec3<T>& d) noexcept : r(s), v(d) {}

        static constexpr Quat Identity() noexcept { return Quat(T(1), Vec3<T>(T(0), T(0), T(0))); }

        // --- Construction depuis angles d'Euler, ordre Y-X-Z (Yaw, Pitch, Roll) ---
        //  Angles.x = Pitch (X), Angles.y = Yaw (Y), Angles.z = Roll (Z).
        //  eulerAngles = true  -> les angles sont en DEGRÉS (conversion appliquée).
        //  eulerAngles = false -> les angles sont déjà en RADIANS.
        explicit Quat(const Vec3<T>& Angles, bool eulerAngles = false) noexcept
        {
            Vec3<T> a = Angles;                     // (corrigé : était Vec3f, cassait Quat<double>)
            if (eulerAngles) { a.x *= LV3::TO_RADIAN; a.y *= LV3::TO_RADIAN; a.z *= LV3::TO_RADIAN; }

            T ph = a.x * T(0.5), yw = a.y * T(0.5), rl = a.z * T(0.5);
            T cp = std::cos(ph), sp = std::sin(ph);
            T cy = std::cos(yw), sy = std::sin(yw);
            T cr = std::cos(rl), sr = std::sin(rl);

            r = cr * cp * cy + sr * sp * sy;
            v.x = cr * sp * cy + sr * cp * sy;
            v.y = cr * cp * sy - sr * sp * cy;
            v.z = sr * cp * cy - cr * sp * sy;
        }

        // Idem mais en mutation, angles toujours en DEGRÉS.
        Quat& SetEulerAngles(const Vec3<T>& euler) noexcept
        {
            T ph = euler.x * LV3::TO_RADIAN * T(0.5);
            T yw = euler.y * LV3::TO_RADIAN * T(0.5);
            T rl = euler.z * LV3::TO_RADIAN * T(0.5);
            T cp = std::cos(ph), sp = std::sin(ph);
            T cy = std::cos(yw), sy = std::sin(yw);
            T cr = std::cos(rl), sr = std::sin(rl);

            r = cr * cp * cy + sr * sp * sy;
            v.x = cr * sp * cy + sr * cp * sy;
            v.y = cr * cp * sy - sr * sp * cy;
            v.z = sr * cp * cy - cr * sp * sy;
            return *this;
        }

        // Axe (normalisé en interne) + angle en RADIANS.
        Quat& SetAxisAngle(const Vec3<T>& axis, T radians) noexcept
        {
            v = axis.Normalized() * std::sin(radians * T(0.5));
            r = std::cos(radians * T(0.5));
            return *this;
        }

        // Empêche l'accumulation d'erreurs après de nombreuses multiplications.
        Quat& normalize() noexcept
        {
            T mag = std::sqrt(r * r + v.x * v.x + v.y * v.y + v.z * v.z);
            if (mag > T(0)) { r /= mag; v /= mag; }
            return *this;
        }

        // Conjugué : (r, -v). Pour un quaternion unitaire, c'est l'inverse.
        constexpr Quat conjugate() const noexcept { return Quat(r, -v); }

        // Inverse général : conjugué / |q|²  (pas de sqrt, donc constexpr).
        constexpr Quat inverse() const noexcept
        {
            T n2 = r * r + v.dotProduct(v);
            if (n2 <= T(0)) return Identity();
            T inv = T(1) / n2;
            return Quat(r * inv, -v * inv);
        }

        // Rotation directe d'un vecteur (formule optimisée, right-handed) :
        //  p' = p + 2r(v×p) + 2(v×(v×p))
        constexpr Vec3<T> rotate(const Vec3<T>& p) const noexcept
        {
            Vec3<T> t = v.crossProduct(p) * T(2);
            return p + t * r + v.crossProduct(t);
        }

        // --- Conversion en matrice ROW-MAJOR (vecteur-ligne), cohérente MatrixLib ---
        constexpr Matrix44<T> ToMatrix44() const noexcept
        {
            return Matrix44<T>(
                1 - 2 * (v.y * v.y + v.z * v.z), 2 * (v.x * v.y + v.z * r), 2 * (v.z * v.x - v.y * r), 0,
                2 * (v.x * v.y - v.z * r), 1 - 2 * (v.z * v.z + v.x * v.x), 2 * (v.y * v.z + v.x * r), 0,
                2 * (v.z * v.x + v.y * r), 2 * (v.y * v.z - v.x * r), 1 - 2 * (v.y * v.y + v.x * v.x), 0,
                0, 0, 0, 1);
        }

        // ============================================================
        //  Oriente le repère local de sorte que son axe AVANT (-Z en main
        //  droite) pointe vers 'dir', et que son axe UP soit aussi proche
        //  que possible de 'worldUp'.
        //
        //  Équivalent LV3 de Quaternion.LookRotation (Unity) /
        //  glm::quatLookAtRH.
        //
        //  dir     : direction de VISÉE (n'a pas besoin d'être normalisée)
        //  worldUp : référence verticale, arbitraire (n'a pas besoin d'être
        //            perpendiculaire à dir)
        // ============================================================
        static Quat LookRotation(const Vec3<T>& dir, const Vec3<T>& worldUp = Vec3<T>::Up()) noexcept
        {
            // --- 1. Garde-fou : direction nulle ---
            if (dir.norm() <= static_cast<T>(LV3::EPSILON_FLOAT))
                return Identity();

            const Vec3<T> f = dir.Normalized();          // direction de visée
                                                         // en main droite vaut -f
														 // en main gauche vaut +f

            
            // --- 2. Base orthonormée, main droite ---
            //     right = f x up ;  up' = right x f
            Vec3<T> up = worldUp;
            Vec3<T> r = f.crossProduct(up);

            // Cas dégénéré : dir colinéaire à worldUp (regard à la verticale).
            // C'est LE bug de ton ancien lookAT(), contourné par un EPSILON dans setOrigin().
            if (r.norm() <= static_cast<T>(LV3::EPSILON_FLOAT))
            {
                // On choisit une verticale de secours non colinéaire à f.
                up = (std::abs(f.z) < static_cast<T>(0.9))
                    ? Vec3<T>(T(0), T(0), T(1))
                    : Vec3<T>(T(0), T(1), T(0));
                r = f.crossProduct(up);
            }
            r = r.Normalized();
            const Vec3<T> u = r.crossProduct(f);         // déjà unitaire (r ⟂ f, tous deux unitaires)

            // --- 3. Matrice de rotation, ROW-MAJOR / vecteur-ligne ---
            //     Les LIGNES sont les axes locaux exprimés dans le monde.
            //     Ligne 0 = +X local (right)
            //     Ligne 1 = +Y local (up)
            //     Ligne 2 = +Z local = -avant   (main droite)
            const T m00 = r.x, m01 = r.y, m02 = r.z;
            const T m10 = u.x, m11 = u.y, m12 = u.z;
            const T m20 = -f.x, m21 = -f.y, m22 = -f.z;

            // --- 4. Matrice -> quaternion (Shepperd) ---
            //     Cohérent avec ToMatrix44() : m01 = 2(xy + zw), m10 = 2(xy - zw), etc.
            // Pourquoi Shepperd et pas la formule directe.
            // La formule w = sqrt(trace + 1) / 2 puis x = (m12 - m21) / (4w) explose quand w → 0, c'est-à-dire pour une rotation de 180°. 
            // Les quatre branches ci-dessous choisissent celle des quatre composantes qui est la plus grande, donc le dénominateur le plus sûr. 
            // C'est le standard, y compris chez Unity et GLM.
            Quat q;
            const T trace = m00 + m11 + m22;

            if (trace > T(0))
            {
                const T s = std::sqrt(trace + T(1)) * T(2);            // s = 4w
                q.r = T(0.25) * s;
                q.v.x = (m12 - m21) / s;
                q.v.y = (m20 - m02) / s;
                q.v.z = (m01 - m10) / s;
            }
            else if (m00 > m11 && m00 > m22)
            {
                const T s = std::sqrt(T(1) + m00 - m11 - m22) * T(2);  // s = 4x
                q.r = (m12 - m21) / s;
                q.v.x = T(0.25) * s;
                q.v.y = (m01 + m10) / s;
                q.v.z = (m20 + m02) / s;
            }
            else if (m11 > m22)
            {
                const T s = std::sqrt(T(1) + m11 - m00 - m22) * T(2);  // s = 4y
                q.r = (m20 - m02) / s;
                q.v.x = (m01 + m10) / s;
                q.v.y = T(0.25) * s;
                q.v.z = (m12 + m21) / s;
            }
            else
            {
                const T s = std::sqrt(T(1) + m22 - m00 - m11) * T(2);  // s = 4z
                q.r = (m01 - m10) / s;
                q.v.x = (m20 + m02) / s;
                q.v.y = (m12 + m21) / s;
                q.v.z = T(0.25) * s;
            }

            return q.normalize();
        }

        // Confort : oriente depuis un point vers un autre.
        // (Remplace l'ancien couple FROM/TO de la classe Camera.)
        static Quat LookAt(const Vec3<T>& eye, const Vec3<T>& target, const Vec3<T>& worldUp = Vec3<T>::Up()) noexcept
        {
            return LookRotation(target - eye, worldUp);
        }

    };

    using Quatf = Quat<float>;
    using Quatd = Quat<double>;

    // --- Produit de Hamilton : q1 * q2 (right-handed) ---
    template<typename T>
    constexpr inline Quat<T> operator*(const Quat<T>& q1, const Quat<T>& q2) noexcept
    {
        return Quat<T>(
            q1.r * q2.r - q1.v.dotProduct(q2.v),
            q1.r * q2.v + q1.v * q2.r + q1.v.crossProduct(q2.v));
    }

    // Produit scalaire de deux quaternions.
    template<typename T>
    constexpr inline T Dot(const Quat<T>& q1, const Quat<T>& q2) noexcept
    {
        return q1.r * q2.r + q1.v.dotProduct(q2.v);
    }

    template<typename T>
    constexpr inline Quat<T> operator+(const Quat<T>& q1, const Quat<T>& q2) noexcept
    {
        return Quat<T>(q1.r + q2.r, q1.v + q2.v);
    }

    template<typename T>
    constexpr inline Quat<T> operator*(T s, const Quat<T>& q) noexcept { return Quat<T>(s * q.r, s * q.v); }

    template<typename T>
    constexpr inline Quat<T> operator*(const Quat<T>& q, T s) noexcept { return Quat<T>(q.r * s, q.v * s); }

    // --- Interpolation sphérique (SLERP) avec repli LERP et gestion anti-podale ---
    template<typename T>
    inline Quat<T> Slerp(const Quat<T>& q1, const Quat<T>& q2, T t) noexcept
    {
        T cosOmega = Dot(q1, q2);

        // Chemin le plus court : si l'angle > 90°, on inverse q2 (même rotation).
        Quat<T> q2b = q2;
        if (cosOmega < T(0)) { q2b.r = -q2.r; q2b.v = T(-1) * q2.v; cosOmega = -cosOmega; }

        T scale1, scale2;
        const T LERP_THRESHOLD = T(0.9995);
        if (cosOmega > LERP_THRESHOLD) {
            // Quasi colinéaires : LERP (sin(omega) ~ 0 -> division instable).
            scale1 = T(1) - t;
            scale2 = t;
        }
        else {
            T omega = std::acos(cosOmega);
            T sinOmega = std::sin(omega);
            scale1 = std::sin((T(1) - t) * omega) / sinOmega;
            scale2 = std::sin(t * omega) / sinOmega;
        }

        Quat<T> result = scale1 * q1 + scale2 * q2b;
        if (cosOmega > LERP_THRESHOLD) result.normalize(); // LERP ne garantit pas l'unité
        return result;
    }

} // namespace LV3
