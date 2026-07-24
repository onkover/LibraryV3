#include "pch.h"

#include <fstream>
#include <filesystem>

#include "../Core/Logger.h"
#include "Serializer.hpp"
#include "../Ressources/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace LV3
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
		Logger::log("\033[32mPremière passe : initialisation des données\033[0m");
		Logger::log("- Lecteur et parsing du fichier json");

		std::ifstream file(sceneFilePath + jsonSceneFile);
		if (!file.is_open()) {
			Logger::error("\033[31mSceneSerializer::Load — fichier introuvable : " + sceneFilePath + jsonSceneFile + "\033[0m");
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
			Logger::error(std::string("\033[31mSceneSerializer::Load — JSON malformé : ") + e.what() + "\033[0m");
			return false;
		}

		Logger::log("Phase 1 : Construction de la scène" + sceneData["sceneName"].get<std::string>());            // utiliser sceneData["sceneName"].dump() si on n"est pas sûr que ce soit une string


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

				if (!ParseNode(&nodeJson, ctx, entity))
				{
					Logger::error("\033[31mSceneSerializer::Load — erreur lors du parsing du noeud : " + id + "\033[0m");
					return false;
				}

				Logger::log("Première passe terminée. " + std::to_string(entityMap.size()) + " noeuds créés.");

			}

			Logger::log("\033[32mPhase 2 : Link des hiérarchie\033[0m");

			for (const auto& nodeJson : sceneData["nodes"])
			{
				if (!ParseHierarchy(&nodeJson, ctx))
				{
					Logger::error("\033[31mSceneSerializer::Load — erreur lors des hiérarchies\033[0m");
					return false;
				}
			}

			Logger::log("\033[32mDeuxième passe terminée. Hiérarchie assemblée. \033[0m");
			Logger::log("\033[32mBuildSceneGraph (ECS) terminé. " + std::to_string(registry.GetAliveCount()) + " entités créées. \033[0m");
			Logger::log("\033[32mConstruction de la scène terminée avec succès.\033[0m");

		}
		else
		{
			Logger::warn("\033[31mSceneSerializer::Load — clé 'nodes' absente ou invalide dans " + sceneFilePath + " \033[0m");
		}

		Logger::log("\033[32mSceneSerializer::Load — scène chargée : " + sceneFilePath + " \033[0m");


		return true;
	}

	bool SceneSerializer::ParseNode(const void* pJsonNode, ParseContext& ctx, Entity entity)
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
					ParseMesh(&compJson, ctx, entity);

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

		ctx.registry.addComponent(entity, std::move(t)); // Transforme la copie forcée (si on joint juste 't' en déplacement grace à std::move(t)
														 // TransformComponent est un POD pur (que des Vec3f / Matrix44f) — le gain est nul ici en pratique, mais l'uniformité du réflexe compte : on prend l'habitude de toujours céder une lvalue locale qu'on ne réutilise plus, sans se demander à chaque fois si le composant est « assez lourd » pour que ça vaille le coup. 
														 // Le compilateur ne te punira jamais pour un move inutile sur un POD.


	}

	void SceneSerializer::ParseMesh(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;

		std::string meshPath;
		meshPath = compJson.value("model", "");

		MeshComponent m;
		if (compJson.contains("orbitalSpeed")) m.m_orbitalSpeed = compJson["orbitalSpeed"];
		if (compJson.contains("rotationSpeed")) m.m_rotationSpeed = compJson["rotationSpeed"];

		const std::string fullPath = ResolvePath(ctx.baseDir, meshPath);

		MeshHandle hMesh;
		if (compJson.contains("model")) 
		{
			OBJLoadOptions  opts;
			opts.flipUVsVertically = false;
			opts.generateNormalsIfMissing = true;
			hMesh = ctx.pRM.LoadMesh(compJson["model"].get<std::string>(), opts);
		}
		if (!hMesh.IsValid())
		{
			Logger::error("\033[31mSceneSerializer::ParseMesh — échec du chargement du mesh : " + fullPath + " \033[0m");
			return;
		}
		
		if (compJson.contains("texture")) m.m_texture = compJson["texture"];

		//                    m.m_mesh->AABB.resetAABB();
		//                    m.m_mesh->buildAABB(VERTEXSTATE::OBJECT();

		ctx.registry.addComponent(entity, std::move(m)); // Transforme la copie forcée en déplacement passer par move(xxx) que passer par xxx
														   // Ici le gain est réel : m_texture est un std::string, et m_mesh un shared_ptr (dont le déplacement évite un incrément/décrément atomique du compteur de références — pas cher, mais pas gratuit non plus).
	}

	//***************************************************************************************
	void SceneSerializer::ParseLight(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;

		LightComponent l;
		if (compJson.contains("type"))
		{
			std::string typeStr = compJson["type"];
			if (typeStr == "Point")
				l.m_type = ELightType::Point;
			else if (typeStr == "Directional")
				l.m_type = ELightType::Directional;
			else if (typeStr == "Spot")
				l.m_type = ELightType::Spot;
			else if (typeStr == "Ambient")
				l.m_type = ELightType::Ambient;
			else
			{
				l.m_type = ELightType::Ambient;
				std::cout << "\033[31mAvertissement: Type de lumière inconnu '" << typeStr << "' sur le noeud '" << compJson.contains("type") << "'.\033[0m" << std::endl;
				std::cout << "\033[31mAmbient par défaut\033[0m" << std::endl;
			}
		}
		if (compJson.contains("color")) l.m_color = Vec3f{ compJson["color"][0], compJson["color"][1], compJson["color"][2] };
		if (compJson.contains("intensity")) l.m_intensity = compJson["intensity"];

		ctx.registry.addComponent(entity, std::move(l)); // Transforme la copie forcée en déplacement 

	}

	void SceneSerializer::ParseCamera(const void* pJsonNode, ParseContext& ctx, Entity entity, Entity& out_activeCamera)
	{
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;

		CameraComponent c;
		if (compJson.contains("fov")) c.m_fov = compJson["fov"];
		if (compJson.contains("nearPlane")) c.m_nearPlane = compJson["nearPlane"];
		if (compJson.contains("farPlane")) c.m_farPlane = compJson["farPlane"];

		ctx.registry.addComponent(entity, std::move(c)); // Transforme la copie forcée en déplacement 
														// Vérifie bien : out_activeCamera = entity n'utilise jamais c après le déplacement, donc aucun piège ici. 
														// CameraComponent est un POD, gain nul mais cohérence.

		out_activeCamera = entity;          // sauvegarde l'entité de la caméra
											// solution simple, c'est la dernière identifiée
	}

	void SceneSerializer::ParseTrigger(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;


		// value() lit la clé si présente, sinon retourne le défaut fourni —
		// remplace élégamment tes if/else répétés
		const float radius = compJson.value("radius", 1.0f);
		std::string onEnterEvent = compJson.value("onEnterEvent", std::string{});
		std::string onStayEvent = compJson.value("onStayEvent", std::string{});
		std::string onExitEvent = compJson.value("onExitEvent", std::string{});
		const bool isColliding = compJson.value("isColliding", false);

		// On evite de construire un TriggerComponent local et de la transférer ensuite car cela induirait une copie de celui-ci via son constructeur
		// Attention à l'ordre des variables transmises !!!
		ctx.registry.emplaceComponent<TriggerComponent>(
										entity,
										radius,
										std::move(onEnterEvent),	// std::move : les strings locales ne servent plus après, autant les céder
										std::move(onStayEvent),
										std::move(onExitEvent),
										false,						// is_colliding
										std::set<Entity>{}			// overlapping_entities
									);
		// todo : ajouter emplaceComponent là où cela est nécessaire pour les autres parsing de composants

		//TriggerComponent t;


		//if (compJson.contains("radius"))
		//    t.radius = compJson["radius"];
		//else
		//    t.radius = 1.0f; // valeur par défaut


		//// Lit les noms des événements à publier
		//if (compJson.contains("onEnterEvent")) t.onEnterEvent = compJson["onEnterEvent"];
		//if (compJson.contains("onStayEvent")) t.onStayEvent = compJson["onStayEvent"];
		//if (compJson.contains("onExitEvent")) t.onExitEvent = compJson["onExitEvent"];

		//ctx.registry.addComponent(entity, t);
		std::cout << "INFO: Entité " << entity << " a un trigger de rayon : " << radius << std::endl;
	}

	void SceneSerializer::PlayerControl(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;

		PlayerControlComponent t;
		if (compJson.contains("speed")) t.m_speed = compJson["speed"];

		ctx.registry.addComponent(entity, std::move(t)); // Transforme la copie forcée en déplacement 
														// POD trivial (float seul) — cohérence du réflexe, encore une fois.

	}

	void SceneSerializer::ParseHealth(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;

		HealthComponent t;
		if (compJson.contains("maxHealth")) t.m_maxHealth = compJson["maxHealth"];

		ctx.registry.addComponent(entity, std::move(t)); // Transforme la copie forcée en déplacement 
														// POD trivial (int seul) — cohérence du réflexe, encore une fois.
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
				// Ici, rien à changer : HierarchyComponent{ {}, {}, true } est construit directement en argument, donc c'est une prvalue 
				// le compilateur applique déjà le déplacement (voire l'élision de copie) sans ton intervention. Le std::move explicite n'apporte rien sur un temporaire déjà mouvable. C'est le cas exact où ta vigilance doit distinguer lvalue nommée (a besoin de std::move) de temporaire anonyme (n'en a pas besoin).
				ctx.registry.addComponent<HierarchyComponent>(rootEntity, HierarchyComponent{ {}, {},true });
				// todo : optimisation F1 :  Une fois F1 en place, ce sera NULL_ENTITY qu'il faudra écrire ici, pas {}
				// sinon un enfant sans parent explicite pointera silencieusement vers l'entité d'index 0 ({}, c'est-à-dire Entity{} soit 0)
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
		registry.addComponent(child, HierarchyComponent{ parent, {},false }); // pas besoin de std::move(composant) ici car le composant est construit directement en argument, donc c'est une prvalue

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