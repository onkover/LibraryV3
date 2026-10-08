#include "pch.h"

#include <fstream>
#include "../Core/Logger.h"
#include "Serializer.hpp"
#include "Core/EventNames.h"
#include "Core/EngineConfig.h"
#include "Hierarchy.hpp"
#include "SerializerHelpers.hpp"

namespace LV3
{
	using LV3::JsonReader;
	using nlo_json = nlohmann::json;

	bool SceneSerializer::LoadSceneGraph(const std::string& sceneFilePath,
		const std::string& jsonSceneFile,
		Registry& registry,
		ResourceManager& pRM)
	{

		// --- 1. CHARGEMENT ET VALIDATION DU FICHIER JSON ---
		Logger::info("***************************************************");
		Logger::info("=== Première passe : initialisation des données ===");
		Logger::info("[Diag] - Lecteur et parsing du fichier json");

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

		// ── Niveau FICHIER : meme regle que partout (R28 : jamais operator[]) ──
		JsonReader rs(sceneData, "Scene", jsonSceneFile);
		const std::string sceneName = rs.Read("sceneName", std::string("<sans nom>"));
		Logger::info("=== Phase 1 : Construction de la scène " + sceneName + " ===");

		const nlo_json* pNodes = rs.Delegate("nodes", "aucun noeud");
		rs.WarnUnread();
		if (!pNodes || !pNodes->is_array())
		{
			Logger::error("SceneSerializer::Load — 'nodes' absent ou pas un tableau dans " + jsonSceneFile + " — scene refusee");
			return false;
		}

		std::unordered_map<std::string, Entity> entityMap;
		ParseContext ctx{ sceneFilePath, pRM, entityMap, registry };

		// ── PASSE 1 : chaque noeud lu UNE fois, par UN lecteur ──
		std::size_t index = 0;
		for (const nlo_json& nodeJson : *pNodes)
		{
			// Etiquette de DIAGNOSTIC seulement : la VALIDATION de 'id' est faite par rn.Read
			std::string where = "noeud #" + std::to_string(index++);
			if (!nodeJson.is_object())
			{
				Logger::error("SceneSerializer::Load — " + where + " n'est pas un objet — scene refusee");
				return false;
			}
			if (const auto itId = nodeJson.find("id"); itId != nodeJson.end() && itId->is_string())
				where += " '" + itId->get<std::string>() + "'";

			JsonReader rn(nodeJson, "Node", where);

			const std::string id = rn.Read("id", std::string{});
			if (id.empty())
			{
				Logger::error("SceneSerializer::Load — " + where + " : 'id' absent ou vide — scene refusee");
				return false;
			}
			if (ctx.entityMap.contains(id))
			{
				Logger::error("LoadSceneGraph — id dupliqué : '" + id + "'");
				return false;
			}

			const Entity entity = registry.CreateEntity();
			ctx.entityMap.emplace(id, entity);
			registry.addComponent<NameComponent>(entity, NameComponent{ id });

			if (!ParseNode(rn, ctx, entity))
			{
				Logger::error("SceneSerializer::Load — erreur lors du parsing du noeud : " + id);
				return false;
			}

		#if LV3_VERBOSE_LOG
			Logger::info(id + " " + std::to_string(ctx.entityMap.size()) + " noeuds créés.");
		#endif
		}
		Logger::success("Première passe terminée.\n");
		Logger::info("*****************************");
		Logger::info("Phase 2 : Link des hiérarchie");

		// ── PASSE 2 : plus AUCUNE lecture JSON, seulement des resolutions ──
		if (!ResolveParents(ctx))
			return false;

		ResolveDeferredReferences(ctx);
		ValidateHierarchy(registry);

		Logger::info("[Diag] Deuxième passe terminée. Hiérarchie assemblée.");
		Logger::info("[Diag] BuildSceneGraph (ECS) terminé. " + std::to_string(registry.GetAliveCount()) + " entités créées.");
		Logger::info("[Diag] SceneSerializer::Load — scène chargée : " + sceneFilePath + jsonSceneFile);
		Logger::success("[Diag] Construction de la scène terminée avec succès.\n");

		return true;


		//Logger::info("=== Phase 1 : Construction de la scène" + sceneData["sceneName"].get<std::string>() +" ===");            // utiliser sceneData["sceneName"].dump() si on n"est pas sûr que ce soit une string


		//if (sceneData.contains("nodes") && sceneData["nodes"].is_array())
		//{
		//	// Préparer le contexte de parsing
		//	std::unordered_map<std::string, Entity> entityMap;
		//	ParseContext ctx{ sceneFilePath, pRM, entityMap, registry };// , out_activeCamera };

		//	for (const auto& nodeJson : sceneData["nodes"])
		//	{
		//		// Crée une Entité vide et la stocke dans la , 
		//		std::string id = nodeJson["id"];
		//		if (ctx.entityMap.count(id))    // ou entityMap.contains(id) en C++20+
		//		{
		//			Logger::error("LoadSceneGraph — id dupliqué : '" + id + "'");
		//			return false;
		//		}

		//		Entity entity = registry.CreateEntity();
		//		entityMap[id] = entity;
		//		registry.addComponent<NameComponent>(entity, NameComponent{ id });

		//		if (!ParseNode(&nodeJson, ctx, entity))
		//		{
		//			Logger::error("SceneSerializer::Load — erreur lors du parsing du noeud : " + id);
		//			return false;
		//		}

		//		#if LV3_VERBOSE_LOG
		//			Logger::info(id + " " + std::to_string(entityMap.size()) + " noeuds créés.");
		//		#endif		

		//	}
		//	Logger::success("Première passe terminée.\n");
		//	Logger::info("*****************************");
		//	Logger::info("Phase 2 : Link des hiérarchie");

		//	for (const auto& nodeJson : sceneData["nodes"])
		//	{
		//		if (!ParseHierarchy(&nodeJson, ctx))
		//		{
		//			Logger::error("SceneSerializer::Load — erreur lors des hiérarchies");
		//			return false;
		//		}
		//	}


		//	ResolveDeferredReferences(ctx);
		//	ValidateHierarchy(registry);

		//	Logger::info("[Diag] Deuxième passe terminée. Hiérarchie assemblée.");
		//	Logger::info("[Diag] BuildSceneGraph (ECS) terminé. " + std::to_string(registry.GetAliveCount()) + " entités créées.");
		//	Logger::info("[Diag] SceneSerializer::Load — scène chargée : " + sceneFilePath + jsonSceneFile);
		//	Logger::success("[Diag] Construction de la scène terminée avec succès.\n");
		//}
		//else
		//{
		//	Logger::warn("SceneSerializer::Load — clé 'nodes' absente ou invalide dans " + sceneFilePath);
		//}

		//return true;
	}




