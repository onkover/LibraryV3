#include "pch.h"          // ← première ligne, toujours#include "SceneGraph.hpp"

#include "SceneGraph.hpp"

using json = nlohmann::json;

namespace LV3
{
    //***************************************************************************************

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
    void linkChildToParent(Registry& registry, Entity child, Entity parent)
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

    //***************************************************************************************
    /// <summary>
    /// Construit le graphe de scène à partir d'un fichier de description JSON.
    /// </summary>
    /// <param name="sceneFilePath">Chemin vers le fichier JSON de la scène.</param>
    /// <param name="registry">Registre des composants</param>
    /// <param name="out_activeCamera">Référence pour stocker l'entité de la caméra active.</param>
    /// <param name="pRM">Gestionnaire de ressources de maillages.</param>
    /// <param name="pWorld">Pointeur vers le monde de jeu.</param>
    /// <returns>True si la construction réussit, false sinon.</returns>
    bool buildSceneGraph(
        const std::string& sceneFilePath,
        Registry& registry,
        Entity& out_activeCamera,
        ResourceManager& pRM, const char* directory)
    {
        // --- 1. CHARGEMENT ET VALIDATION DU FICHIER JSON ---
        std::cout << "Première passe : initialisation des données" << std::endl;
        std::cout << "- Lecteur et parsing du fichier json" << std::endl;

        std::ifstream file(sceneFilePath);
        if (!file.is_open())
        {
            std::cerr << "Erreur [buildSceneGraph]: Impossible d'ouvrir le fichier : " << sceneFilePath << std::endl;
            return false;
        }

        nlohmann::json sceneData;
        try
        {
            file >> sceneData;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            std::cerr << "Erreur [buildSceneGraph]: Erreur de parsing JSON. " << e.what() << std::endl;
            return false;
        }

        std::cout << "Construction de la scène '" << sceneData["sceneName"] << "'..." << std::endl;

        // La map 'entityMap' ne contient que des pointeurs bruts pour un accès rapide par ID.
        // Son unique rôle est de fournir un accès rapide aux noeuds par leur nom sans parcourir la totalité du vecteur à chaque recherche.
        // Donc de servir "d'annuaire" pour retrouver rapidement les entités par leur nom pendant la phase de construction de l'arbre.
        std::unordered_map<std::string, Entity> entityMap;

        // --- 2. PREMIÈRE PASSE : CRÉATION DES NOEUDS ET COMPOSANTS ---
        std::cout << "- Initialisation des structures de données" << std::endl;

        if (!sceneData.contains("nodes"))
        {
            std::cerr << "Erreur [buildSceneGraph]: Le fichier JSON ne contient pas de section 'nodes'." << std::endl;
            return false;
        }

        for (const auto& nodeJson : sceneData["nodes"])
        {
            std::string id = nodeJson["id"];

            // Crée une Entité vide et la stocke dans la map
            Entity entity = registry.CreateEntity();
            entityMap[id] = entity;
            //NameComponent n;
            registry.addComponent<NameComponent>(entity, NameComponent{ id });

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
                    {
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

                        registry.addComponent(entity, t);
                    }

                    else if (compName == "Mesh")
                    {

                        MeshComponent m;
                        if (compJson.contains("orbitalSpeed")) m.m_orbitalSpeed = compJson["orbitalSpeed"];
                        if (compJson.contains("rotationSpeed")) m.m_rotationSpeed = compJson["rotationSpeed"];
                        if (compJson.contains("model")) m.m_mesh = pRM.getMesh(pWorld, directory, compJson["model"]);
                        if (compJson.contains("texture")) m.m_texture = compJson["texture"];

                        //                    m.m_mesh->AABB.resetAABB();
                        //                    m.m_mesh->buildAABB(VERTEXSTATE::OBJECT);

                        registry.addComponent(entity, m);

                    }

                    else if (compName == "Camera")
                    {
                        CameraComponent c;
                        if (compJson.contains("fov")) c.m_fov = compJson["fov"];
                        if (compJson.contains("nearPlane")) c.m_nearPlane = compJson["nearPlane"];
                        if (compJson.contains("farPlane")) c.m_farPlane = compJson["farPlane"];

                        registry.addComponent(entity, c);
                        out_activeCamera = entity;          // sauvegarde l'entité de la caméra
                        // solution simple, c'est la dernière identifiée

                    }

                    else if (compName == "Light")
                    {
                        LightComponent l;
                        if (compJson.contains("type"))
                        {
                            std::string typeStr = compJson["type"];
                            if (typeStr == "Point")
                                    l.m_type = ELightType::Point;
                                if (typeStr == "Directional")
                                    l.m_type = ELightType::Directional;
                                else if (typeStr == "Spot")
                                    l.m_type = ELightType::Spot;
                                else if (typeStr == "Ambient")
                                    l.m_type = ELightType::Ambient;
                                else
                                {
                                    l.m_type = ELightType::Ambient;
                                    std::cout << "Avertissement: Type de lumière inconnu '" << typeStr << "' sur le noeud '" << id << "'." << std::endl;
                                    std::cout << "Ambient par défaut" << std::endl;
                                }

                        }
                        if (compJson.contains("color")) l.m_color = Vec3f{ compJson["color"][0], compJson["color"][1], compJson["color"][2] };
                        if (compJson.contains("intensity")) l.m_intensity = compJson["intensity"];

                        registry.addComponent(entity, l);

                    }

                    else if (compName == "Trigger")
                    {
                        TriggerComponent t;
                        if (compJson.contains("radius")) t.radius = compJson["radius"];

                        // Lit les noms des événements à publier
                        if (compJson.contains("onEnterEvent")) t.onEnterEvent = compJson["onEnterEvent"];
                        if (compJson.contains("onStayEvent")) t.onStayEvent = compJson["onStayEvent"];
                        if (compJson.contains("onExitEvent")) t.onExitEvent = compJson["onExitEvent"];

                        registry.addComponent(entity, t);
                        std::cout << "INFO: Entité " << id << " a un trigger de rayon " << t.radius << std::endl;
                    }

                    else if (compName == "Health")
                    {
                        HealthComponent t;
                        if (compJson.contains("maxHealth")) t.m_maxHealth = compJson["maxHealth"];

                        registry.addComponent(entity, t);
                    }

                    else if (compName == "PlayerControl")
                    {
                        PlayerControlComponent t;
                        if (compJson.contains("speed")) t.m_speed = compJson["speed"];

                        registry.addComponent(entity, t);
                    }

                    else
                    {
                        std::cout << "Avertissement: Composant inconnu '" << compName << "' sur le noeud '" << id << "'." << std::endl;
                    }
                }
            }
        }

        std::cout << "Première passe terminée. " << entityMap.size() << " noeuds créés." << std::endl;


        // --- 3. DEUXIÈME PASSE : ASSEMBLAGE DE LA HIÉRARCHIE ---
        for (const auto& nodeJson : sceneData["nodes"])
        {
            // Vérifie si le noeud a un parent spécifié dans le JSON
            if (nodeJson.contains("parent"))
            {
                std::string childId = nodeJson["id"];
                std::string parentId = nodeJson["parent"];

                // On utilise la map pour retrouver les ID
                Entity childEntity = entityMap[childId];
                Entity parentEntity = entityMap[parentId];

                // On crée le lien parent-enfant
                linkChildToParent(registry, childEntity, parentEntity);

            }
            else
            {
                // S'il n'y a pas de parent, il s'agit donc de la racine (ex: Sun), 
                // on doit quand même lui créer un HierarchyComponent "isRoot"
                Entity rootEntity = entityMap.at(nodeJson["id"]);
                if (!registry.hasComponent<HierarchyComponent>(rootEntity))
                {
                    registry.addComponent<HierarchyComponent>(rootEntity, HierarchyComponent{ {}, {},true });
                }
            }

        }
        std::cout << "Deuxième passe terminée. Hiérarchie assemblée." << std::endl;
        std::cout << "BuildSceneGraph (ECS) terminé. " << registry.getEntityCount() << " entités créées." << std::endl;
        std::cout << "Construction de la scène terminée avec succès." << std::endl;
        return true;
    }

}