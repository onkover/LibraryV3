#include "pch.h"

#include <fstream>
#include "../Core/Logger.h"
#include "Serializer.hpp"
#include "Core/EventNames.h"
#include "Hierarchy.hpp"
#include "SerializerHelpers.hpp"

namespace LV3
{
	using LV3::JsonReader;
	using nlo_json = nlohmann::json;




	bool SceneSerializer::LoadSceneGraph(const std::string& sceneFilePath,
		const std::string& jsonSceneFile,
		Registry& registry,
		Entity& out_activeCamera,
		ResourceManager& pRM)
	{

		// --- 1. CHARGEMENT ET VALIDATION DU FICHIER JSON ---
		Logger::info("Première passe : initialisation des données");
		Logger::info("- Lecteur et parsing du fichier json");

		std::ifstream file(sceneFilePath + jsonSceneFile);
		if (!file.is_open()) {
			Logger::error("SceneSerializer::Load — fichier introuvable : " + sceneFilePath + jsonSceneFile);
			return false;
		}

		// 2. Parser le JSON
		nlo_json sceneData;
		try
		{
			file >> sceneData;
		}
		catch (const nlo_json::parse_error& e)
		{
			Logger::error(std::string("SceneSerializer::Load — JSON malformé : ") + e.what());
			return false;
		}

		Logger::info("******************************************************");
		Logger::info("Phase 1 : Construction de la scène" + sceneData["sceneName"].get<std::string>());            // utiliser sceneData["sceneName"].dump() si on n"est pas sûr que ce soit une string


		if (sceneData.contains("nodes") && sceneData["nodes"].is_array())
		{
			// Préparer le contexte de parsing
			std::unordered_map<std::string, Entity> entityMap;
			ParseContext ctx{ sceneFilePath, pRM, entityMap, registry, out_activeCamera };

			for (const auto& nodeJson : sceneData["nodes"])
			{
				// Crée une Entité vide et la stocke dans la , 
				std::string id = nodeJson["id"];
				if (ctx.entityMap.count(id))    // ou entityMap.contains(id) en C++20+
				{
					Logger::error("LoadSceneGraph — id dupliqué : '" + id + "'");
					return false;
				}

				Entity entity = registry.CreateEntity();
				entityMap[id] = entity;
				registry.addComponent<NameComponent>(entity, NameComponent{ id });

				if (!ParseNode(&nodeJson, ctx, entity))
				{
					Logger::error("SceneSerializer::Load — erreur lors du parsing du noeud : " + id);
					return false;
				}

				Logger::info(id + " " + std::to_string(entityMap.size()) + " noeuds créés.");

			}
			Logger::info("Première passe terminée.\n");
			Logger::info("*****************************");
			Logger::info("Phase 2 : Link des hiérarchie");

			for (const auto& nodeJson : sceneData["nodes"])
			{
				if (!ParseHierarchy(&nodeJson, ctx))
				{
					Logger::error("SceneSerializer::Load — erreur lors des hiérarchies");
					return false;
				}
			}

			Logger::info("Deuxième passe terminée. Hiérarchie assemblée.");
			Logger::info("BuildSceneGraph (ECS) terminé. " + std::to_string(registry.GetAliveCount()) + " entités créées.");
			Logger::info("Construction de la scène terminée avec succès.");

			ResolveDeferredReferences(ctx);
			ValidateHierarchy(registry);
		}
		else
		{
			Logger::warn("SceneSerializer::Load — clé 'nodes' absente ou invalide dans " + sceneFilePath);
		}

		Logger::info("SceneSerializer::Load — scène chargée : " + sceneFilePath);



		return true;
	}




