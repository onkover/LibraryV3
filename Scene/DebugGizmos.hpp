#pragma once
#include "../maths/Projection.h"    // EProjectionType
//#include "SerializerHelpers.hpp"    // entity
#include "Entity.hpp"
#include "CameraBinding.hpp"

namespace LV3
{
    class Registry;
    class ResourceManager;

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
	void SpawnCameraGizmos(Registry& registry, const GizmoAssets& assets);
//    void CameraGizmoSystem(Registry& registry, Entity activeCamera, float aspect, const GizmoAssets& assets);
    void CameraGizmoSystem(Registry& registry, Entity activeCamera, const CameraBinding* bindings, size_t count, const GizmoAssets& assets);

}