	//bool SceneSerializer::ParseNode(const void* pJsonNode, ParseContext& ctx, Entity entity)
	//{
	//	const nlo_json& nodeJson = *static_cast<const nlo_json*>(pJsonNode);
	//	if (!nodeJson.is_object()) return false;

	//	if (!nodeJson.contains("components")) return true;
	//	const nlo_json& comps = nodeJson["components"];

	//	// ============================================================
	//	//  Le Transform D'ABORD, hors de la boucle.
	//	//
	//	//  /!\ nlohmann::json stocke ses objets dans un std::map :
	//	//      items() parcourt les cles par ordre ALPHABETIQUE,
	//	//      PAS dans l'ordre d'ecriture du fichier.
	//	//      "Camera" < "CameraFPS" < "Mesh" < "Transform" < "Trigger"
	//	//      -> le Transform serait parse en avant-dernier.
	//	//
	//	//  Or ParseMesh (rayon d'orbite) et ParseCameraFPS (yaw/pitch
	//	//  initiaux) le LISENT. Ils doivent le trouver deja en place.
	//	// ============================================================
	//	if (comps.contains("Transform"))
	//		ParseTransform(&comps["Transform"], ctx, entity);

	//	for (auto& [compName, compJson] : comps.items())
	//	{
	//		if (compName == "Transform")     continue;              // deja fait ci-dessus
	//		else if (compName == "Mesh")          ParseMesh(&compJson, ctx, entity);
	//		else if (compName == "Light")         ParseLight(&compJson, ctx, entity);
	//		else if (compName == "Camera")        ParseCamera(&compJson, ctx, entity);// , ctx.out_activeCamera);
	//		else if (compName == "CameraFPS")     ParseCameraFPS(&compJson, ctx, entity);
	//		else if (compName == "CameraFollow")  ParseCameraFollow(&compJson, ctx, entity);
	//		else if (compName == "Trigger")       ParseTrigger(&compJson, ctx, entity);
	//		else if (compName == "Health")        ParseHealth(&compJson, ctx, entity);
	//		else if (compName == "PlayerControl") PlayerControif compName == "Mesh"l(&compJson, ctx, entity);
	//		else
	//		{
	//			// Un nom de composant inconnu ne doit PAS avorter tout le chargement.
	//			Logger::warn("Composant inconnu ignore : '" + compName + "' sur " + EntityLabel(ctx.registry, entity) + "\n");
	//		}
	//	}
	//	#if LV3_VERBOSE_LOG
	//		Logger::info(EntityLabel(ctx.registry, entity) + " : Tous les composants du node ont été parsés.");
	//	#endif
	//	return true;

