#pragma once
#include "pch.h"

#include "../Ressources/ResourceManager.h"
#include "../Core/config.h"
#include "../Core/logger.h"
#include "../Ressources/ResourceHandle.h"
#include <string>     // std::string, std::to_string
#include "DebugGizmos.hpp"
#include "Registry.hpp"
#include "Hierarchy.hpp"
#include "system.hpp"
#include "SerializerHelpers.hpp"    // entity
#include "CameraBinding.hpp"

    /**********************************************

        Gestion de l'affichage du gizmo Camera

    **********************************************/
namespace LV3
{

    // Charge les assets pour les 2 types de caméras (perspective et orthographique
    GizmoAssets LoadGizmoAssets(ResourceManager& rm, const std::string gizmoMeshPerspect, const std::string gizmoMeshOrthogr)
    {
        GizmoAssets a;
        auto resultPerspect = rm.LoadMeshChecked(gizmoMeshPerspect, {});
        auto resultOrthogr = rm.LoadMeshChecked(gizmoMeshOrthogr, {});

        if (resultPerspect.has_value() || resultOrthogr.has_value())
        {
            if (resultPerspect.has_value())
            {
                a.m_perspective = *resultPerspect;
                Logger::log("\033[32mLa camera en perspective est chargée\033[0m");
            }
            else
                Logger::error("\033[33m[Gizmo] echec chargement '" + gizmoMeshPerspect + "' code=" + std::to_string(static_cast<int>(resultPerspect.error())) + "\033[0m");

            if (resultOrthogr.has_value())
            {
                a.m_orthographic = *resultOrthogr;
                Logger::log("\033[32mLa camera Orthographique est chargée\033[0m");
            }
            else
                Logger::error("\033[33m[Gizmo] echec chargement '" + gizmoMeshOrthogr + "' code=" + std::to_string(static_cast<int>(resultOrthogr.error())) + "\033[0m");


            Logger::log("\033[32m[Gizmo]\033[0m");
            const MeshClass* mg = rm.GetMesh(a.m_perspective);
            Logger::log("[Gizmo]  faces=" + std::to_string(mg->faceCount())
                + "  verts=" + std::to_string(mg->vertexPositions.size()));

            const MeshClass* mg2 = rm.GetMesh(a.m_orthographic);
            Logger::log("[Gizmo]  faces=" + std::to_string(mg2->faceCount())
                + "  verts=" + std::to_string(mg2->vertexPositions.size()));

            Logger::log("[Gizmo] persp=" + std::to_string(rm.GetMesh(a.m_perspective)->faceCount())
                + " faces, ortho=" + std::to_string(rm.GetMesh(a.m_orthographic)->faceCount())
                + " faces");

            return a;
        }
        else
        {
            if (!resultPerspect)
                Logger::error("\033[33m[Gizmo] echec chargement '" + gizmoMeshPerspect + "' code=" + std::to_string(static_cast<int>(resultPerspect.error())) + "\033[0m");
            if (!resultOrthogr)
                Logger::error("\033[33m[Gizmo] echec chargement '" + gizmoMeshOrthogr + "' code=" + std::to_string(static_cast<int>(resultOrthogr.error())) + "\033[0m");

            return {};
        }

    }

    // ***********************************************************************************
    // Inclusion du gizmo dans le graph scène
    // Le mot "spawn" vient des jeux vidéo. Il signifie apparaître ou le lieu d'apparition d'un joueur, d'un monstre ou d'un objet dans le monde virtue
    void SpawnCameraGizmos(Registry& registry, const GizmoAssets& assets)
    {

        LV3_ASSERT(assets.m_perspective.IsValid() && assets.m_orthographic.IsValid());

        // 1. COLLECTER d'abord : creer des entites pendant l'iteration
        //    d'un ViewGroup invalide les tableaux denses du SparseSet.
        std::vector<Entity> cameras;
        for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
            if (cam.m_gizmoLength > 0.0f) cameras.push_back(e);

        // 2. Creer ensuite.
        for (Entity camEntity : cameras)
        {
            const CameraComponent& cam = registry.getComponent<CameraComponent>(camEntity);

            Entity g = registry.CreateEntity();
            registry.addComponent(g, NameComponent{ "__gizmo(" + EntityLabel(registry, camEntity) + ")" });
            registry.addComponent(g, TransformComponent{});
            registry.addComponent(g, MeshComponent{ assets.For(cam.m_projection) });
            registry.addComponent(g, CameraGizmoComponent{ camEntity, cam.m_gizmoLength });
            registry.addComponent(g, DebugVisualComponent{ Color{}, camEntity });
            linkChildToParent(registry, g, camEntity);
        }
    }

