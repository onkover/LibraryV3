#pragma once
#include "pch.h"

#include "../Ressources/ResourceManager.h"
#include "../Core/config.h"
#include "../Core/logger.h"
#include "SpawnCameraGizmos.hpp"
#include "../Ressources/ResourceHandle.h"
#include <string>     // std::string, std::to_string

namespace LV3
{

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

            //const MeshClass* mg = rm.GetMesh(a.m_perspective);
            //Logger::log("  faces=" + std::to_string(mg->faceCount())
            //    + "  verts=" + std::to_string(mg->vertexPositions.size()));

            //const MeshClass* mg2 = rm.GetMesh(a.m_orthographic);
            //Logger::log("  faces=" + std::to_string(mg2->faceCount())
            //    + "  verts=" + std::to_string(mg2->vertexPositions.size()));
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

}