	//}

	bool SceneSerializer::ParseNode(JsonReader& rn, ParseContext& ctx, Entity entity)
	{
		// ── 1. PARENT : LU ici, RESOLU plus tard (passe 2, ResolveParents) ──
		//    absente -> souci + racine     null -> info + racine
		if (const nlo_json* pParent = rn.Delegate("parent", "racine"))
		{
			if (!pParent->is_string() || pParent->get_ref<const std::string&>().empty())
			{
				Logger::error("ParseNode — 'parent' doit etre un id (texte non vide) ou null sur " + EntityLabel(ctx.registry, entity));
				return false;     // un graphe faux ne se charge pas « presque bien »
			}
			ctx.pendingParents.push_back({ entity, pParent->get<std::string>() });
		}

		// ── 2. COMPOSANTS : DELEGUES, chaque Parse* ouvre son propre JsonReader ──
		//    absente -> souci (noeud sans composant)     null -> info
		if (const nlo_json* pComps = rn.Delegate("components", "aucun composant"))
		{
			if (!pComps->is_object())
			{
				Logger::error("ParseNode — 'components' doit etre un objet sur " + EntityLabel(ctx.registry, entity));
				return false;
			}
			const nlo_json& comps = *pComps;

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
			if (const auto itT = comps.find("Transform"); itT != comps.end())
				ParseTransform(&*itT, ctx, entity);

			for (auto& [compName, compJson] : comps.items())
			{
				if (compName == "Transform")     continue;              // deja fait ci-dessus
				else if (compName == "Mesh")          ParseMesh(&compJson, ctx, entity);
				else if (compName == "Light")         ParseLight(&compJson, ctx, entity);
				else if (compName == "Camera")        ParseCamera(&compJson, ctx, entity);
				else if (compName == "CameraFPS")     ParseCameraFPS(&compJson, ctx, entity);
				else if (compName == "CameraFollow")  ParseCameraFollow(&compJson, ctx, entity);
				else if (compName == "Trigger")       ParseTrigger(&compJson, ctx, entity);
				else if (compName == "Health")        ParseHealth(&compJson, ctx, entity);
				else if (compName == "PlayerControl") PlayerControl(&compJson, ctx, entity);
				else
				{
					// Un nom de composant inconnu ne doit PAS avorter tout le chargement.
					Logger::warn("Composant inconnu ignore : '" + compName + "' sur " + EntityLabel(ctx.registry, entity));
				}
			}
		}

		// ── 3. CLES DE NOEUD INCONNUES ("parnet", "Notes", "type"...) : enfin signalees.
		//    '_type', '_note' : prefixe '_' = assume, non lu.
		rn.WarnUnread();

		#if LV3_VERBOSE_LOG
			Logger::info(EntityLabel(ctx.registry, entity) + " : Tous les composants du node ont été parsés.");
		#endif
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

		//// --- 2. Chargement. UNE seule variable de chemin, celle qu'on charge vraiment ---
		//const std::string fullPath = ResolvePath(ctx.baseDir, modelPath);

		//OBJLoadOptions opts;
		//opts.flipUVsVertically = false;
		//opts.generateNormalsIfMissing = true;

		//auto meshResult = ctx.pRM.LoadMeshChecked(fullPath, opts);
		//if (!meshResult.has_value())
		//{
		//	const char* reason =
		//		meshResult.error() == EMeshLoadError::FileNotFound ? "fichier introuvable"
		//		: meshResult.error() == EMeshLoadError::ParseFailed ? "echec de parsing OBJ"
		//		: "mesh vide";
		//	Logger::error("ParseMesh — " + std::string(reason) + " : " + modelPath);
		//	return;
		//}
		//const MeshHandle hMesh = *meshResult;

		//// 4d-2 : le composant porte une CHAINE. Sans descripteur, chaine implicite
		//// de longueur 1, partagee par toutes les entites de ce mesh.
		//const LodChainHandle hMeshChain = ctx.pRM.GetOrCreateSingleLevelChain(hMesh);
		//if (!hMeshChain.IsValid())
		//{
		//	Logger::error("ParseMesh — chaine refusee pour : " + modelPath);
		//	return;
		//}

		// --- 2. Chargement. UNE seule variable de chemin, celle qu'on charge vraiment ---
		const std::string fullPath = ResolvePath(ctx.baseDir, modelPath);

		// La scene dit EXPLICITEMENT si l'objet a une chaine de LOD : un .lod.json
		// la decrit, un .obj donne une chaine implicite de longueur 1.
		// UNE definition des options pour la scene : un mesh direct et les niveaux
		// d'une chaine doivent etre charges a l'identique.
		OBJLoadOptions opts;
		opts.flipUVsVertically = false;
		opts.generateNormalsIfMissing = true;

		LodChainHandle hChain;
		if (fullPath.ends_with(".lod.json"))
		{
			const auto chainResult = ctx.pRM.LoadLodChainChecked(fullPath, opts);
			if (!chainResult.has_value())
			{
				Logger::error("ParseMesh — chaine refusee (" + std::string(ToString(chainResult.error())) + ") : " + modelPath);
				return;
			}
			hChain = *chainResult;
		}
		else
		{
			//OBJLoadOptions opts;
			//opts.flipUVsVertically = false;
			//opts.generateNormalsIfMissing = true;

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

			// Sans descripteur : chaine implicite de longueur 1, partagee par
			// toutes les entites de ce mesh.
			hChain = ctx.pRM.GetOrCreateSingleLevelChain(*meshResult);
			if (!hChain.IsValid())
			{
				Logger::error("ParseMesh — chaine refusee pour : " + modelPath);
				return;
			}
		}

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
			//hMesh,                                  // m_meshHandle
			hChain,                                 // m_lodChain
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

	void SceneSerializer::ParseCamera(const void* pJsonNode, ParseContext& ctx, Entity entity)// , Entity& out_activeCamera)
	{
		const nlo_json& j = *static_cast<const nlo_json*>(pJsonNode);
		if (!j.is_object()) return;

		const std::string owner = EntityLabel(ctx.registry, entity);
		JsonReader r(j, "Camera", owner);

		CameraComponent c;

		// ── 1. PROJECTION : discriminant de premier niveau ─────────────
		c.m_projection = ReadProjectionType(r, "projection");

		// ── 2. PLANS ──────────────────────────────────────────────────
		c.m_nearPlane = r.Read("near", 0.1f);
		c.m_infiniteFar = r.Read("infiniteFar", false);
		c.m_farPlane = r.Read("far", 1000.0f);

		// Surcharges MOTEUR (precondition : EngineConfig charge AVANT la scene, cf. main.cpp)
		const EngineConfig& cfg = EngineConfig::Get();

		const std::optional<float> ddr = ReadPositiveOverride(r, "depthDisplayRange", cfg.debug.depthDisplayRange, "Camera", owner);
		c.m_hasDepthDisplayRange = ddr.has_value();
		c.m_depthDisplayRange = ddr.value_or(0.0f);	// sans objet si absent

		const std::optional<float> tau = ReadPositiveOverride(r, "lodTolerancePx", cfg.lod.tolerancePx, "Camera", owner);
		c.m_hasLodTolerancePx = tau.has_value();
		c.m_lodTolerancePx = tau.value_or(0.0f);

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
		c.m_category = ReadCameraCategory(r, "category");
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
		r.WarnUnread();
		
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
		c.m_lookAtHeight = r.Read("lookAtHeight", 0.0f);	   // vise un peu au-dessus des pieds
		c.m_followRotation = r.Read("followRotation", true);   // l'offset pivote-t-il avec la cible ? (clé auparavant jamais lue)
															// true (la caméra - suiveuse classique, "chase cam") : l'offset est d'abord tourné par la rotation courante de la cible, puis ajouté à sa position.Concrètement, si offset = (0, 2, -6) (« 2 au - dessus, 6 derrière »), la caméra reste toujours 2 unités au - dessus et 6 unités derrière le nez du vaisseau, quelle que soit la direction où il pointe.Le vaisseau tourne à gauche → la caméra pivote avec lui pour rester dans son dos.C'est le comportement attendu pour un jeu de vaisseau/voiture en troisième personne.
															// false : l'offset est ajouté tel quel, en axes MONDE, sans jamais être tourné par l'orientation de la cible.La caméra garde un déplacement fixe par rapport aux axes du monde — par exemple toujours « 2 units en Y, -6 en Z » depuis la position de la cible, peu importe où elle regarde.Le vaisseau tourne sur lui - même → la caméra ne suit pas ce pivot, elle continue de le regarder depuis le même angle absolu.Utile pour une vue plus « stratégique » / isométrique qui ne doit pas tourner avec l'objet suivi.


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

		const ETriggerRole role = ReadTriggerRole(r, "role", owner);

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
			role,
			std::move(onEnterEvent),	// std::move : les strings locales ne servent plus après, autant les céder
			std::move(onStayEvent),
			std::move(onExitEvent),
			false,						// is_colliding
			std::vector<Entity>{}
		);

		// todo : ajouter emplaceComponent là où cela est nécessaire pour les autres parsing de composants

		#if LV3_VERBOSE_LOG
			Logger::info("[Trigger] " + owner + " : rayon " + std::to_string(radius) + ", role  = ETriggerRole::" + std::to_string(static_cast<int>(role)));
		#endif
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


	//bool SceneSerializer::ParseHierarchy(const void* pJsonNode, ParseContext& ctx)
	//{
	//	const nlo_json& nodeJson = *static_cast<const nlo_json*>(pJsonNode);
	//	if (!nodeJson.is_object()) return false;

	//	if (nodeJson.contains("parent"))
	//	{
	//		const std::string childId = nodeJson["id"];
	//		const std::string parentId = nodeJson["parent"];

	//		// R28 : operator[] d'une map n'est JAMAIS un lookup — il insère.
	//		// Ici, un parent mal orthographié fabriquait Entity(0) : l'objet
	//		// devenait enfant du PREMIER noeud de la scène, sans un mot.
	//		const auto itChild = ctx.entityMap.find(childId);
	//		const auto itParent = ctx.entityMap.find(parentId);
	//		LV3_ASSERT(itChild != ctx.entityMap.end());   // créé en passe 1, sinon bug interne

	//		if (itParent == ctx.entityMap.end())
	//		{
	//			Logger::error("ParseHierarchy — parent '" + parentId + "' introuvable pour '" + childId + "'");
	//			return false;      // un graphe faux ne se charge pas « presque bien »
	//		}

	//		linkChildToParent(ctx.registry, itChild->second, itParent->second);
	//	}
	//	return true;
	//}
	
	// Passe 2 : aucune lecture JSON. Les liens ont ete LUS par ParseNode.
	bool SceneSerializer::ResolveParents(ParseContext& ctx)
	{
		for (const PendingParentLink& link : ctx.pendingParents)
		{
			// R28 : find(), jamais operator[] (un parent mal orthographie
			// fabriquait Entity(0) : enfant du PREMIER noeud, sans un mot).
			const auto itParent = ctx.entityMap.find(link.parentId);
			if (itParent == ctx.entityMap.end())
			{
				Logger::error("ResolveParents — parent '" + link.parentId + "' introuvable pour "
					+ EntityLabel(ctx.registry, link.child));
				return false;      // un graphe faux ne se charge pas « presque bien »
			}
			linkChildToParent(ctx.registry, link.child, itParent->second);
		}
		ctx.pendingParents.clear();
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