#pragma once
#define MATRIX_LIB
// ============================================================
//  Maths/MatrixLib.h — Matrices 4x4 templatées du moteur LibraryV3
//  Convention : row-major (x[row][col]), vecteur-ligne (v' = v·M),
//               composition S·R·T, translation en LIGNE 3,
//               right-handed, NDC z ∈ [0,1], reverse-Z
//  Standardisée Leçon 02 : scoping LV3, code mort retiré,
//  noexcept, ajout des matrices caméra (LookAt/Perspective/Ortho).
//  Dépend de Vectorlib.h (LV3::Vec3<T> et ses méthodes héritées
//  dotProduct / crossProduct / Normalized).
// ============================================================

#include "../Core/Compiler.h"
#include "../Core/config.h"     // LV3::TO_DEGRE, LV3::EPSILON_FLOAT
#include "Vectorlib.h"
#include <cmath>
#include <iostream>
#include <iomanip>
//#include <cstdint>

namespace LV3
{

    template<typename T>
    class Matrix44
    {
    public:
        // Stockage row-major : x[row][col]. Identité par défaut.
        T x[4][4] = { {1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1} };

        constexpr Matrix44() noexcept = default;

        constexpr Matrix44(T a, T b, T c, T d, T e, T f, T g, T h,
            T i, T j, T k, T l, T m, T n, T o, T p) noexcept
        {
            x[0][0] = a; x[0][1] = b; x[0][2] = c; x[0][3] = d;
            x[1][0] = e; x[1][1] = f; x[1][2] = g; x[1][3] = h;
            x[2][0] = i; x[2][1] = j; x[2][2] = k; x[2][3] = l;
            x[3][0] = m; x[3][1] = n; x[3][2] = o; x[3][3] = p;
        }

        // Accès ligne : m[row][col]
        LV3_FORCEINLINE constexpr const T* operator[](int i) const noexcept { return x[i]; }
        LV3_FORCEINLINE constexpr T* operator[](int i)       noexcept { return x[i]; }

        // Pointeur brut (16 floats contigus, row-major)
        LV3_FORCEINLINE const T* data() const noexcept { return &x[0][0]; }
        LV3_FORCEINLINE T* data()       noexcept { return &x[0][0]; }

        static constexpr Matrix44 Identity() noexcept { return Matrix44(); }

        // --- Produit matriciel (optimisé __restrict) ---
        Matrix44 operator*(const Matrix44& v) const noexcept
        {
            Matrix44 tmp;
            multiply(*this, v, tmp);
            return tmp;
        }

        static void multiply(const Matrix44& a, const Matrix44& b, Matrix44& c) noexcept
        {
            const T* __restrict ap = &a.x[0][0];
            const T* __restrict bp = &b.x[0][0];
            T* __restrict cp = &c.x[0][0];

            T a0, a1, a2, a3;
            a0 = ap[0]; a1 = ap[1]; a2 = ap[2]; a3 = ap[3];
            cp[0] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
            cp[1] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
            cp[2] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
            cp[3] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

            a0 = ap[4]; a1 = ap[5]; a2 = ap[6]; a3 = ap[7];
            cp[4] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
            cp[5] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
            cp[6] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
            cp[7] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

            a0 = ap[8]; a1 = ap[9]; a2 = ap[10]; a3 = ap[11];
            cp[8] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
            cp[9] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
            cp[10] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
            cp[11] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

            a0 = ap[12]; a1 = ap[13]; a2 = ap[14]; a3 = ap[15];
            cp[12] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
            cp[13] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
            cp[14] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
            cp[15] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];
        }

        // --- Transposition ---
        Matrix44 transposed() const noexcept
        {
            return Matrix44(x[0][0], x[1][0], x[2][0], x[3][0],
                x[0][1], x[1][1], x[2][1], x[3][1],
                x[0][2], x[1][2], x[2][2], x[3][2],
                x[0][3], x[1][3], x[2][3], x[3][3]);
        }
        Matrix44& transpose() noexcept { *this = transposed(); return *this; }