    // ***********************************************************************************
    // Système pour l'affichage du gizmo
    // A executer avant
    // * LocalTransformSystem
    // * WorldTransformSystem
    void CameraGizmoSystem(Registry& registry, Entity activeCamera,
        const CameraBinding* bindings, size_t count,
        const GizmoAssets& assets)
    {
        for (auto&& [e, giz, tr, mc, dbg] :
            registry.ViewGroup<CameraGizmoComponent, TransformComponent,
            MeshComponent, DebugVisualComponent>())
        {
            const CameraComponent* cam = registry.TryGet<CameraComponent>(giz.m_owner);
            if (!cam) continue;

            // L'aspect vient du viewport ou CETTE camera est rendue.
            const CameraBinding* b = nullptr;
            for (size_t i = 0; i < count; ++i)
                if (bindings[i].m_camera == giz.m_owner) { b = &bindings[i]; break; }

            // Camera non rendue : on laisse le gizmo dans son dernier etat valide.
            // Le mesh et la couleur, eux, restent a jour.
            const float aspect = b ? b->m_viewport.Aspect() : 0.0f;
            //const float aspect = 1536.0f / 800.0f;   // provoque l'ancien bug, reintroduit exprès

            if (aspect > 0.0f)
            {
                const float L = giz.m_length;
                Vec3f wanted;

                if (cam->m_projection == EProjectionType::Orthographic)
                {
                    const float halfH = cam->m_orthoHeight * 0.5f;
                    wanted = { halfH * aspect, halfH, L };
                }
                else
                {
                    const float tanHalf = std::tan(CameraFovY(*cam) * 0.5f);
                    wanted = { L * tanHalf * aspect, L * tanHalf, L };
                }

                if (std::fabs(wanted.x - tr.m_local.scale.x) > 1e-6f ||
                    std::fabs(wanted.y - tr.m_local.scale.y) > 1e-6f ||
                    std::fabs(wanted.z - tr.m_local.scale.z) > 1e-6f)
                {
                    tr.m_local.scale = wanted;
                    tr.m_dirty = true;
                }
            }

            const MeshHandle want = assets.For(cam->m_projection);
            if (mc.m_meshHandle.id != want.id) mc.m_meshHandle = want;

            dbg.m_color = (giz.m_owner == activeCamera)
                ? Color{ 255, 216,  26 } : Color{ 110, 112, 128 };
        }
    }
    
    //void CameraGizmoSystem(Registry& registry, Entity activeCamera, float aspect, const GizmoAssets& assets)
    //{
    //    for (auto&& [e, giz, tr, mc, dbg] :
    //        registry.ViewGroup<CameraGizmoComponent, TransformComponent,
    //        MeshComponent, DebugVisualComponent>())
    //    {
    //        const CameraComponent* cam = registry.TryGet<CameraComponent>(giz.m_owner);
    //        if (!cam) continue;

    //        const float L = giz.m_length;
    //        Vec3f wanted;

    //        if (cam->m_projection == EProjectionType::Orthographic)
    //        {
    //            // Section CONSTANTE : Sx / Sy ne dependent PAS de L.
    //            const float halfH = cam->m_orthoHeight * 0.5f;
    //            wanted = { halfH * aspect, halfH, L };
    //        }
    //        else
    //        {
    //            // Section PROPORTIONNELLE a z : Sx / Sy sont multiplies par L.
    //            const float tanHalf = std::tan(CameraFovY(*cam) * 0.5f);
    //            wanted = { L * tanHalf * aspect, L * tanHalf, L };
    //        }

    //        if (std::fabs(wanted.x - tr.m_local.scale.x) > 1e-6f ||
    //            std::fabs(wanted.y - tr.m_local.scale.y) > 1e-6f ||
    //            std::fabs(wanted.z - tr.m_local.scale.z) > 1e-6f)
    //        {
    //            tr.m_local.scale = wanted;
    //            tr.m_dirty = true;
    //        }

    //        // Le type de projection peut changer a l'execution : le mesh suit.
    //        const MeshHandle want = assets.For(cam->m_projection);
    //        if (mc.m_meshHandle.id != want.id) mc.m_meshHandle = want;

    //        dbg.m_color = (giz.m_owner == activeCamera)
    //            ? Color{ 255, 216,  26 }
    //        : Color{ 110, 112, 128 };
    //    }
    //}
}