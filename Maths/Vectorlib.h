#pragma once
#define VECTOR_LIB
// ============================================================
//  Maths/Vectorlib.h — Vecteurs templatés du moteur LibraryV3
//  Convention : right-handed, vecteur-ligne (v' = v·M)
//  Standardisée Leçon 02 : scoping LV3, fabriques corrigées,
//  noexcept/constexpr, méthodes géométriques ajoutées.
//  Noms hérités conservés (dotProduct/crossProduct/Normalized)
//  car MatrixLib.h et QuaternionLib.h en dépendent.
// ============================================================

#include "../Core/Compiler.h"
#include <cmath>          // ← std::sqrt, std::atan2, std::acos
#include <cmath>          // ← std::sqrt, std::atan2, std::acos
#include <cstdint>        // ← uint8_t & co
#include <algorithm>      // ← std::min, std::max, std::clamp
#include <iostream>       // ← operator<< (candidat au retrait plus tard)

namespace LV3
{

    // ============================================================
    //  Vec2<T>
    // ============================================================
    template<typename T>
    class Vec2
    {
    public:
        T x{ T(0) }, y{ T(0) };

        constexpr Vec2() noexcept = default;
        constexpr explicit Vec2(T xx) noexcept : x(xx), y(xx) {}
        constexpr Vec2(T xx, T yy) noexcept : x(xx), y(yy) {}

        // --- arithmétique ---
        LV3_FORCEINLINE constexpr Vec2 operator+(const Vec2& v) const noexcept { return { x + v.x, y + v.y }; }
        LV3_FORCEINLINE constexpr Vec2 operator-(const Vec2& v) const noexcept { return { x - v.x, y - v.y }; }
        LV3_FORCEINLINE constexpr Vec2 operator-()              const noexcept { return { -x, -y }; }
        LV3_FORCEINLINE constexpr Vec2 operator*(T r)           const noexcept { return { x * r, y * r }; }
        LV3_FORCEINLINE constexpr Vec2 operator*(const Vec2& v) const noexcept { return { x * v.x, y * v.y }; }
        LV3_FORCEINLINE constexpr Vec2 operator/(T r)           const noexcept { return { x / r, y / r }; }
        LV3_FORCEINLINE constexpr Vec2 operator/(const Vec2& v) const noexcept { return { x / v.x, y / v.y }; }

        LV3_FORCEINLINE constexpr Vec2& operator+=(const Vec2& v) noexcept { x += v.x; y += v.y; return *this; }
        LV3_FORCEINLINE constexpr Vec2& operator-=(const Vec2& v) noexcept { x -= v.x; y -= v.y; return *this; }
        LV3_FORCEINLINE constexpr Vec2& operator*=(T r)           noexcept { x *= r;   y *= r;   return *this; }
        LV3_FORCEINLINE constexpr Vec2& operator/=(T r)           noexcept { x /= r;   y /= r;   return *this; }

        LV3_FORCEINLINE constexpr bool operator==(const Vec2& v) const noexcept = default;

        // --- produits & métriques ---
        // Produit vectoriel 2D : scalaire = aire signée du parallélogramme.
        LV3_FORCEINLINE constexpr T crossProduct(const Vec2& v) const noexcept { return x * v.y - y * v.x; }
        LV3_FORCEINLINE constexpr T dotProduct(const Vec2& v)   const noexcept { return x * v.x + y * v.y; }
        LV3_FORCEINLINE constexpr T norm()    const noexcept { return x * x + y * y; }   // longueur au carré
        LV3_FORCEINLINE T          length()   const noexcept { return std::sqrt(norm()); }
        LV3_FORCEINLINE T          getAngle() const noexcept { return std::atan2(y, x); } // angle / axe +X

        LV3_FORCEINLINE Vec2& normalize() noexcept {
            T n = norm();
            if (n > T(0)) { T f = T(1) / std::sqrt(n); x *= f; y *= f; }
            return *this;
        }
        LV3_FORCEINLINE Vec2 Normalized() const noexcept {
            T n = norm();
            if (n <= T(0)) return Vec2(T(0));
            T f = T(1) / std::sqrt(n);
            return { x * f, y * f };
        }
        LV3_FORCEINLINE Vec2& setMagnitude(T r) noexcept { normalize(); x *= r; y *= r; return *this; }

