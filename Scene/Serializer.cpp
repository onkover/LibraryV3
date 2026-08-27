#include "pch.h"

#include <fstream>
#include "../Core/Logger.h"
#include "Serializer.hpp"
#include "Core/EventNames.h"
#include "Hierarchy.hpp"
#include "SerializerHelpers.hpp"

namespace LV3
{

	class JsonReader
	{
	public:
		JsonReader(const json& j, std::string comp, std::string owner) noexcept
			: m_j(j), m_comp(std::move(comp)), m_owner(std::move(owner)) {
		}

		template<typename T>
		[[nodiscard]] T Read(const char* key, T def)
		{
			m_seen.insert(key);                 // enregistré PARCE QU'on l'a lu
			return m_j.value(key, def);
		}

		[[nodiscard]] Vec3f ReadVector(const char* key, const Vec3f& def)
		{
			m_seen.insert(key);
			return LV3::ReadVec3(m_j, key, def);
		}

		[[nodiscard]] EProjectionType ReadProjectionType(const char* key)
		{
			m_seen.insert(key);
			return ReadProjection(m_j, key);
		}

		// Descente dans un sous-objet. NON const : elle consomme une cle.
		[[nodiscard]] JsonReader Child(const char* key)
		{
			m_seen.insert(key);            // "gizmo" est lue ICI, en tant que CONTENEUR

			// Objet vide de repli : duree de vie statique, donc jamais pendant.
			static const json s_empty = json::object();

			const auto it = m_j.find(key);
			const json& sub = (it != m_j.end() && it->is_object()) ? *it : s_empty;

			return JsonReader(sub, m_comp + "." + key, m_owner);
		}

		[[nodiscard]] bool Has(std::string_view key) const { return m_j.contains(key); }

		// À appeler en DERNIER : toute clé du JSON jamais passée par Read() est inconnue.
		void WarnUnread() const
		{
			for (auto& [key, _] : m_j.items())
			{
				if (key.starts_with('_')) continue;              // "_note" = commentaire assumé
				if (!m_seen.contains(key))
					Logger::warn("\033[31m[" + m_comp + "] cle ignoree '" + key + "' sur " + m_owner + "\033[0m");
			}
		}

	private:
		const json& m_j;
		std::string                m_comp, m_owner;
		std::set<std::string, std::less<>> m_seen;
	};
	/**********************************************

	Fin Helper

	**********************************************/



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

