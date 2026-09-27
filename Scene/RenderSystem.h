#pragma once
#include "../Core/logger.h"


// ============================================================
//  Scene/RenderSystem.h — Le systeme de rendu ECS
//
//  Parcourt les entites, transforme, cull, projette, et SOUMET
//  des triangles ecran au Renderer. Il ne connait ni le rasterizer,
//  ni les fragments, ni le format des pixels.
//
//  Appelable N fois par frame : une fois par ViewData.
// ============================================================

namespace LV3
{
    // Declarations anticipees : ce header n'inclut RIEN.
    // Toute retouche de Renderer ou de MeshClass ne recompile
    // que RenderSystem.cpp, pas ses appelants.
    class  Registry;
    class  ResourceManager;
    class  Renderer;
    struct ViewData;
    class  ClipSpaceBuffer;     // Rendering/VertexStage.h — toujours aucun #include ici

    // clipBuf : tampon de l'etage sommets, possede par l'appelant, reutilise
    // pour chaque (instance, vue). Jamais partage entre deux threads.
    void RenderView(Registry& registry, ResourceManager& rm,
        Renderer& renderer, const ViewData& view, ClipSpaceBuffer& clipBuf);

    // Rapport de statistiques de culling. No-op en Release.
    // Appeler UNE fois par frame, après la derniere vue.
    void ReportCullStats();
}
