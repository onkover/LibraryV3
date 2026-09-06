#include "pch.h"
#include "EngineConfig.h"
#include "Logger.h"
#include "JsonReader.h"
#include "../Ressources/json.hpp"

#include <fstream>

namespace LV3
{
    bool EngineConfig::LoadFromJson(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            Logger::error("EngineConfig::LoadFromJson — fichier introuvable : " + path);
            return false;
        }

        nlohmann::json root;
        try
        {
            file >> root;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            Logger::error(std::string("EngineConfig::LoadFromJson — JSON malforme : ") + e.what());
            return false;
        }

        try
        {
            JsonReader r(root, "engine", path);

            // ── renderer ─────────────────────────────────────────
            {
                JsonReader rr = r.Child("renderer");

                renderer.rasterizer = rr.Read("rasterizer", true);
                renderer.raycaster = rr.Read("raycaster", false);
                renderer.tileSize = rr.Read("tile_size", LV3_DEFAULT_TILE_SIZE);
                renderer.maxBounces = rr.Read("max_bounces", LV3_DEFAULT_MAX_BOUNCES);
                renderer.shadowBias = rr.Read("shadow_bias", LV3_DEFAULT_SHADOW_BIAS);
                renderer.alphaThresh = rr.Read("alpha_thresh", LV3_DEFAULT_ALPHA_TEST_THRESH);
                renderer.multithread = rr.Read("multithread", static_cast<bool>(LV3_DEFAULT_FEATURE_MULTITHREAD));

                if (renderer.rasterizer && renderer.raycaster)
                    Logger::warn("[EngineConfig] rasterizer ET raycaster actifs — aucun arbitrage defini");

                if (renderer.tileSize <= 0)
                {
                    Logger::warn("[EngineConfig] renderer.tile_size <= 0, force a " + std::to_string(LV3_DEFAULT_TILE_SIZE));
                    renderer.tileSize = LV3_DEFAULT_TILE_SIZE;
                }

                rr.WarnUnread();
            }

            // ── features ─────────────────────────────────────────
            {
                JsonReader rf = r.Child("features");

                features.shadows = rf.Read("shadows", static_cast<bool>(LV3_DEFAULT_FEATURE_SHADOWS));
                features.fog = rf.Read("fog", static_cast<bool>(LV3_DEFAULT_FEATURE_FOG));
                features.wireframe = rf.Read("wireframe", static_cast<bool>(LV3_DEFAULT_FEATURE_WIREFRAME));
                features.stats = rf.Read("stats", static_cast<bool>(LV3_DEFAULT_FEATURE_STATS));
                features.raycast = rf.Read("raycast", static_cast<bool>(LV3_DEFAULT_FEATURE_RAYCAST));

                if (features.raycast && !renderer.raycaster)
                    Logger::warn("[EngineConfig] features.raycast=true mais renderer.raycaster=false — sans effet");

                rf.WarnUnread();
            }

            // ── resources ────────────────────────────────────────
            {
                JsonReader rs = r.Child("resources");
                resources.path = rs.Read("assets", std::string(LV3_DEFAULT_RESOURCE_PATH));
                resources.pathMesh = rs.Read("mesh", std::string(LV3_DEFAULT_RESOURCE_PATH_MESH));
                resources.pathGraphScene = rs.Read("scene", std::string(LV3_DEFAULT_RESOURCE_PATH_SCENE));
                rs.WarnUnread();
            }

            // ── debug (optionnel) ──────────────────────────────────
            if (r.Has("debug"))
            {
                JsonReader rd = r.Child("debug");
                debug.depthDisplayRange = rd.Read("depthDisplayRange", 80.0f);
                rd.WarnUnread();
            }

            // ── simulation ──────────────────────────────────

            if (r.Has("simulation"))
            {
                JsonReader rs = r.Child("simulationclock");

				// Lecture des paramètres de simulation par défaut pour chaque frame
                simulation.m_timeScale = rs.Read("TimeScale", LV3_DEFAULT_TIME_SCALE);
                simulation.m_simTime = rs.Read("SimTime", LV3_DEFAULT_SIMTIME_SCALE);

				// Lecture des paramètres de simulation un fois pour l'ensemble des crans de simulation
				simulation.m_step = rs.Read("daysPerSecond", LV3_DEFAULT_STEP_SCALE);
                simulation.m_sprint = rs.Read("simTime", LV3_DEFAULT_SPRINT_SCALE);
                simulation.m_min = rs.Read("Step", LV3_DEFAULT_MIN_SCALE);
                simulation.m_max = rs.Read("Sprint", LV3_DEFAULT_MAX_SCALE);

                rs.WarnUnread();
            }

            r.WarnUnread();
        }
        catch (const nlohmann::json::exception& e)
        {
            Logger::error(std::string("EngineConfig::LoadFromJson — type invalide dans engine.json : ") + e.what());
            return false;
        }

        Logger::info("EngineConfig::LoadFromJson — configuration chargee depuis " + path);
        return true;
    }

} // namespace LV3