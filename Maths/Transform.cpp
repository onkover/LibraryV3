#include "pch.h"
#include "Transform.h"

namespace LV3 {
    Matrix44f Transform::ToLocalMatrix() const noexcept {
        return Matrix44f::Scale(scale) * rotation.ToMatrix44() * Matrix44f::Translation(position);
    }
    Matrix44f Transform::ToWorldMatrix(const Transform* parent) const noexcept {
        Matrix44f local = ToLocalMatrix();
        if (!parent) return local;
        return local * parent->ToWorldMatrix();   // row-major : enfant · parent
    }
    Matrix44f Transform::ToWorldInverseMatrix(const Transform* parent) const noexcept {
        return ToWorldMatrix(parent).inverse();
    }
} // namespace LV3