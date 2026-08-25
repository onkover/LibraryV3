#pragma once
#include "../maths/Projection.h"
#include "../Ressources/ResourceManager.h"

namespace LV3
{

    struct GizmoAssets
    {
        MeshHandle m_perspective;    // camera_gizmo.obj      (pyramide, 6 faces)
        MeshHandle m_orthographic;   // camera_gizmo_box.obj  (boite,   12 faces)

        [[nodiscard]] MeshHandle For(EProjectionType p) const noexcept
        {
            return (p == EProjectionType::Orthographic) ? m_orthographic : m_perspective;
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return m_perspective.IsValid() && m_orthographic.IsValid();
        }
    };

    GizmoAssets LoadGizmoAssets(ResourceManager& rm, const std::string gizmoMeshPerspect, const std::string gizmoMeshOrthogr);
}