        // --- Transformation d'un POINT (w=1) : translation incluse, division homogène ---
        template<typename S>
        void multVecMatrix(const Vec3<S>& src, Vec3<S>& dst) const noexcept
        {
            S a = src[0] * x[0][0] + src[1] * x[1][0] + src[2] * x[2][0] + x[3][0];
            S b = src[0] * x[0][1] + src[1] * x[1][1] + src[2] * x[2][1] + x[3][1];
            S c = src[0] * x[0][2] + src[1] * x[1][2] + src[2] * x[2][2] + x[3][2];
            S w = src[0] * x[0][3] + src[1] * x[1][3] + src[2] * x[2][3] + x[3][3];

            S invW = (w != S(0)) ? S(1) / w : S(1);   // garde anti division par zéro
            dst.x = a * invW;
            dst.y = b * invW;
            dst.z = c * invW;
        }

        // --- Transformation d'une DIRECTION / NORMALE (w=0) : bloc 3x3 seul, pas de translation ---
        template<typename S>
        void multDirMatrix(const Vec3<S>& src, Vec3<S>& dst) const noexcept
        {
            dst.x = src[0] * x[0][0] + src[1] * x[1][0] + src[2] * x[2][0];
            dst.y = src[0] * x[0][1] + src[1] * x[1][1] + src[2] * x[2][1];
            dst.z = src[0] * x[0][2] + src[1] * x[1][2] + src[2] * x[2][2];
        }

        // --- Inverse (Gauss-Jordan). Renvoie l'identité si singulière. ---
        Matrix44 inverse() const noexcept
        {
            int i, j, k;
            Matrix44 s;            // identité (devient l'inverse)
            Matrix44 t(*this);     // copie de travail

            for (i = 0; i < 3; i++) {
                int pivot = i;
                T pivotsize = t[i][i];
                if (pivotsize < 0) pivotsize = -pivotsize;

                for (j = i + 1; j < 4; j++) {
                    T tmp = t[j][i];
                    if (tmp < 0) tmp = -tmp;
                    if (tmp > pivotsize) { pivot = j; pivotsize = tmp; }
                }
                if (pivotsize == 0) return Matrix44(); // singulière

                if (pivot != i) {
                    for (j = 0; j < 4; j++) {
                        T tmp;
                        tmp = t[i][j]; t[i][j] = t[pivot][j]; t[pivot][j] = tmp;
                        tmp = s[i][j]; s[i][j] = s[pivot][j]; s[pivot][j] = tmp;
                    }
                }
                for (j = i + 1; j < 4; j++) {
                    T factor = t[j][i] / t[i][i];
                    for (k = 0; k < 4; k++) { t[j][k] -= factor * t[i][k]; s[j][k] -= factor * s[i][k]; }
                }
            }
            for (i = 3; i >= 0; --i) {
                T factor;
                if ((factor = t[i][i]) == 0) return Matrix44(); // singulière
                for (j = 0; j < 4; j++) { t[i][j] /= factor; s[i][j] /= factor; }
                for (j = 0; j < i; j++) {
                    factor = t[j][i];
                    for (k = 0; k < 4; k++) { t[j][k] -= factor * t[i][k]; s[j][k] -= factor * s[i][k]; }
                }
            }
            return s;
        }
        Matrix44& invert() noexcept { *this = inverse(); return *this; }