				Logger::log(id + " " + std::to_string(entityMap.size()) + " noeuds créés.");

			}
			Logger::log("Première passe terminée.");

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


			ResolveDeferredReferences(ctx);

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

		if (!nodeJson.contains("components")) return true;
		const json& comps = nodeJson["components"];

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
				Logger::warn("\033[31mComposant inconnu ignore : '" + compName + "' sur " + EntityLabel(ctx.registry, entity) + "\033[0m");
			}
		}
		Logger::log("\033[32mTous les composants ont été parsés.\033[0m");
		return true;

	}

	void SceneSerializer::ParseTransform(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{

		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;
		
		TransformComponent t;
		t.m_local.position = ReadVec3(compJson, "translation", Vec3f::Zero());
		t.m_local.scale = ReadVec3(compJson, "scale", Vec3f::One());

		// Le JSON stocke des DEGRES. Quat(v, true) fait la conversion lui-meme :
		// ne PAS la faire une seconde fois ici.
		const Vec3f eulerDeg = ReadVec3(compJson, "rotation", Vec3f::Zero());
		t.m_local.rotation = Quatf(eulerDeg, true);

		t.m_initialRotation = t.m_local.rotation;   // reference figee pour AnimationSystem
		t.m_dirty = true;

		ctx.registry.addComponent(entity, std::move(t));// Transforme la copie forcée (si on joint juste 't' en déplacement grace à std::move(t)
														 // TransformComponent est un POD pur (que des Vec3f / Matrix44f) — le gain est nul ici en pratique, mais l'uniformité du réflexe compte : on prend l'habitude de toujours céder une lvalue locale qu'on ne réutilise plus, sans se demander à chaque fois si le composant est « assez lourd » pour que ça vaille le coup. 
														 // Le compilateur ne te punira jamais pour un move inutile sur un POD.

	}
	void SceneSerializer::ParseMesh(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& j = *static_cast<const json*>(pJsonNode);
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

		auto meshResult = ctx.pRM.LoadMeshChecked(modelPath, opts);
		if (!meshResult.has_value())
		{
			const char* reason =
				meshResult.error() == EMeshLoadError::FileNotFound ? "fichier introuvable"
				: meshResult.error() == EMeshLoadError::ParseFailed ? "echec de parsing OBJ"
				: "mesh vide";
			Logger::error("\033[31mParseMesh — " + std::string(reason) + " : " + modelPath + "\033[0m");
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
			Logger::warn("\033[31mParseMesh : pas de Transform sur " + EntityLabel(ctx.registry, entity) + " — orbite desactivee\033[0m");
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

		static_assert(std::is_trivially_copyable_v<MeshComponent>,
			"MeshComponent doit rester un POD : pas de std::string ni de conteneur");

		r.WarnUnread();                                                    // ← 1 fois : fermeture

	}
	

	//***************************************************************************************
	void SceneSerializer::ParseLight(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;

		JsonReader r(compJson, "Light", EntityLabel(ctx.registry, entity));   // ← 1 fois : ouverture

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

		r.WarnUnread();

	}
	//********************************************************************

	//void SceneSerializer::ParseCamera(const void* pJsonNode, ParseContext& ctx, Entity entity, Entity& out_activeCamera)
	//{
	//	const json& j = *static_cast<const json*>(pJsonNode);
	//	if (!j.is_object()) return;

	//	JsonReader r(j, "Camera", EntityLabel(ctx.registry, entity));   // ← 1 fois : ouverture

	//	CameraComponent c;
	//	c.m_projection = r.ReadProjectionType("projection");
	//	c.m_nearPlane = r.Read("near", 0.1f);
	//	c.m_farPlane = r.Read("far", 1000.0f);
	//	c.m_infiniteFar = r.Read("infiniteFar", false);
	//	c.m_isActive = r.Read("active", true);
	//	c.m_priority = r.Read("priority", 0);

	//	// --- Perspective : FOV direct, ou modèle sténopé ---
	//	if (j.contains("focalLength"))
	//	{
	//		c.m_lensModel = r.Read("lensModel", 0) == 1 ? ELensModel::Filmback : ELensModel::FieldOfView;
	//		c.m_focalLengthMm = r.Read("focalLength", 35.0f);
	//		c.m_focalLengthMm = r.Read("focalLength", 35.0f);
	//		c.m_filmHeightMm = r.Read("filmHeight", 24.0f);
	//		//c.m_filmWidthMm = r.Read("filmWidth", 24.892f);
	//		//c.m_filmHeightMm = r.Read("filmHeight", 18.669f);
	//		c.m_infiniteFar = r.Read("infiniteFar", false);
	//		c.m_gateFit = (r.Read("gateFit", std::string("fill")) == "overscan")
	//			? EGateFit::Overscan : EGateFit::Fill;
	//	}
	//	else
	//	{
	//		c.m_lensModel = ELensModel::FieldOfView;
	//		c.m_fovYDeg = r.Read("fov", 45.0f);       // VERTICAL, en degrés
	//	}

	//	c.m_orthoHeight = r.Read("orthoHeight", 10.0f);


	//	if (r.Has("gizmo"))
	//	{
	//		JsonReader rg = r.Child("gizmo");             // "gizmo" marquee sur le parent
	//		c.m_gizmoLength = rg.Read("length", 2.0f);    // "length" marquee sur l'enfant
	//		rg.WarnUnread();                              // TNR du sous-objet
	//	}
	//	else
	//		c.m_gizmoLength = 0.0f;      // 0 = pas de gizmo



	//	// --- Garde-fous : une scène mal écrite ne doit pas casser le rendu ---
	//	if (c.m_nearPlane <= 0.0f)            c.m_nearPlane = 0.1f;
	//	if (c.m_farPlane <= c.m_nearPlane)   c.m_farPlane = c.m_nearPlane * 1000.0f;
	//	c.m_fovYDeg = std::clamp(c.m_fovYDeg, 1.0f, 179.0f);

	//	ctx.registry.addComponent(entity, c);
	//	r.WarnUnread();                                                    // ← 1 fois : fermeture

	//	// Sélection : la plus haute priorité gagne, pas "la dernière lue".
	//	if (c.m_isActive)
	//	{
	//		const CameraComponent* current = (out_activeCamera != NULL_ENTITY)
	//			? ctx.registry.TryGet<CameraComponent>(out_activeCamera)
	//			: nullptr;
	//		if (!current || c.m_priority >= current->m_priority)
	//			out_activeCamera = entity;
	//	}
	//	

	//	
	//}

	void SceneSerializer::ParseCamera(const void* pJsonNode, ParseContext& ctx, Entity entity, Entity& out_activeCamera)
	{
		const json& j = *static_cast<const json*>(pJsonNode);
		if (!j.is_object()) return;

		const std::string owner = EntityLabel(ctx.registry, entity);
		JsonReader r(j, "Camera", owner);

		CameraComponent c;

		// ── 1. PROJECTION : discriminant de premier niveau ─────────────
		c.m_projection = r.ReadProjectionType("projection");

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

		ctx.registry.addComponent(entity, c);
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
		const json& j = *static_cast<const json*>(pJsonNode);
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
		ctx.registry.addComponent(entity, c);

		r.WarnUnread();
	}

	//********************************************************************
	void SceneSerializer::ParseCameraFollow(const void* pJsonNode, ParseContext& ctx, Entity entity)
	{
		const json& j = *static_cast<const json*>(pJsonNode);
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
		ctx.registry.addComponent(entity, c);

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
		const json& compJson = *static_cast<const json*>(pJsonNode);
		if (!compJson.is_object()) return;


		// value() lit la clé si présente, sinon retourne le défaut fourni —
		// remplace élégamment tes if/else répétés
		const float radius = compJson.value("radius", 1.0f);

		std::string onEnterEvent = compJson.value("onEnterEvent", std::string{});
		if (!IsKnownEvent(onEnterEvent)) Logger::warn("\033[31mTrigger : evenement inconnu '" + onEnterEvent + "' — ne sera jamais recu\033[0m");

		std::string onStayEvent = compJson.value("onStayEvent", std::string{});
		if (!IsKnownEvent(onStayEvent)) Logger::warn("\033[31mTrigger : evenement inconnu '" + onStayEvent + "' — ne sera jamais recu\033[0m");

		std::string onExitEvent = compJson.value("onExitEvent", std::string{});
		if (!IsKnownEvent(onExitEvent)) Logger::warn("\033[31mTrigger : evenement inconnu '" + onExitEvent	 + "' — ne sera jamais recu\033[0m");

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
		Logger::warn("INFO: Entité : " + EntityLabel(ctx.registry, entity) + " a un trigger de rayon : ");
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
				ctx.registry.addComponent<HierarchyComponent>(rootEntity, HierarchyComponent{ NULL_ENTITY, {}, true });
				// todo : optimisation F1 :  Une fois F1 en place, ce sera NULL_ENTITY qu'il faudra écrire ici, pas {}
				// sinon un enfant sans parent explicite pointera silencieusement vers l'entité d'index 0 ({}, c'est-à-dire Entity{} soit 0)
			}
		}
		return true;
	}

	//void SceneSerializer::SpawnCameraGizmos(Registry& registry, ResourceManager& rm, const std::string gizmoMesh)
	//{
	//	auto result = rm.LoadMeshChecked(gizmoMesh, {});
	//	if (!result.has_value()) return;
	//	const MeshHandle hGizmo = *result;

	//	// 1. COLLECTER d'abord. Creer des entites pendant l'iteration
	//	//    d'un ViewGroup invalide les tableaux denses du SparseSet.
	//	std::vector<Entity> cameras;
	//	for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
	//		if (cam.m_gizmoLength > 0.0f) cameras.push_back(e);

	//	// 2. Creer ensuite.
	//	for (Entity cam : cameras)
	//	{
	//		Entity g = registry.CreateEntity();
	//		registry.addComponent(g, NameComponent{ "__gizmo" });
	//		registry.addComponent(g, TransformComponent{});
	//		registry.addComponent(g, MeshComponent{ hGizmo });
	//		registry.addComponent(g, CameraGizmoComponent{cam, registry.getComponent<CameraComponent>(cam).m_gizmoLength });
	//		registry.addComponent(g, DebugVisualComponent{ Color{}, cam });
	//		linkChildToParent(registry, g, cam);
	//	}
	//}

	//void SceneSerializer::SpawnCameraGizmos(Registry& registry, const GizmoAssets& assets)
	//{

	//	LV3_ASSERT(assets.m_perspective.IsValid() && assets.m_orthographic.IsValid());

	//	// 1. COLLECTER d'abord : creer des entites pendant l'iteration
	//	//    d'un ViewGroup invalide les tableaux denses du SparseSet.
	//	std::vector<Entity> cameras;
	//	for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
	//		if (cam.m_gizmoLength > 0.0f) cameras.push_back(e);

	//	// 2. Creer ensuite.
	//	for (Entity camEntity : cameras)
	//	{
	//		const CameraComponent& cam = registry.getComponent<CameraComponent>(camEntity);

	//		Entity g = registry.CreateEntity();
	//		registry.addComponent(g, NameComponent{"__gizmo(" + EntityLabel(registry, camEntity) + ")" });
	//		registry.addComponent(g, TransformComponent{});
	//		registry.addComponent(g, MeshComponent{ assets.For(cam.m_projection) });
	//		registry.addComponent(g, CameraGizmoComponent{ camEntity, cam.m_gizmoLength });
	//		registry.addComponent(g, DebugVisualComponent{ Color{}, camEntity });
	//		linkChildToParent(registry, g, camEntity);
	//	}
	//}




	

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