	bool SceneSerializer::ParseNode(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& nodeJson = *static_cast<const nlo_json*>(pJsonNode);
		if (!nodeJson.is_object()) return false;

		if (!nodeJson.contains("components")) return true;
		const nlo_json& comps = nodeJson["components"];

		// ============================================================
		//  Le Transform D'ABORD, hors de la boucle.
		//
		//  /!\ nlohmann::json stocke ses objets dans un std::map :
		//      items() parcourt les cles par ordre ALPHABETIQUE,
		//      PAS dans l'ordre d'ecriture du fichier.
		//      "Camera" < "CameraFPS" < "Mesh" < "Transform" < "Trigger"
		//      -> le Transform serait parse en avant-dernier.
		//
		//  Or ParseMesh (rayon d'orbite) et ParseCameraFPS (yaw/pitch
		//  initiaux) le LISENT. Ils doivent le trouver deja en place.
		// ============================================================
		if (comps.contains("Transform"))
			ParseTransform(&comps["Transform"], ctx, entity);

		for (auto& [compName, compJson] : comps.items())
		{
			if (compName == "Transform")     continue;              // deja fait ci-dessus
			else if (compName == "Mesh")          ParseMesh(&compJson, ctx, entity);
			else if (compName == "Light")         ParseLight(&compJson, ctx, entity);
			else if (compName == "Camera")        ParseCamera(&compJson, ctx, entity, ctx.out_activeCamera);
			else if (compName == "CameraFPS")     ParseCameraFPS(&compJson, ctx, entity);
			else if (compName == "CameraFollow")  ParseCameraFollow(&compJson, ctx, entity);
			else if (compName == "Trigger")       ParseTrigger(&compJson, ctx, entity);
			else if (compName == "Health")        ParseHealth(&compJson, ctx, entity);
			else if (compName == "PlayerControl") PlayerControl(&compJson, ctx, entity);
			else
			{
				// Un nom de composant inconnu ne doit PAS avorter tout le chargement.
				Logger::warn("Composant inconnu ignore : '" + compName + "' sur " + EntityLabel(ctx.registry, entity) + "\n");
			}
		}
		Logger::info("Tous les composants ont été parsés.");
		return true;

	}