        // ========================================================
        //  Constructeurs de transformation (instance, post-multiplication)
        //  Chaînés depuis l'identité : M.scale(s).rotateX(a).translate(t) = S·R·T
        // ========================================================
        template<typename S>
        Matrix44& translate(const Vec3<S>& v) noexcept
        {
            Matrix44 tm;                 // identité
            tm.x[3][0] = v.x; tm.x[3][1] = v.y; tm.x[3][2] = v.z;
            *this = *this * tm;
            return *this;
        }
        template<typename S>
        Matrix44& scale(const Vec3<S>& v) noexcept
        {
            Matrix44 sm;
            sm.x[0][0] = v.x; sm.x[1][1] = v.y; sm.x[2][2] = v.z;
            *this = *this * sm;
            return *this;
        }
        Matrix44& rotateX(T rad) noexcept   // RH : +Y -> +Z
        {
            T c = std::cos(rad), s = std::sin(rad);
            Matrix44 rm;
            rm.x[1][1] = c; rm.x[1][2] = s; rm.x[2][1] = -s; rm.x[2][2] = c;
            *this = *this * rm;
            return *this;
        }
        Matrix44& rotateY(T rad) noexcept   // RH : +Z -> +X
        {
            T c = std::cos(rad), s = std::sin(rad);
            Matrix44 rm;
            rm.x[0][0] = c; rm.x[0][2] = -s; rm.x[2][0] = s; rm.x[2][2] = c;
            *this = *this * rm;
            return *this;
        }
        Matrix44& rotateZ(T rad) noexcept   // RH : +X -> +Y
        {
            T c = std::cos(rad), s = std::sin(rad);
            Matrix44 rm;
            rm.x[0][0] = c; rm.x[0][1] = s; rm.x[1][0] = -s; rm.x[1][1] = c;
            *this = *this * rm;
            return *this;
        }

        // ========================================================
        //  Matrices caméra (statiques) — row-major, right-handed
        // ========================================================
        // Vue : amène le monde dans le repère caméra. RH -> regarde vers -Z.
        //static Matrix44 LookAt(const Vec3<T>& eye, const Vec3<T>& target, const Vec3<T>& up) noexcept
        //{
        //    Vec3<T> f = (target - eye).Normalized();     // forward (vers la cible)
        //    Vec3<T> r = f.crossProduct(up).Normalized(); // right
        //    Vec3<T> u = r.crossProduct(f);               // up ré-orthogonalisé

        //    Matrix44 m; // identité
        //    m.x[0][0] = r.x; m.x[0][1] = u.x; m.x[0][2] = -f.x;
        //    m.x[1][0] = r.y; m.x[1][1] = u.y; m.x[1][2] = -f.y;
        //    m.x[2][0] = r.z; m.x[2][1] = u.z; m.x[2][2] = -f.z;
        //    m.x[3][0] = -r.dotProduct(eye);
        //    m.x[3][1] = -u.dotProduct(eye);
        //    m.x[3][2] = f.dotProduct(eye);
        //    return m;
        //}

        // Projection perspective. fovRad = champ de vision VERTICAL en radians.
        //static Matrix44 Perspective(T fovRad, T aspect, T nearZ, T farZ) noexcept
        //{
        //    T th = std::tan(fovRad * T(0.5));

        //    Matrix44 m;                                  // identité...
        //    for (int i = 0; i < 4; ++i)
        //        for (int j = 0; j < 4; ++j) m.x[i][j] = T(0);   // ...remise à ZÉRO obligatoire

        //    m.x[0][0] = T(1) / (aspect * th);
        //    m.x[1][1] = T(1) / th;
        //    m.x[2][2] = -(farZ + nearZ) / (farZ - nearZ);
        //    m.x[2][3] = -T(1);                            // recopie -z dans w
        //    m.x[3][2] = -(T(2) * farZ * nearZ) / (farZ - nearZ);
        //    return m;                                     // m.x[3][3] = 0 : matrice projective
        //}

        //// Projection orthographique.
        //static Matrix44 Orthographic(T l, T r, T b, T t, T n, T f) noexcept
        //{
        //    Matrix44 m; // identité
        //    m.x[0][0] = T(2) / (r - l);
        //    m.x[1][1] = T(2) / (t - b);
        //    m.x[2][2] = -T(2) / (f - n);
        //    m.x[3][0] = -(r + l) / (r - l);
        //    m.x[3][1] = -(t + b) / (t - b);
        //    m.x[3][2] = -(f + n) / (f - n);
        //    return m;
        //}

        static constexpr Matrix44 Translation(const Vec3<T>& t) noexcept {
            Matrix44 m;                                   // identité
            m.x[3][0] = t.x; m.x[3][1] = t.y; m.x[3][2] = t.z;   // translation en LIGNE 3
            return m;
        }