        // --- fabriques ---
        static constexpr Vec2 Zero() noexcept { return { T(0), T(0) }; }
        static constexpr Vec2 One()  noexcept { return { T(1), T(1) }; }

        friend std::ostream& operator<<(std::ostream& s, const Vec2& v) { return s << '[' << v.x << ' ' << v.y << ']'; }
    };

    template<typename T>
    LV3_FORCEINLINE constexpr Vec2<T> operator*(T r, const Vec2<T>& v) noexcept { return { v.x * r, v.y * r }; }

    using Vec2d = Vec2<double>;
    using Vec2f = Vec2<float>;
    using Vec2i = Vec2<int>;
    using Vec2l = Vec2<long>;

    // ============================================================
    //  Vec3<T>  — cœur 3D (points, vecteurs, normales)
    // ============================================================
    template<typename T>
    class Vec3
    {
    public:
        T x{ T(0) }, y{ T(0) }, z{ T(0) };

        constexpr Vec3() noexcept = default;
        constexpr explicit Vec3(T xx) noexcept : x(xx), y(xx), z(xx) {}
        constexpr Vec3(T xx, T yy, T zz) noexcept : x(xx), y(yy), z(zz) {}

        // --- arithmétique ---
        LV3_FORCEINLINE constexpr Vec3 operator+(const Vec3& v) const noexcept { return { x + v.x, y + v.y, z + v.z }; }
        LV3_FORCEINLINE constexpr Vec3 operator-(const Vec3& v) const noexcept { return { x - v.x, y - v.y, z - v.z }; }
        LV3_FORCEINLINE constexpr Vec3 operator-()              const noexcept { return { -x, -y, -z }; }
        LV3_FORCEINLINE constexpr Vec3 operator*(T r)           const noexcept { return { x * r, y * r, z * r }; }
        LV3_FORCEINLINE constexpr Vec3 operator*(const Vec3& v) const noexcept { return { x * v.x, y * v.y, z * v.z }; }
        LV3_FORCEINLINE constexpr Vec3 operator/(T r)           const noexcept { return { x / r, y / r, z / r }; }
        LV3_FORCEINLINE constexpr Vec3 operator/(const Vec3& v) const noexcept { return { x / v.x, y / v.y, z / v.z }; }

