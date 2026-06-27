#pragma once
#include "Vec3f.h"
#include "Vec4f.h"
namespace LibV2 {
    struct Matrix44f {
        float m[4][4] = {};   // colonne-majeur : m[col][row]
        static Matrix44f Identity()    noexcept;
        static Matrix44f Translation(const Vec3f& t) noexcept;
        static Matrix44f Scale(const Vec3f& s)       noexcept;
        static Matrix44f RotationX(float rad)        noexcept;
        static Matrix44f RotationY(float rad)        noexcept;
        static Matrix44f RotationZ(float rad)        noexcept;
        static Matrix44f LookAt(const Vec3f& eye, const Vec3f& target, const Vec3f& up) noexcept;
        static Matrix44f Perspective(float fovRad, float aspect, float near_, float far_) noexcept;
        static Matrix44f Orthographic(float l, float r, float b, float t, float n, float f) noexcept;

        Matrix44f operator*(const Matrix44f& o) const noexcept;
        Vec3f     operator*(const Vec3f& v)     const noexcept;  // w=1
        Vec4f     operator*(const Vec4f& v)     const noexcept;
        Vec3f     transformDir(const Vec3f& v)  const noexcept;  // w=0

        Matrix44f transpose()    const noexcept;
        Matrix44f inverse()      const noexcept;
        float     determinant()  const noexcept;

        const float* data() const noexcept { return &m[0][0]; }

        LV2_FORCEINLINE float& at(int row, int col)       noexcept { return m[col][row]; }
        LV2_FORCEINLINE float  at(int row, int col) const noexcept { return m[col][row]; }
    };
} // namespace LibV2