	void SceneSerializer::ParseTransform(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{

		const nlo_json& compJson = *static_cast<const nlo_json*>(pJsonNode);
		if (!compJson.is_object()) return;

		LV3::JsonReader r(compJson, "Transform", EntityLabel(ctx.registry, entity));

		TransformComponent t;
		t.m_local.position = r.ReadVector("translation", Vec3f::Zero());
		t.m_local.scale = r.ReadVector("scale", Vec3f::One());

		// Le JSON stocke des DEGRES. Quat(v, true) fait la conversion lui-meme :
		// ne PAS la faire une seconde fois ici.
		const Vec3f eulerDeg = r.ReadVector("rotation", Vec3f::Zero());
		t.m_local.rotation = Quatf(eulerDeg, true);

		t.m_initialRotation = t.m_local.rotation;   // reference figee pour AnimationSystem
		t.m_dirty = true;

		ctx.registry.addComponent(entity, std::move(t));// Transforme la copie forcée (si on joint juste 't' en déplacement grace à std::move(t)
														 // TransformComponent est un POD pur (que des Vec3f / Matrix44f) — le gain est nul ici en pratique, mais l'uniformité du réflexe compte : on prend l'habitude de toujours céder une lvalue locale qu'on ne réutilise plus, sans se demander à chaque fois si le composant est « assez lourd » pour que ça vaille le coup. 
														 // Le compilateur ne te punira jamais pour un move inutile sur un POD.

	}
	void SceneSerializer::ParseMesh(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& j = *static_cast<const nlo_json*>(pJsonNode);
		if (!j.is_object()) return;

		JsonReader r(j, "Mesh", EntityLabel(ctx.registry, entity));   // ← 1 fois : ouverture

		// --- 1. Un MeshComponent sans mesh n'a aucun sens ---
		const std::string modelPath = r.Read("model", std::string(""));
		if (modelPath.empty())
		{
			Logger::warn("ParseMesh : cle 'model' absente sur " + EntityLabel(ctx.registry, entity));
			return;
		}

		// --- 2. Chargement. UNE seule variable de chemin, celle qu'on charge vraiment ---
		const std::string fullPath = ResolvePath(ctx.baseDir, modelPath);

		OBJLoadOptions opts;
		opts.flipUVsVertically = false;
		opts.generateNormalsIfMissing = true;

		auto meshResult = ctx.pRM.LoadMeshChecked(fullPath, opts);
		if (!meshResult.has_value())
		{
			const char* reason =
				meshResult.error() == EMeshLoadError::FileNotFound ? "fichier introuvable"
				: meshResult.error() == EMeshLoadError::ParseFailed ? "echec de parsing OBJ"
				: "mesh vide";
			Logger::error("ParseMesh — " + std::string(reason) + " : " + modelPath);
			return;
		}
		const MeshHandle hMesh = *meshResult;

		// --- 3. Rayon d'orbite, FIGÉ ici et jamais recalculé ensuite ---
		//     Plan XZ uniquement : inclure Y fausserait le rayon.
		//     /!\ Exige que ParseTransform ait été appelé AVANT (voir ParseNode).
		float orbitRadius = 0.0f;
		if (const TransformComponent* tr = ctx.registry.TryGet<TransformComponent>(entity))
		{
			const Vec3f& p = tr->m_local.position;
			orbitRadius = Vec3f(p.x, 0.0f, p.z).length();
		}
		else
		{
			Logger::warn("ParseMesh : pas de Transform sur " + EntityLabel(ctx.registry, entity) + " — orbite desactivée");
		}

		// --- 4. Construction sur place ---
		//     /!\ L'ORDRE suit EXACTEMENT la declaration de MeshComponent.
		//         Inserer un membre au milieu du struct casse cet appel EN SILENCE.
		ctx.registry.emplaceComponent<MeshComponent>(
			entity,
			hMesh,                                  // m_meshHandle
			r.Read("orbitalSpeed", 0.0f),         // m_orbitalSpeed
			r.Read("rotationSpeed", 0.0f),         // m_rotationSpeed
			orbitRadius,                            // m_orbitRadius          ← était perdu
			0.0f,                                   // m_currentOrbitAngle
			0.0f                                    // m_currentRotationAngle
		);

		// todo lecture de la texture

		static_assert(std::is_trivially_copyable_v<MeshComponent>, "MeshComponent doit rester un POD : pas de std::string ni de conteneur");

		r.WarnUnread();                                                    // ← 1 fois : fermeture
	}
	

	//***************************************************************************************
	void SceneSerializer::ParseLight(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& compJson = *static_cast<const nlo_json*>(pJsonNode);
		if (!compJson.is_object()) return;

		const std::string owner = EntityLabel(ctx.registry, entity);
		JsonReader r(compJson, "Light", owner);

		std::string typeStr = r.Read("type", std::string("Ambient"));

		LightComponent l;

		if (typeStr == "Point")				l.m_type = ELightType::Point;
		else if (typeStr == "Directional")	l.m_type = ELightType::Directional;
		else if (typeStr == "Spot")			l.m_type = ELightType::Spot;
		else if (typeStr == "Ambient") 		l.m_type = ELightType::Ambient;
		else
		{
			l.m_type = ELightType::Ambient;
			Logger::warn("[Light] " + owner + " : type inconnu '" + typeStr + "' -> Ambient par defaut");
		}

		l.m_color = r.ReadVector("color", Vec3f::One());
		l.m_intensity = r.Read("intensity", 1.0f);

		ctx.registry.addComponent(entity, std::move(l)); // Transforme la copie forcée en déplacement 

		r.WarnUnread();
	}
	//********************************************************************

	void SceneSerializer::ParseCamera(const void* pJsonNode, ParseContext& ctx, Entity entity, Entity& out_activeCamera)
	{
		const nlo_json& j = *static_cast<const nlo_json*>(pJsonNode);
		if (!j.is_object()) return;

		const std::string owner = EntityLabel(ctx.registry, entity);
		JsonReader r(j, "Camera", owner);

		CameraComponent c;

		// ── 1. PROJECTION : discriminant de premier niveau ─────────────
//		c.m_projection = r.ReadProjectionType("projection");
		c.m_projection = ReadProjectionType(r, "projection");

		// ── 2. PLANS ──────────────────────────────────────────────────
		c.m_nearPlane = r.Read("near", 0.1f);
		c.m_infiniteFar = r.Read("infiniteFar", false);
		c.m_farPlane = r.Read("far", 1000.0f);

		if (c.m_infiniteFar && r.Has("far"))
			Logger::warn("\033[33m[Camera] " + owner + " : 'far' est ignore (infiniteFar=true)\033[0m");

		// ── 3. LENTILLE : chaque branche assigne TOUS ses champs ──────
		// ** Orthographique **
		if (c.m_projection == EProjectionType::Orthographic)
		{
			c.m_lensModel = ELensModel::FieldOfView;      // sans objet, mais DEFINI
			c.m_orthoHeight = r.Read("orthoHeight", 10.0f);
		}
		else
		{
			// ** Perspective **
			const std::string lens = r.Read("lens", std::string("fov"));
			c.m_lensModel = (lens == "filmback") ? ELensModel::Filmback : ELensModel::FieldOfView;
			if (lens != "fov" && lens != "filmback")
				Logger::warn("\033[33m[Camera] " + owner + " : 'lens' inconnu '" + lens + "' -> fov\033[0m");

			if (c.m_lensModel == ELensModel::Filmback)
			{
				c.m_focalLengthMm = r.Read("focalLength", 35.0f);
				c.m_filmHeightMm = r.Read("filmHeight", 24.0f);
				c.m_gateFit = (r.Read("gateFit", std::string("fill")) == "overscan")
					? EGateFit::Overscan : EGateFit::Fill;
			}
			else
			{
				c.m_fovYDeg = std::clamp(r.Read("fov", 45.0f), 1.0f, 179.0f);
			}
		}

		// ── 4. GIZMO ──────────────────────────────────────────────────
		if (r.Has("gizmo"))
		{
			JsonReader rg = r.Child("gizmo");
			c.m_gizmoLength = std::max(0.0f, rg.Read("length", 2.0f));
			rg.WarnUnread();
		}
		else
			c.m_gizmoLength = 0.0f;


		// ── 5. SELECTION ──────────────────────────────────────────────
		c.m_isActive = r.Read("active", false);   // defaut FALSE : l'activite se declare
		c.m_priority = r.Read("priority", 0);

		// ── 6. GARDE-FOUS QUI PARLENT ─────────────────────────────────
		if (c.m_nearPlane <= 0.0f)
		{
			Logger::warn("[Camera] " + owner + " : near <= 0, force a 0.1");
			c.m_nearPlane = 0.1f;
		}
		if (!c.m_infiniteFar && c.m_farPlane <= c.m_nearPlane)
		{
			Logger::warn("[Camera] " + owner + " : far <= near, force a near*1000");
			c.m_farPlane = c.m_nearPlane * 1000.0f;
		}

		ctx.registry.addComponent(entity, std::move(c));
		r.WarnUnread();                                    // DERNIERE ligne du parse

		if (c.m_isActive)
		{
			const CameraComponent* cur = (out_activeCamera != NULL_ENTITY)
				? ctx.registry.TryGet<CameraComponent>(out_activeCamera) : nullptr;
			if (!cur || c.m_priority >= cur->m_priority)
				out_activeCamera = entity;
		}
	}

	//********************************************************************
	void SceneSerializer::ParseCameraFPS(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& j = *static_cast<const nlo_json*>(pJsonNode);
		if (!j.is_object()) return;

		JsonReader r(j, "CameraFPS", EntityLabel(ctx.registry, entity));   // ← 1 fois : ouverture

		FPSControllerComponent c;
		c.m_isEnabled = r.Read("enabled", true);
		c.m_moveSpeed = r.Read("moveSpeed", 5.0f);
		c.m_mouseSensitivity = r.Read("mouseSensitivity", 0.15f);
		c.m_lockVertical = r.Read("lockVertical", true);
		c.m_pitchLimitDeg = r.Read("pitchLimit", 89.0f);
		c.m_sprintMultiplier = r.Read("sprintMultiplier", 3.0f);

		// Angles initiaux dérivés du Transform déjà parsé, sinon la caméra
		// saute à (0,0) à la première frame.
		if (const TransformComponent* tr = ctx.registry.TryGet<TransformComponent>(entity))
		{
			const Vec3f fwd = tr->m_local.rotation.rotate(Vec3f::Forward());
			c.m_yawDeg = std::atan2(fwd.x, -fwd.z) * TO_DEGRE;
			c.m_pitchDeg = std::asin(std::clamp(fwd.y, -1.0f, 1.0f)) * TO_DEGRE;
		}

		c.m_pitchLimitDeg = std::clamp(c.m_pitchLimitDeg, 1.0f, 89.9f);
		ctx.registry.addComponent(entity, std::move(c));

		r.WarnUnread();
	}

	//********************************************************************
	void SceneSerializer::ParseCameraFollow(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& j = *static_cast<const nlo_json*>(pJsonNode);
		if (!j.is_object()) return;

		JsonReader r(j, "CameraFollow", EntityLabel(ctx.registry, entity));   // ← 1 fois : ouverture

		CameraFollowComponent c;
		c.m_isEnabled = r.Read("enabled", true);
		c.m_offset = r.ReadVector("offset", Vec3f(0.0f, 2.0f, -6.0f));
		c.m_smoothSpeed = r.Read("smoothSpeed", 5.0f);
		c.m_lookAtHeight = r.Read("lookAtHeight", 0.0f);   // vise un peu au-dessus des pieds


		c.m_smoothSpeed = std::max(c.m_smoothSpeed, 0.0f); // 0 = suivi rigide, pas de lissage
		c.m_isInitialized = false;                          // le système fera un snap à la 1re frame

		// --- La cible est une RÉFÉRENCE AVANT : résolution différée ---
		const std::string targetName = r.Read("target", std::string(""));
		ctx.registry.addComponent(entity, std::move(c));

		if (!targetName.empty())
			ctx.pendingFollowTargets.push_back({ entity, targetName });
		else
			Logger::warn("CameraFollow sans 'target' sur : " + EntityLabel(ctx.registry, entity) + " — suivi inactif");

		r.WarnUnread();
	}

	//********************************************************************
	static bool IsKnownEvent(const std::string& e)
	{
		return e.empty()
			|| e == Events::TakingDamage
			|| e == Events::StartedTakingDamage
			|| e == Events::StoppedTakingDamage
			|| e == Events::EntityDied;
	}

	void SceneSerializer::ParseTrigger(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& compJson = *static_cast<const nlo_json*>(pJsonNode);
		if (!compJson.is_object()) return;

		const std::string owner = EntityLabel(ctx.registry, entity);
		JsonReader r(compJson, "Trigger", owner);

		const float radius = r.Read("radius", 1.0f);
		std::string onEnterEvent=r.Read("onEnterEvent", std::string{});
		if (!IsKnownEvent(onEnterEvent)) Logger::warn("[Trigger] " + owner + " : évènement inconnu '" + onEnterEvent + "' — ne sera jamais recu.");

		std::string onStayEvent = r.Read("onStayEvent", std::string{});
		if (!IsKnownEvent(onStayEvent)) Logger::warn("[Trigger] " + owner + " : evenement inconnu '" + onStayEvent + "' — ne sera jamais recu.");

		std::string onExitEvent = r.Read("onExitEvent", std::string{});
		if (!IsKnownEvent(onExitEvent)) Logger::warn("[Trigger] " + owner + " : evenement inconnu '" + onExitEvent + "' — ne sera jamais recu.");

		// PAS de lecture de 'isColliding' : c'est un ÉTAT, écrit par le TriggerSystem
		// à l'exécution — jamais une donnée d'auteur (R10 : un système entretient un
		// invariant, il ne le fabrique pas). Un trigger naît toujours "pas en collision".
		//		const bool isColliding = r.Read("isColliding", false);

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


		Logger::warn("INFO: Entité : " + EntityLabel(ctx.registry, entity) + " a un trigger de rayon : " + std::to_string(radius));

		r.WarnUnread();

	}

	void SceneSerializer::PlayerControl(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& compJson = *static_cast<const nlo_json*>(pJsonNode);
		if (!compJson.is_object()) return;

		const std::string owner = EntityLabel(ctx.registry, entity);
		JsonReader r(compJson, "PlayerControl", owner);

		PlayerControlComponent t;
		t.m_speed = r.Read("speed", 1.0f);

		ctx.registry.addComponent(entity, std::move(t)); // Transforme la copie forcée en déplacement 
		// POD trivial (float seul) — cohérence du réflexe, encore une fois.

		r.WarnUnread();
	}

	void SceneSerializer::ParseHealth(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const nlo_json& compJson = *static_cast<const nlo_json*>(pJsonNode);
		if (!compJson.is_object()) return;
		
		const std::string owner = EntityLabel(ctx.registry, entity);
		JsonReader r(compJson, "Health", owner);

		HealthComponent t;
		t.m_maxHealth = r.Read("maxHealth", 100);
		t.m_currentHealth = r.Read("m_currentHealth", 100);

		ctx.registry.addComponent(entity, std::move(t)); // Transforme la copie forcée en déplacement 
		// POD trivial (int seul) — cohérence du réflexe, encore une fois.

		r.WarnUnread();

	}


	bool SceneSerializer::ParseHierarchy(const void* pJsonNode, ParseContext& ctx)
	{
		const nlo_json& nodeJson = *static_cast<const nlo_json*>(pJsonNode);
		if (!nodeJson.is_object()) return false;

		if (nodeJson.contains("parent"))
		{
			const std::string childId = nodeJson["id"];
			const std::string parentId = nodeJson["parent"];

			// R28 : operator[] d'une map n'est JAMAIS un lookup — il insère.
			// Ici, un parent mal orthographié fabriquait Entity(0) : l'objet
			// devenait enfant du PREMIER noeud de la scène, sans un mot.
			const auto itChild = ctx.entityMap.find(childId);
			const auto itParent = ctx.entityMap.find(parentId);
			LV3_ASSERT(itChild != ctx.entityMap.end());   // créé en passe 1, sinon bug interne

			if (itParent == ctx.entityMap.end())
			{
				Logger::error("ParseHierarchy — parent '" + parentId + "' introuvable pour '" + childId + "'");
				return false;      // un graphe faux ne se charge pas « presque bien »
			}

			linkChildToParent(ctx.registry, itChild->second, itParent->second);
		}
		return true;
	}

	//Attention au piège de ta scène : ton entité a "parent" : "Earth" et "target" : "Earth".Une caméra enfant de sa propre cible se déplace déjà avec elle — le contrôleur de suivi ajoutera son offset par - dessus le mouvement hérité.Tu obtiendras un décalage double.Pour une caméra de suivi, la règle est : pas de parent, ou parent = racine.Le suivi est le mécanisme d'attachement.
	void SceneSerializer::ResolveDeferredReferences(ParseContext& ctx)
	{
		for (const PendingEntityRef& ref : ctx.pendingFollowTargets)
		{
			const auto it = ctx.entityMap.find(ref.targetName);
			if (it == ctx.entityMap.end())
			{
				Logger::warn("CameraFollow : cible " + ref.targetName + " introuvable");
				if (auto* f = ctx.registry.TryGet<CameraFollowComponent>(ref.owner))
					f->m_isEnabled = false;             // on desactive plutot que de crasher
				continue;
			}
			if (auto* f = ctx.registry.TryGet<CameraFollowComponent>(ref.owner))
				f->m_target = it->second;
		}
		ctx.pendingFollowTargets.clear();
	}




}