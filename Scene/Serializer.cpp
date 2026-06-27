#include "pch.h"

#include <fstream>
#include <filesystem>

#include "../Core/Logger.h"
#include "Serializer.hpp"
#include "../Ressources/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace LibV3
{

    /**********************************************

        Helper

    **********************************************/

    // Résout un chemin relatif au fichier JSON
    std::string ResolvePath(const std::string& baseDir, const std::string& rel)
    {
        if (rel.empty())
            return {};

        fs::path p = fs::path(baseDir) / fs::path(rel);

        return p.lexically_normal().string();
    }



    bool SceneSerializer::LoadSceneGraph(const std::string& sceneFilePath,
        const std::string& jsonSceneFile,
        Registry& registry,
        Entity& out_activeCamera,
        ResourceManager& pRM)
    {

        // --- 1. CHARGEMENT ET VALIDATION DU FICHIER JSON ---
        std::cout << "Première passe : initialisation des données" << std::endl;
        std::cout << "- Lecteur et parsing du fichier json" << std::endl;

        std::ifstream file(sceneFilePath + jsonSceneFile);
        if (!file.is_open()) {
            Logger::error("SceneSerializer::Load — fichier introuvable : " + sceneFilePath + jsonSceneFile);
            return false;
        }

        // 2. Parser le JSON
        json sceneData;
        try
        {
            file >> sceneData;
        }
        catch (const json::parse_error& e)
        {
            Logger::error(std::string("SceneSerializer::Load — JSON malformé : ") + e.what());
            return false;
        }

        Logger::log("Construction de la scène" + sceneData["sceneName"].get<std::string>());            // utiliser sceneData["sceneName"].dump() si on n"est pas sûr que ce soit une string


        if (sceneData.contains("nodes") && sceneData["nodes"].is_array())
        {
            // Préparer le contexte de parsing
            std::unordered_map<std::string, Entity> entityMap;
            //        ParseContext ctx{ fs::path(sceneFilePath).parent_path().string(), pRM, entityMap, registry, out_activeCamera };
            ParseContext ctx{ sceneFilePath, pRM, entityMap, registry, out_activeCamera };

            for (const auto& nodeJson : sceneData["nodes"])
            {
                // Crée une Entité vide et la stocke dans la , 
                std::string id = nodeJson["id"];
                Entity entity = registry.CreateEntity();
                entityMap[id] = entity;
                registry.addComponent<NameComponent>(entity, NameComponent{ id });

                if (!ParseNode(&nodeJson, ctx, entity, pWorld))
                {
                    Logger::error("SceneSerializer::Load — erreur lors du parsing du noeud : " + id);
                    return false;
                }

                Logger::log("Première passe terminée. " + std::to_string(entityMap.size()) + " noeuds créés.");

            }
            for (const auto& nodeJson : sceneData["nodes"])
            {
                if (!ParseHierarchy(&nodeJson, ctx))
                {
                    Logger::error("SceneSerializer::Load — erreur lors des hiérarchies");
                    return false;
                }
            }

            Logger::log("Deuxième passe terminée. Hiérarchie assemblée.");
            Logger::log("BuildSceneGraph (ECS) terminé. " + std::to_string(registry.getEntityCount()) + " entités créées.");
            Logger::log("Construction de la scène terminée avec succès.");

        }
        else
        {
            Logger::warn("SceneSerializer::Load — clé 'nodes' absente ou invalide dans " + sceneFilePath);
        }

        Logger::log("SceneSerializer::Load — scène chargée : " + sceneFilePath);


        return true;
    }

    bool SceneSerializer::ParseNode(const void* pJsonNode, ParseContext& ctx, Entity entity, world* pWorld)
    {
        const json& nodeJson = *static_cast<const json*>(pJsonNode);
        if (!nodeJson.is_object()) return false;

        if (nodeJson.contains("components"))
        {
            for (auto& [compName, compJson] : nodeJson["components"].items())
            {
                // La ligne de code for (auto& [compName, compJson] : nodeJson["components"].items()) 
                // utilise une fonctionnalité appelée "structured binding" (liaison structurée), qui a été introduite en C++17. 
                // Cette syntaxe très pratique permet de décomposer directement un objet(comme une paire clé - valeur d'une map) en variables distinctes.
                // version compatible C++11/14 serait
                // for (auto& element : nodeJson["components"].items()) {
                //    std::string compName = element.key();
                //    const auto& compJson = element.value();

                if (compName == "Transform")
                    ParseTransform(&compJson, ctx, entity);

                else if (compName == "Mesh")
                    ParseMesh(&compJson, ctx, entity, pWorld);

                else if (compName == "Light")
                    ParseLight(&compJson, ctx, entity);

                else if (compName == "Camera")
                    ParseCamera(&compJson, ctx, entity, ctx.out_activeCamera);

                else if (compName == "Trigger")

                    ParseTrigger(&compJson, ctx, entity);

                else if (compName == "Health")
                    ParseHealth(&compJson, ctx, entity);

                else if (compName == "PlayerControl")
                    PlayerControl(&compJson, ctx, entity);

                else
                    return false;

            }
        }
        return true;
    }

    void SceneSerializer::ParseTransform(const void* pJsonNode, ParseContext& ctx, Entity entity)
    {

        const json& compJson = *static_cast<const json*>(pJsonNode);
        if (!compJson.is_object()) return;

        TransformComponent t;
        if (compJson.contains("translation")) t.m_initialLocalPosition = { compJson["translation"][0], compJson["translation"][1], compJson["translation"][2] };
        if (compJson.contains("rotation"))
        {
            Vec3f eulerAngles = { compJson["rotation"][0].get<float>() * TO_RADIAN,
                                compJson["rotation"][1].get<float>() * TO_RADIAN,
                                compJson["rotation"][2].get<float>() * TO_RADIAN };
            t.m_initialLocalRotation = eulerAngles;
        }
        if (compJson.contains("scale")) t.m_initialLocalScale = { compJson["scale"][0], compJson["scale"][1], compJson["scale"][2] };

        ctx.registry.addComponent(entity, t);
    }

    void SceneSerializer::ParseMesh(const void* pJsonNode, ParseContext& ctx, Entity entity, world* pWorld)
    {
        const json& compJson = *static_cast<const json*>(pJsonNode);
        if (!compJson.is_object()) return;

        std::string meshPath;
        meshPath = compJson.value("model", "");



        MeshComponent m;
        if (compJson.contains("orbitalSpeed")) m.m_orbitalSpeed = compJson["orbitalSpeed"];
        if (compJson.contains("rotationSpeed")) m.m_rotationSpeed = compJson["rotationSpeed"];

        const std::string fullPath = ResolvePath(ctx.baseDir, meshPath);

        if (compJson.contains("model")) m.m_mesh = ctx.pRM.getMesh(pWorld, ctx.baseDir.c_str(), meshPath.c_str());// compJson["model"]);
        // if (compJson.contains("model")) m.m_mesh = ctx.pRM.getMesh(pWorld, ctx.baseDir.c_str(), fullPath.c_str());// compJson["model"]);
        if (compJson.contains("texture")) m.m_texture = compJson["texture"];

        //                    m.m_mesh->AABB.resetAABB();
        //                    m.m_mesh->buildAABB(VERTEXSTATE::OBJECT();

        ctx.registry.addComponent(entity, m);
    }

    void SceneSerializer::ParseLight(const void* pJsonNode, ParseContext& ctx, Entity entity)
    {
        const json& compJson = *static_cast<const json*>(pJsonNode);
        if (!compJson.is_object()) return;

        LightComponent l;
        if (compJson.contains("type"))
        {
            std::string typeStr = compJson["type"];
            if (typeStr == "POINT_LIGHT") l.m_type = POINT_LIGHT;
            //                        else if (typeStr == "DIRECTIONAL_LIGHT") l.m_type = DIRECTIONAL_LIGHT;
            else if (typeStr == "SPOT_LIGHT") l.m_type = SPOT_LIGHT;
        }
        if (compJson.contains("color")) l.m_color = Vec3f{ compJson["color"][0], compJson["color"][1], compJson["color"][2] };
        if (compJson.contains("intensity")) l.m_intensity = compJson["intensity"];

        ctx.registry.addComponent(entity, l);

    }

    void SceneSerializer::ParseCamera(const void* pJsonNode, ParseContext& ctx, Entity entity, Entity& out_activeCamera)
    {
        const json& compJson = *static_cast<const json*>(pJsonNode);
        if (!compJson.is_object()) return;

        CameraComponent c;
        if (compJson.contains("fov")) c.m_fov = compJson["fov"];
        if (compJson.contains("nearPlane")) c.m_nearPlane = compJson["nearPlane"];
        if (compJson.contains("farPlane")) c.m_farPlane = compJson["farPlane"];

        ctx.registry.addComponent(entity, c);
        out_activeCamera = entity;          // sauvegarde l'entité de la caméra
        // solution simple, c'est la dernière identifiée
    }

    void SceneSerializer::ParseTrigger(const void* pJsonNode, ParseContext& ctx, Entity entity)
    {
        const json& compJson = *static_cast<const json*>(pJsonNode);
        if (!compJson.is_object()) return;

        TriggerComponent t;
        if (compJson.contains("radius")) t.radius = compJson["radius"];

        // Lit les noms des événements à publier
        if (compJson.contains("onEnterEvent")) t.onEnterEvent = compJson["onEnterEvent"];
        if (compJson.contains("onStayEvent")) t.onStayEvent = compJson["onStayEvent"];
        if (compJson.contains("onExitEvent")) t.onExitEvent = compJson["onExitEvent"];

        ctx.registry.addComponent(entity, t);
        //    std::cout << "INFO: Entité " << id << " a un trigger de rayon " << t.radius << std::endl;
    }

    void SceneSerializer::PlayerControl(const void* pJsonNode, ParseContext& ctx, Entity entity)
    {
        const json& compJson = *static_cast<const json*>(pJsonNode);
        if (!compJson.is_object()) return;

        PlayerControlComponent t;
        if (compJson.contains("speed")) t.m_speed = compJson["speed"];

        ctx.registry.addComponent(entity, t);
    }

    void SceneSerializer::ParseHealth(const void* pJsonNode, ParseContext& ctx, Entity entity)
    {
        const json& compJson = *static_cast<const json*>(pJsonNode);
        if (!compJson.is_object()) return;

        HealthComponent t;
        if (compJson.contains("maxHealth")) t.m_maxHealth = compJson["maxHealth"];

        ctx.registry.addComponent(entity, t);
    }

    bool SceneSerializer::ParseHierarchy(const void* pJsonNode, ParseContext& ctx)
    {
        const json& nodeJson = *static_cast<const json*>(pJsonNode);
        if (!nodeJson.is_object()) return false;

        // Vérifie si le noeud a un parent spécifié dans le JSON
        if (nodeJson.contains("parent"))
        {
            std::string childId = nodeJson["id"];
            std::string parentId = nodeJson["parent"];

            // On utilise la map pour retrouver les ID
            Entity childEntity = ctx.entityMap[childId];
            Entity parentEntity = ctx.entityMap[parentId];

            // On crée le lien parent-enfant
            linkChildToParent(ctx.registry, childEntity, parentEntity);

        }
        else
        {
            // S'il n'y a pas de parent, il s'agit donc de la racine (ex: Sun), 
            // on doit quand même lui créer un HierarchyComponent "isRoot"
            Entity rootEntity = ctx.entityMap.at(nodeJson["id"]);
            if (!ctx.registry.hasComponent<HierarchyComponent>(rootEntity))
            {
                ctx.registry.addComponent<HierarchyComponent>(rootEntity, HierarchyComponent{ {}, {},true });
            }
        }
        return true;
    }

    /// <summary>
    /// Fonction pour lier un enfant à un parent (et vice versa) dans le Registry
    /// Pour une entité donnée dans HierarchyComponent, nous allons créer une nouvelle nouvelle hiérarchie pour référencer :
    /// * son parent
    /// * ses enfants
    /// 
    /// On recherche ensuite son parent pour référencer son enfant
    /// </summary>
    /// <param name="registry">Registre des composants</param>
    /// <param name="child">entité enfant</param>
    /// <param name="parent">entité parent</param>
    void SceneSerializer::linkChildToParent(Registry& registry, Entity child, Entity parent)
    {
        std::cout << "LIAISON : Entité " << registry.getComponent<NameComponent>(child).m_id << " est maintenant enfant de " << registry.getComponent<NameComponent>(parent).m_id << std::endl;

        // 1. Attache un composant Hierarchie à l'enfant pour référencer son futur parent
        registry.addComponent(child, HierarchyComponent{ parent, {},false });

        // 2. Ajoute l'enfant à la liste du parent
        if (!registry.hasComponent<HierarchyComponent>(parent))
        {
            // Crée un composant parent s'il n'existe pas
            registry.addComponent(parent, HierarchyComponent{ {}, {}, true }); // parent vide, marqué comme racine
        }
        // Retrouve le parent (nouvellement créé ou pas) pour référencer son enfant
        registry.getComponent<HierarchyComponent>(parent).m_children.push_back(child);
    }

}