#pragma once
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
        int   tileSize = LV3_DEFAULT_TILE_SIZE;
        bool  multithread = LV3_DEFAULT_MULTITHREAD;
        float shadowBias = 0.0001f;
    };

    struct FeaturesConfig
    {
        bool shadows = LV3_DEFAULT_SHADOWS;
        bool fog = true;
        bool wireframe = false;
        bool stats = true;
    };

    struct EngineConfig
    {
        RendererConfig renderer;
        FeaturesConfig features;
        std::string    resourcePath = "assets/";

        // Singleton d'accès — chargé une fois au démarrage
        static EngineConfig& Get()
        {
            static EngineConfig instance;
            return instance;
        }

        void LoadFromJson(const std::string& path); // lit engine.json
    };

} // namespace LV3