#pragma once
#include "Vectorlib.h"
#include "MatrixLib.h"
#include "QuaternionLib.h"

namespace LV3
{
    // Transformation LOCALE pure. Ne connaît aucune hiérarchie :
    // la composition parent/enfant est le métier de WorldTransformSystem.
    struct Transform
    {
        Vec3f position = Vec3f::Zero();
        Quatf rotation = Quatf::Identity();
        Vec3f scale = Vec3f::One();

        constexpr Transform() noexcept = default;
        Transform(const Vec3f& pos,
            const Quatf& rot = Quatf::Identity(),
            const Vec3f& scl = Vec3f::One()) noexcept
            : position(pos), rotation(rot), scale(scl) {
        }

        // S · R · T — LA seule définition de l'ordre de composition du moteur.
        [[nodiscard]] LV3_FORCEINLINE Matrix44f ToLocalMatrix() const noexcept
        {
            return Matrix44f::Scale(scale)
                * rotation.ToMatrix44()
                * Matrix44f::Translation(position);
        }

        [[nodiscard]] LV3_FORCEINLINE Vec3f Forward() const noexcept { return rotation.rotate(Vec3f::Forward()); }
        [[nodiscard]] LV3_FORCEINLINE Vec3f Right()   const noexcept { return rotation.rotate(Vec3f::Right()); }
        [[nodiscard]] LV3_FORCEINLINE Vec3f Up()      const noexcept { return rotation.rotate(Vec3f::Up()); }
    };
}