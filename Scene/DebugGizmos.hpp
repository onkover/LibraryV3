#pragma once
#include "../maths/Projection.h"    // EProjectionType
//#include "SerializerHelpers.hpp"    // entity
#include "Entity.hpp"
//#include "CameraBinding.hpp"
#include "Components/Component.hpp"
#include "System.hpp"

namespace LV3
{
    class Registry;
    class ResourceManager;
    struct CameraBinding;

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

    // Demi-section du gizmo, en unites locales, pour une camera donnee.
    // SOURCE UNIQUE : consommee par CameraGizmoSystem et par Test_GizmoMatchesFrustum.
    /*
     * EProjectionType::Orthographic
            SECTION CONSTANTE (A5 §2.4) : la section ortho ne depend pas de la
            distance, elle vaut orthoHeight — une grandeur de SCENE, non bornee.
            A l'echelle 1 le gizmo est donc ingerable des que la camera cadre large.
            On affiche une MAQUETTE a l'echelle s = L / (orthoHeight/2).
            Le ratio x/y reste EXACTEMENT aspect : la maquette decrit toujours la
            forme du frustum, seule sa taille absolue est une decision d'affichage.
	* EProjectionType::Perspective
            SECTION PROPORTIONNELLE : la section croit avec la distance, donc L est deja un vrai bouton de taille. Rien a changer.
			SECTION VARIABLE (A5 §2.3) : la section depend de la distance L et de l'angle fovY.
			La section vaut L * tan(fovY/2) — une grandeur de SCENE, non bornee.
			A l'echelle 1 le gizmo est donc ingerable des que la camera cadre large.
			On affiche une MAQUETTE a l'echelle s = L * tan(fovY/2).
			Le ratio x/y reste EXACTEMENT aspect : la maquette decrit toujours la
			forme du frustum, seule sa taille absolue est une decision d'affichage.
    */
    [[nodiscard]] inline Vec2f GizmoHalfSection(const CameraComponent& cam, float L) noexcept
    {
        if (cam.m_projection == EProjectionType::Orthographic)
            return { L, L };
        const float tanHalf = std::tan(CameraFovY(cam) * 0.5f);
        return { L * tanHalf, L * tanHalf };
    }

    // Demi-hauteur REELLE du frustum de cam a la distance d. C'est la grandeur par
    // laquelle la matrice de projection divise — la definition meme du cadrage.
    // Ortho : constante (section constante). Perspective : proportionnelle a d.
    [[nodiscard]] inline float FrustumHalfHeightAt(const CameraComponent& cam, float d) noexcept
    {
        return (cam.m_projection == EProjectionType::Orthographic)
            ? cam.m_orthoHeight * 0.5f
            : d * std::tan(CameraFovY(cam) * 0.5f);
    }

    GizmoAssets LoadGizmoAssets(ResourceManager& rm, const std::string gizmoMeshPerspect, const std::string gizmoMeshOrthogr);
	void SpawnCameraGizmos(Registry& registry, const GizmoAssets& assets);
    void CameraGizmoSystem(Registry& registry, Entity activeCamera, const CameraBinding* bindings, size_t count, const GizmoAssets& assets);

}