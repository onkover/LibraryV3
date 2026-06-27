#pragma once

#include <string>
#include "../Ressources/ResourceManager.h
#include "Registry.hpp"
#include <unordered_map>

namespace LibV3
{
    class SceneSerializer
    {
    public:
        SceneSerializer() = delete;

        // ── Désérialisation ───────────────────────────────────
        // Charge un fichier JSON et peuple la scène.
        // Retourne false si le fichier est introuvable ou malformé.
        // La scène est préalablement vidée (Clear()) avant tout ajout.
        static bool LoadSceneGraph(const std::string& sceneFilePath,
            const std::string& jsonSceneFile,
            Registry& registry,
            Entity& out_activeCamera,
            ResourceManager& pRM);


        // ── Sérialisation ─────────────────────────────────────
        // Sérialise l'état courant de la scène vers un fichier JSON.
        // Retourne false si l'écriture échoue.
        //static bool Save(const std::string& path,
        //    const Scene& scene);

    private:
        // ── Parsers internes ──────────────────────────────────
        // Forward-declaration du type JSON pour éviter d'exposer nlohmann dans le header public. Le type est résolu dans le .cpp.
        struct ParseContext
        {
            std::string      baseDir;   // répertoire du fichier JSON (pour les chemins relatifs)
            ResourceManager& pRM;  // référence vers le gestionnaire de ressources
            std::unordered_map<std::string, Entity>& entityMap; // map temporaire pour retrouver les entités par leur ID de noeud
            Registry& registry;
            Entity& out_activeCamera;       // référence vers l'entité de la caméra active (sera modifiée si un noeud "Camera" est trouvé)
        };

        static void linkChildToParent(Registry& registry, Entity child, Entity parent);

        static bool ParseNode(const void* jsonNode, ParseContext& ctx, Entity entity, world* pWorld);
        static void ParseTransform(const void* pJsonNode, ParseContext& ctx, Entity entity);
        static void ParseMesh(const void* pJsonNode, ParseContext& ctx, Entity entity, world* pWorld);
        static void ParseLight(const void* pJsonNode, ParseContext& ctx, Entity entity);
        static void ParseCamera(const void* pJsonNode, ParseContext& ctx, Entity entity, Entity& out_activeCamera);
        static void ParseTrigger(const void* pJsonNode, ParseContext& ctx, Entity entity);
        static void PlayerControl(const void* pJsonNode, ParseContext& ctx, Entity entity);
        static void ParseHealth(const void* pJsonNode, ParseContext& ctx, Entity entity);
        static bool ParseHierarchy(const void* pJsonNode, ParseContext& ctx);


        // ── Sérialiseurs internes ─────────────────────────────
    //    static void* SerializeNode(const SceneNode& node);   // retourne nlohmann::json*
    };
}