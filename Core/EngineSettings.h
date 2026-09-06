#pragma once
// ============================================================
//  Core/EngineSettings.h — Réglages compile-time du moteur
//  HORS pch.h — inclure manuellement là où c'est nécessaire
//  Modifier librement : seuls les .cpp incluants recompilent
// 
// Contient que les données non "gravées dans le marbre" qui ne peuvent pas être placées dans "pch.h"
// Contient tout ce qui se règle, se tune, ou se toggle — sans jamais toucher au PCH.
// ============================================================

namespace LV3
{

    // -------------------------------------------------------
    //  LIMITES MÉMOIRE
    //  Bornes maximales des conteneurs — dimensionnent les
    //  structures de données au compile-time
    // -------------------------------------------------------
    #define LV3_MAX_LIGHTS          8       // Lumières actives par scène
    #define LV3_MAX_MATERIALS       256     // Matériaux en mémoire simultanément
    #define LV3_MAX_MESH_VERTICES   65536   // Sommets par mesh (uint16 safe)
    #define LV3_MAX_SUBMESHES       32      // SubMeshes par MeshClass
    #define LV3_MAX_RENDER_LAYERS   32      // Couches de rendu (ELayerMask)
    #define LV3_MAX_ENTITIES        4096    // Entités dans le Registry ECS

    // -------------------------------------------------------
    //  RENDERER CPU
    //  valeurs par défaut
    // Valeurs par défaut — utilisées UNIQUEMENT si engine.json est absent
    // -------------------------------------------------------
    #define LV3_DEFAULT_TILE_SIZE           64      // Pixels par tile (rendu multithread)
    #define LV3_DEFAULT_MAX_BOUNCES         10      // Rebonds raycast max (ray tracer)
    #define LV3_DEFAULT_SHADOW_BIAS         0.0001f // Décalage anti-acné des ombres
    #define LV3_DEFAULT_ALPHA_TEST_THRESH   0.5f    // Seuil EBlendMode::AlphaTest

    // -------------------------------------------------------
    //  FEATURES — toggles système0
    // Valeurs par défaut — utilisées UNIQUEMENT si engine.json est absent
    //  0 = système entier désactivé à la compilation
    //  Le code mort est éliminé par le compilateur (#if)
    // -------------------------------------------------------
    #define LV3_DEFAULT_FEATURE_SHADOWS     0   // Ombres portées
    #define LV3_DEFAULT_FEATURE_WIREFRAME   1   // Mode wireframe (ERenderMode)
    #define LV3_DEFAULT_FEATURE_STATS       1   // Compteurs perf (drawcalls, triangles)
    #define LV3_DEFAULT_FEATURE_RAYCAST     0   // Système de raycasting
    #define LV3_DEFAULT_FEATURE_MULTITHREAD 0   // Rendu des tiles en parallèle
    #define LV3_DEFAULT_FEATURE_FOG         0   // Brouillard atmosphérique
    
    

    // -------------------------------------------------------
    //  RESSOURCES
    //  Capacités des managers de ressources — utilisées UNIQUEMENT si engine.json est absent
    // -------------------------------------------------------
    #define LV3_DEFAULT_MAX_TEXTURES        512     // Slots texture dans le ResourceManager
    #define LV3_DEFAULT_MAX_MESHES          256     // Slots mesh dans le ResourceManager
    #define LV3_DEFAULT_RESOURCE_PATH       "assets/"  // Chemin racine (défaut, surchargeable)
    #define LV3_DEFAULT_RESOURCE_PATH_MESH  "assets/MESHES/"  // Chemin racine (défaut, surchargeable)
    #define LV3_DEFAULT_RESOURCE_PATH_SCENE "assets/GRAPHSCENE/"  // Chemin racine (défaut, surchargeable)

    #define LV3_DEFAULT_TIME_SCALE          0.20f   // Jours simulés par seconde réelle

} // namespace LV3

/*
Exemple d'utilisation dans un .cpp :

// Renderer.cpp
#include "pch.h"
#include "Core/EngineSettings.h"   // ← inclus manuellement ici

void Renderer::Init()
{
    m_tiles.reserve(
        (screenW / LV3_TILE_SIZE) * (screenH / LV3_TILE_SIZE)
    );
}

void Renderer::RenderFrame()
{
#if LV3_FEATURE_MULTITHREAD
    RenderTilesParallel();
#else
    RenderTilesSerial();
#endif

#if LV3_FEATURE_SHADOWS
    RenderShadowPass();
#endif

#if LV3_FEATURE_STATS
    m_stats.drawcalls++;
#endif
}


// LightSystem.cpp
void LightSystem::AddLight(LightComponent* light)
{
    LV3_ASSERT(m_lights.size() < LV3_MAX_LIGHTS);
    m_lights.push_back(light);
}


*/