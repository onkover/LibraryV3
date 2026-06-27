#include "pch.h"
#include "Transform.h"
namespace LibV2 {
    Matrix44f Transform::ToLocalMatrix() const noexcept {
        return Matrix44f::Translation(position) * rotation.ToMatrix() * Matrix44f::Scale(scale);
    }
    Matrix44f Transform::ToWorldMatrix(const Transform* parent) const noexcept {
        Matrix44f local = ToLocalMatrix();
        if (!parent) return local;
        return parent->ToWorldMatrix() * local;
    }
    Matrix44f Transform::ToWorldInverseMatrix(const Transform* parent) const noexcept {
        return ToWorldMatrix(parent).inverse();
    }
} // namespace LibV2
