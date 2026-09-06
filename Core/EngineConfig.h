#pragma once
#include "EngineSettings.h"

// ============================================================
// Core/EngineConfig.h — HORS pch.h
// Chargée depuis engine.json au démarrage
// Accessible partout via EngineConfig::Get()

/*
La valeur dimensionne un tableau ou un type ?
    OUI → EngineSettings.h  (compile-time)
    NON → engine.json       (startup-time)

La valeur change pendant l'exécution du jeu ?
    OUI → EngineConfig en mémoire (runtime)
    NON → engine.json suffit
*/
// ============================================================

namespace LV3
{

    struct RendererConfig
    {
        bool  rasterizer = true;
        bool  raycaster = false;
        int   tileSize = LV3_DEFAULT_TILE_SIZE;
        int   maxBounces = LV3_DEFAULT_MAX_BOUNCES;
        float shadowBias = LV3_DEFAULT_SHADOW_BIAS;
        float alphaThresh = LV3_DEFAULT_ALPHA_TEST_THRESH;
        bool  multithread = static_cast<bool>(LV3_DEFAULT_FEATURE_MULTITHREAD);
    };

    struct FeaturesConfig
    {
        bool shadows = static_cast<bool>(LV3_DEFAULT_FEATURE_SHADOWS);
        bool fog = static_cast<bool>(LV3_DEFAULT_FEATURE_FOG);
        bool wireframe = static_cast<bool>(LV3_DEFAULT_FEATURE_WIREFRAME);
        bool stats = static_cast<bool>(LV3_DEFAULT_FEATURE_STATS);
        bool raycast = static_cast<bool>(LV3_DEFAULT_FEATURE_RAYCAST);
    };

    struct ResourcesConfig
    {
        std::string path = LV3_DEFAULT_RESOURCE_PATH;
        std::string pathMesh = LV3_DEFAULT_RESOURCE_PATH_MESH;
        std::string pathGraphScene = LV3_DEFAULT_RESOURCE_PATH_SCENE;
    };

    struct SimulationConfig
    {
        float daysPerSecond = LV3_DEFAULT_TIME_SCALE;
    };

    // cf. Annexe A5 §13.3 — parametre de LISIBILITE, ne se derive jamais
    // d'une grandeur geometrique (far plane, etc.)
    struct DebugConfig
    {
        float depthDisplayRange = 80.0f;
    };

    struct EngineConfig
    {
        RendererConfig  renderer;
        FeaturesConfig  features;
        ResourcesConfig resources;
        DebugConfig     debug;
		SimulationConfig simulation;

        static EngineConfig& Get()
        {
            static EngineConfig instance;
            return instance;
        }

        // Charge engine.json et ecrase les valeurs par defaut ci-dessus.
        // Retourne false si le fichier est introuvable ou malforme :
        // *this garde alors ses defauts LV3_DEFAULT_*, jamais un etat partiel.
        bool LoadFromJson(const std::string& path);
    };

} // namespace LV3