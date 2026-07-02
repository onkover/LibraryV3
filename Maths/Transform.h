#pragma once
#include "Vectorlib.h"
#include "MatrixLib.h"
#include "QuaternionLib.h"

namespace LV3 {
    struct Transform {
        Vec3f  position = Vec3f::Zero();
        Quatf  rotation = Quatf::Identity();   // QuatF -> Quatf (nom LV3)
        Vec3f  scale = Vec3f::One();

        constexpr Transform() noexcept = default;
        Transform(const Vec3f& pos, const Quatf& rot = Quatf::Identity(),
            const Vec3f& scl = Vec3f::One()) noexcept
            : position(pos), rotation(rot), scale(scl) {
        }

        Matrix44f ToLocalMatrix()  const noexcept;
        Matrix44f ToWorldMatrix(const Transform* parent = nullptr) const noexcept;
        Matrix44f ToWorldInverseMatrix(const Transform* parent = nullptr) const noexcept;

        LV3_FORCEINLINE Vec3f Forward() const noexcept { return rotation.rotate(Vec3f::Forward()); }
        LV3_FORCEINLINE Vec3f Right()   const noexcept { return rotation.rotate(Vec3f::Right()); }
        LV3_FORCEINLINE Vec3f Up()      const noexcept { return rotation.rotate(Vec3f::Up()); }
    };
} // namespace LV3