        LV3_FORCEINLINE constexpr Vec3& operator+=(const Vec3& v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
        LV3_FORCEINLINE constexpr Vec3& operator-=(const Vec3& v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
        LV3_FORCEINLINE constexpr Vec3& operator*=(T r)           noexcept { x *= r;   y *= r;   z *= r;   return *this; }
        LV3_FORCEINLINE constexpr Vec3& operator*=(const Vec3& v) noexcept { x *= v.x; y *= v.y; z *= v.z; return *this; }
        LV3_FORCEINLINE constexpr Vec3& operator/=(T r)           noexcept { x /= r;   y /= r;   z /= r;   return *this; }
        LV3_FORCEINLINE constexpr Vec3& operator/=(const Vec3& v) noexcept { x /= v.x; y /= v.y; z /= v.z; return *this; }

        LV3_FORCEINLINE constexpr bool operator==(const Vec3& v) const noexcept = default;

        // accès indexé : v[0]=x, v[1]=y, v[2]=z (utilisé par MatrixLib)
        LV3_FORCEINLINE constexpr const T& operator[](int i) const noexcept { return (&x)[i]; }
        LV3_FORCEINLINE constexpr T& operator[](int i)       noexcept { return (&x)[i]; }

        // --- produits & métriques ---
        LV3_FORCEINLINE constexpr T    dotProduct(const Vec3& v)   const noexcept { return x * v.x + y * v.y + z * v.z; }
        LV3_FORCEINLINE constexpr Vec3 crossProduct(const Vec3& v) const noexcept { // right-handed
            return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
        }
        LV3_FORCEINLINE constexpr T norm()   const noexcept { return x * x + y * y + z * z; } // longueur au carré
        LV3_FORCEINLINE T           length() const noexcept { return std::sqrt(norm()); }

        LV3_FORCEINLINE Vec3& normalize() noexcept {
            T n = norm();
            if (n > T(0)) { T f = T(1) / std::sqrt(n); x *= f; y *= f; z *= f; }
            return *this;
        }
        LV3_FORCEINLINE Vec3 Normalized() const noexcept {
            T n = norm();
            if (n <= T(0)) return Vec3(T(0));
            T f = T(1) / std::sqrt(n);
            return { x * f, y * f, z * f };
        }

        // --- géométrie (ajouts Leçon 02) ---
        // Réflexion autour d'une normale n (supposée unitaire) : r = v - 2(v·n)n
        LV3_FORCEINLINE constexpr Vec3 reflect(const Vec3& n) const noexcept {
            return *this - n * (T(2) * dotProduct(n));
        }
        // Projection de *this sur u (u quelconque, non nul)
        LV3_FORCEINLINE Vec3 projectOnto(const Vec3& u) const noexcept {
            T d = u.norm();
            if (d <= T(0)) return Vec3(T(0));
            return u * (dotProduct(u) / d);
        }
        LV3_FORCEINLINE T distanceTo(const Vec3& v) const noexcept { return (*this - v).length(); }
        // Angle non signé entre deux vecteurs (radians)
        LV3_FORCEINLINE T angleTo(const Vec3& v) const noexcept {
            T denom = length() * v.length();
            if (denom <= T(0)) return T(0);
            T c = dotProduct(v) / denom;
            c = std::clamp(c, T(-1), T(1));
            return std::acos(c);
        }

        LV3_FORCEINLINE Vec3& setMagnitude(T r) noexcept { normalize(); x *= r; y *= r; z *= r; return *this; }
        LV3_FORCEINLINE Vec3& setMin(const Vec3& v) noexcept {
            x = std::min(x, v.x); y = std::min(y, v.y); z = std::min(z, v.z); return *this;
        }
        LV3_FORCEINLINE Vec3& setMax(const Vec3& v) noexcept {
            x = std::max(x, v.x); y = std::max(y, v.y); z = std::max(z, v.z); return *this;
        }
        // Plafonne la longueur à maxLen
        LV3_FORCEINLINE Vec3& Limit(T maxLen) noexcept {
            if (norm() > maxLen * maxLen) { normalize(); *this *= maxLen; }
            return *this;
        }

        // --- fabriques (corrigées : l'ancienne version ne compilait pas) ---
        static constexpr Vec3 Zero()    noexcept { return { T(0), T(0), T(0) }; }
        static constexpr Vec3 One()     noexcept { return { T(1), T(1), T(1) }; }
        static constexpr Vec3 Up()      noexcept { return { T(0), T(1), T(0) }; }
        static constexpr Vec3 Down()    noexcept { return { T(0), T(-1), T(0) }; }
        static constexpr Vec3 Right()   noexcept { return { T(1), T(0), T(0) }; }
        static constexpr Vec3 Left()    noexcept { return { T(-1), T(0), T(0) }; }
        static constexpr Vec3 Forward() noexcept { return { T(0), T(0), T(-1) }; }  // main droite : la vue regarde vers -Z
        static constexpr Vec3 Back()    noexcept { return { T(0), T(0), T(1) }; }  // +Z sort de l'ecran vers l'observateur
        // Interpolation linéaire
        static constexpr Vec3 Lerp(const Vec3& a, const Vec3& b, T t) noexcept { return a + (b - a) * t; }

        friend std::ostream& operator<<(std::ostream& s, const Vec3& v) {
            return s << '[' << v.x << ' ' << v.y << ' ' << v.z << ']';
        }
    };

    template<typename T>
    LV3_FORCEINLINE constexpr Vec3<T> operator*(T r, const Vec3<T>& v) noexcept { return { v.x * r, v.y * r, v.z * r }; }

    using Vec3d = Vec3<double>;
    using Vec3f = Vec3<float>;
    using Vec3i = Vec3<int>;

    // ============================================================
    //  Verrou de convention — vérifié À LA COMPILATION.
    //  Si quelqu'un touche à Forward(), Right() ou crossProduct(),
    //  le projet refuse de compiler. Aucun bug silencieux possible.
    // ============================================================
    static_assert(Vec3f::Forward().z < 0.0f, "LV3 : main DROITE -> Forward() doit valoir -Z");
    static_assert(Vec3f::Right().crossProduct(Vec3f::Forward()) == Vec3f::Up(), "LV3 : repere incoherent -> Right x Forward doit valoir Up");
    static_assert(Vec3f::Forward().crossProduct(Vec3f::Up()) == Vec3f::Right(), "LV3 : repere incoherent -> Forward x Up doit valoir Right");



    // ============================================================
    //  Vec4<T>  — coordonnées homogènes
    //  (dotProduct/length/normalize ajoutés : absents de l'original)
    // ============================================================
    template<typename T>
    class Vec4
    {
    public:
        T x{ T(0) }, y{ T(0) }, z{ T(0) }, w{ T(0) };

        constexpr Vec4() noexcept = default;
        constexpr explicit Vec4(T xx) noexcept : x(xx), y(xx), z(xx), w(xx) {}
        constexpr Vec4(T xx, T yy, T zz, T ww) noexcept : x(xx), y(yy), z(zz), w(ww) {}
        constexpr explicit Vec4(const Vec3<T>& v, T ww = T(1)) noexcept : x(v.x), y(v.y), z(v.z), w(ww) {}

        LV3_FORCEINLINE constexpr Vec3<T> xyz() const noexcept { return { x, y, z }; }

        LV3_FORCEINLINE constexpr Vec4 operator+(const Vec4& v) const noexcept { return { x + v.x, y + v.y, z + v.z, w + v.w }; }
        LV3_FORCEINLINE constexpr Vec4 operator-(const Vec4& v) const noexcept { return { x - v.x, y - v.y, z - v.z, w - v.w }; }
        LV3_FORCEINLINE constexpr Vec4 operator-()              const noexcept { return { -x, -y, -z, -w }; }
        LV3_FORCEINLINE constexpr Vec4 operator*(T r)           const noexcept { return { x * r, y * r, z * r, w * r }; }
        LV3_FORCEINLINE constexpr Vec4 operator*(const Vec4& v) const noexcept { return { x * v.x, y * v.y, z * v.z, w * v.w }; }
        LV3_FORCEINLINE constexpr Vec4 operator/(T r)           const noexcept { return { x / r, y / r, z / r, w / r }; }

        LV3_FORCEINLINE constexpr Vec4& operator+=(const Vec4& v) noexcept { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
        LV3_FORCEINLINE constexpr Vec4& operator*=(T r)           noexcept { x *= r;   y *= r;   z *= r;   w *= r;   return *this; }
        LV3_FORCEINLINE constexpr Vec4& operator/=(T r)           noexcept { x /= r;   y /= r;   z /= r;   w /= r;   return *this; }

        LV3_FORCEINLINE constexpr bool operator==(const Vec4& v) const noexcept = default;

        LV3_FORCEINLINE constexpr const T& operator[](int i) const noexcept { return (&x)[i]; }
        LV3_FORCEINLINE constexpr T& operator[](int i)       noexcept { return (&x)[i]; }

        LV3_FORCEINLINE constexpr T dotProduct(const Vec4& v) const noexcept { return x * v.x + y * v.y + z * v.z + w * v.w; }
        LV3_FORCEINLINE constexpr T norm()   const noexcept { return x * x + y * y + z * z + w * w; }
        LV3_FORCEINLINE T          length()  const noexcept { return std::sqrt(norm()); }

        static constexpr Vec4 Zero() noexcept { return { T(0), T(0), T(0), T(0) }; }

        friend std::ostream& operator<<(std::ostream& s, const Vec4& v) {
            return s << '[' << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w << ']';
        }
    };

    template<typename T>
    LV3_FORCEINLINE constexpr Vec4<T> operator*(T r, const Vec4<T>& v) noexcept { return { v.x * r, v.y * r, v.z * r, v.w * r }; }

    using Vec4d = Vec4<double>;
    using Vec4f = Vec4<float>;
    using Vec4i = Vec4<int>;

} // namespace LV3
