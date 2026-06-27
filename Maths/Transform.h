#pragma once
#include "Vec3f.h"
#include "Quat.h"
#include "Matrix44f.h"
namespace LibV2 {
    struct Transform {
        Vec3f  position = Vec3f::Zero();
        QuatF  rotation = QuatF::Identity();
        Vec3f  scale    = Vec3f::One();
        constexpr Transform() noexcept = default;
        Transform(const Vec3f& pos, const QuatF& rot=QuatF::Identity(),
                  const Vec3f& scl=Vec3f::One()) noexcept
            : position(pos), rotation(rot), scale(scl) {}
        Matrix44f ToLocalMatrix()  const noexcept;
        Matrix44f ToWorldMatrix(const Transform* parent=nullptr) const noexcept;
        Matrix44f ToWorldInverseMatrix(const Transform* parent=nullptr) const noexcept;
        LV2_FORCEINLINE Vec3f Forward() const noexcept { return rotation.rotate(Vec3f::Forward()); }
        LV2_FORCEINLINE Vec3f Right()   const noexcept { return rotation.rotate(Vec3f::Right()); }
        LV2_FORCEINLINE Vec3f Up()      const noexcept { return rotation.rotate(Vec3f::Up()); }
    };
} // namespace LibV2