        static constexpr Matrix44 Scale(const Vec3<T>& s) noexcept {
            Matrix44 m;
            m.x[0][0] = s.x; m.x[1][1] = s.y; m.x[2][2] = s.z;
            return m;
        }
        // ========================================================
        //  Extraction d'angles d'Euler (en degrés).
        //  Heuristique sensible à la convention — à valider sur tes cas.
        //  (Anciennement getAngle<S>() ; généralisé en Vec3<T>.)
        // ========================================================
        Vec3<T> getEulerAngles() const noexcept
        {
            const T toDeg = static_cast<T>(LV3::TO_DEGRE);
            const T eps = static_cast<T>(LV3::EPSILON_FLOAT);
            Vec3<T> a;

            a.y = -std::asin(x[0][2]) * toDeg;
            if (a.y > 0 && x[2][2] < 0) a.y = T(180) - a.y;
            else if (a.y < 0 && x[2][2] < 0) a.y = -(a.y - T(180));
            else if (a.y < 0 && x[2][2] > 0) a.y = T(360) + a.y;

            if (x[0][0] > -eps && x[0][0] < eps)
                a.x = std::atan2(x[0][1], x[1][1]) * toDeg;
            else
                a.x = std::atan2(-x[2][1], x[2][2]) * toDeg;
            if (a.x < 0) a.x = T(360) + a.x;

            a.z = std::atan2(-x[1][0], x[0][0]) * toDeg;
            if (a.z < 0) a.z = T(360) + a.z;
            return a;
        }

        friend std::ostream& operator<<(std::ostream& s, const Matrix44& m)
        {
            std::ios_base::fmtflags old = s.flags();
            int w = 12;
            s.precision(5);
            s.setf(std::ios_base::fixed);
            s << "[" << std::setw(w) << m[0][0] << " " << std::setw(w) << m[0][1] << " " << std::setw(w) << m[0][2] << " " << std::setw(w) << m[0][3] << "\n"
                << " " << std::setw(w) << m[1][0] << " " << std::setw(w) << m[1][1] << " " << std::setw(w) << m[1][2] << " " << std::setw(w) << m[1][3] << "\n"
                << " " << std::setw(w) << m[2][0] << " " << std::setw(w) << m[2][1] << " " << std::setw(w) << m[2][2] << " " << std::setw(w) << m[2][3] << "\n"
                << " " << std::setw(w) << m[3][0] << " " << std::setw(w) << m[3][1] << " " << std::setw(w) << m[3][2] << " " << std::setw(w) << m[3][3] << "]";
            s.flags(old);
            return s;
        }


        // Inverse d'une transformation RIGIDE (rotation + translation, sans scale).
        // Row-major / vecteur-ligne : R = bloc 3x3, t = LIGNE 3.
        //   M   = [ R  | 0 ]        M⁻¹ = [  Rᵀ    | 0 ]
        //         [ t  | 1 ]              [ -t·Rᵀ  | 1 ]
        Matrix44 inverseRigid() const noexcept
        {
            Matrix44 r;
            r.x[0][0] = x[0][0]; r.x[0][1] = x[1][0]; r.x[0][2] = x[2][0];   // transposée 3x3
            r.x[1][0] = x[0][1]; r.x[1][1] = x[1][1]; r.x[1][2] = x[2][1];
            r.x[2][0] = x[0][2]; r.x[2][1] = x[1][2]; r.x[2][2] = x[2][2];

            r.x[3][0] = -(x[3][0] * r.x[0][0] + x[3][1] * r.x[1][0] + x[3][2] * r.x[2][0]);
            r.x[3][1] = -(x[3][0] * r.x[0][1] + x[3][1] * r.x[1][1] + x[3][2] * r.x[2][1]);
            r.x[3][2] = -(x[3][0] * r.x[0][2] + x[3][1] * r.x[1][2] + x[3][2] * r.x[2][2]);

            r.x[0][3] = r.x[1][3] = r.x[2][3] = T(0); r.x[3][3] = T(1);
            return r;
        }
    };

    using Matrix44f = Matrix44<float>;
    using Matrix44d = Matrix44<double>;

} // namespace LV3
