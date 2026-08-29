#include "pch.h"          // ? première ligne, toujours
#include <map>	// pour le debug
#include <set>

#include "Registry.hpp"
#include "../Core/EventBus.hpp"
#include "../Ressources/ResourceManager.h"
#include "System.hpp"
#include "../Core/Logger.h"


//#pragma message("=== Transform.h lu depuis : " __FILE__ " ===")




/*
todo

* refonte  de TriggerSystem : EnTT utilise un enum class entity : uint32_t précisément pour interdire ces conversions implicites. Ce serait une amélioration de fond — mais pas maintenant : ça touche toute la signature de ton Registry. Note-la comme chantier futur, à côté de la refonte du TriggerSystem.



*/

namespace LV3
{

	/// <summary>
	/// Met à jour les angles d'orbite et de rotation des composants Mesh dans le registre en fonction du temps écoulé.
	/// </summary>
	/// <param name="registry">Le registre contenant les entités et leurs composants.</param>
	/// <param name="deltaTime">Temps écoulé (en secondes) depuis la dernière mise à jour, utilisé pour incrémenter les angles selon les vitesses orbitales et de rotation.</param>
	/// Anime les meshes : rotation propre et orbite.
	/// Fait avancer les angles ET les applique au Transform.
	/// Comme tout contrôleur, écrit dans m_local — jamais dans les matrices.
	void AnimationSystem(Registry& registry, float deltaTime)
	{
		constexpr float TWO_PI = 6.28318530718f;

		// On exige aussi le Transform : un mesh sans Transform n'est pas animable.
		for (auto&& [entity, mesh, tr] : registry.ViewGroup<MeshComponent, TransformComponent>())
		{
			bool changed = false;

			// --- Rotation propre ---
			if (mesh.m_rotationSpeed != 0.0f)	// Un mesh sans animation ne doit pas voir son Transform réécrit ni son m_dirty levé — sinon LocalTransformSystem recompose sa matrice à chaque frame pour rien. C'est ce qui donne son sens au m_dirty.
			{
				mesh.m_currentRotationAngle += mesh.m_rotationSpeed * deltaTime;
				mesh.m_currentRotationAngle = std::fmod(mesh.m_currentRotationAngle, TWO_PI);

				Quatf spin;
				spin.SetAxisAngle(Vec3f::Up(), mesh.m_currentRotationAngle);

				// On repart TOUJOURS de la rotation d'auteur, jamais de la précédente :
				// composer sur soi-même accumule les erreurs d'arrondi et dérive.
				//tr.m_local.rotation = spin * tr.m_initialRotation;	// inclinaison puis rotation autour de Y MONDE  ->  précession
				tr.m_local.rotation = tr.m_initialRotation * spin;		// rotation propre puis inclinaison  ->  axe fixe
				changed = true;
			}

			// --- Orbite, dans le plan XZ ---
			if (mesh.m_orbitalSpeed != 0.0f && mesh.m_orbitRadius > 0.0f)
			{
				mesh.m_currentOrbitAngle += mesh.m_orbitalSpeed * deltaTime;
				mesh.m_currentOrbitAngle = std::fmod(mesh.m_currentOrbitAngle, TWO_PI); // std::fmod(angle, 2π). Sans ça, après quelques heures de jeu ton angle atteint des milliers de radians et la précision du float s'effondre : cos() et sin() deviennent visiblement saccadés. Un fmod par frame coûte trois fois rien.

				tr.m_local.position.x = mesh.m_orbitRadius * std::cos(mesh.m_currentOrbitAngle);
				tr.m_local.position.z = mesh.m_orbitRadius * std::sin(mesh.m_currentOrbitAngle);
				// y est laissé intact : l'orbite est plane, l'inclinaison reste celle du JSON
				changed = true;
			}
			
			if (changed) tr.m_dirty = true;
		}
	}

	//********************************************************************
	/// <summary>
	/// Met à jour la matrice locale de chaque TransformComponent présent dans le registry : construit une matrice locale à partir des données initiales (position, rotation, échelle), puis laisse les composants animés (par ex. MeshComponent, CameraComponent) écraser la rotation ou la position si nécessaire (gestion d'orbite, rotation animée, position/rotation lissées pour la caméra).
	/// </summary>
	/// <param name="registry">Référence au Registry qui contient les composants. </param>
	void LocalTransformSystem(Registry& registry)
	{
		SparseSet<TransformComponent>* pool = registry.getStorage<TransformComponent>();
		if (!pool) return;

		for (TransformComponent& tr : pool->GetDenseData())
		{
			if (!tr.m_dirty) continue;

			tr.m_localMatrix = tr.m_local.ToLocalMatrix();   // S · R · T
			tr.m_dirty = false;
		}
	}



	//********************************************************************
	// Descente récursive : chaque nœud est visité UNE fois.
	void PropagateWorld(Registry& reg, Entity e, const Matrix44f& parentWorld)
	{
		TransformComponent* tr = reg.TryGet<TransformComponent>(e);
		if (!tr) return;

		// Vecteur-ligne : v · M_enfant · M_parent  ->  enfant À GAUCHE
		tr->m_worldMatrix = tr->m_localMatrix * parentWorld;

		// COPIE avant de récurser : `tr` peut pendre si le stockage bouge.
		const Matrix44f world = tr->m_worldMatrix;

		if (const HierarchyComponent* h = reg.TryGet<HierarchyComponent>(e))
			for (Entity child : h->m_children)
				PropagateWorld(reg, child, world);
	}


	/// Met à jour la transformation mondiale d'une entité en composant sa transformation locale avec la transformation mondiale du parent, puis applique la mise à jour de manière récursive à ses enfants.
	/// Propage les matrices monde depuis les racines. Une seule passe, O(n).
	/// Exige que LocalTransformSystem ait déjà mis m_localMatrix à jour.
	void WorldTransformSystem(Registry& registry)
	{
		const Matrix44f identity = Matrix44f::Identity();

		for (auto&& [entity, hierarchy] : registry.ViewGroup<HierarchyComponent>())
			if (hierarchy.m_isRoot)
				PropagateWorld(registry, entity, identity);
	}
	//********************************************************************

	Entity FindActiveCamera(Registry& registry)
	{
		Entity best = NULL_ENTITY;
		int    bestPriority = std::numeric_limits<int>::min();

		for (auto&& [entity, cam] : registry.ViewGroup<CameraComponent>())
		{
			if (!cam.m_isActive) continue;
			if (cam.m_priority > bestPriority) { bestPriority = cam.m_priority; best = entity; }
		}
		return best;
	}

	//********************************************************************

	Entity FindCameraByName(Registry& registry, const std::string& name)
	{
		for (auto&& [entity, cam] : registry.ViewGroup<CameraComponent>())
		{
			const NameComponent* n = registry.TryGet<NameComponent>(entity);
			if (n && n->m_id == name) return entity;
		}
		return NULL_ENTITY;
	}
	//********************************************************************
	/*
	todo : Dansle cadre d'une shadow maps, tu voudras un ViewData pour une lumière. Or une lumière n'a ni TransformComponent de caméra, ni CameraComponent : elle a une matrice monde et une focale. Tu devras alors extraire le cœur pur :	
	*/
	ViewData BuildViewData(const Registry& registry, const CameraBinding& b) noexcept
	{

		const auto& tr = registry.getComponent<TransformComponent>(b.m_camera);
		const auto& cam = registry.getComponent<CameraComponent>(b.m_camera);

		ViewData v;
		
		// ════════════════════════════════════════════════════════════════
		//  ÉTAPE 0 — Contexte
		//  Ce qui ne se calcule pas : on recopie ce qu'on nous donne.
		// ════════════════════════════════════════════════════════════════
		v.m_sourceCamera = b.m_camera;		// ETAPE 0, avec le reste du contexte
		v.viewport = b.m_viewport;			// la destination en pixels (et l'aspect ratio)
		v.mode = b.m_mode;				// Mode de rendu pour la viewport
		v.reverseZ = true;					// convention du moteur, mémorisée pour le Z-buffer

		LV3_ASSERT(b.m_viewport.width > 0 && b.m_viewport.height > 0);
		const float aspect = v.viewport.Aspect();   // une variable locale, lue une fois
		LV3_ASSERT(std::isfinite(aspect) && aspect > 0.0f);

		v.nearPlane = cam.m_nearPlane;

		LV3_ASSERT(cam.m_infiniteFar || std::isfinite(v.farPlane));
		//v.farPlane = cam.m_infiniteFar ? std::numeric_limits<float>::infinity() : cam.m_farPlane;
		v.hasFarPlane = !cam.m_infiniteFar;
		v.farPlane = cam.m_farPlane;      // sans objet si hasFarPlane == false

		LV3_ASSERT(!(cam.m_projection == EProjectionType::Orthographic && cam.m_infiniteFar));

		// ════════════════════════════════════════════════════════════════
		//  ÉTAPE 1 — LA MATRICE VIEW (Monde → Vue)
		//
		//  Ancien équivalent : Camera::SetViewMatrix() (qui calculait viewMatrix puis (rotation * viewmatrix).inverse
		//
		//  La View est l'INVERSE de la matrice monde de la caméra : on ne
		//  déplace pas l'œil, on déplace la scène en sens inverse.
		//  C'est une isométrie -> inverse analytique, jamais Gauss-Jordan.
		// ════════════════════════════════════════════════════════════════
		const Matrix44f& world = tr.m_worldMatrix;
		v.viewMatrix = world.inverseRigid();

		//  ── Données annexes extraites de la même matrice ──
		//  Elles ne servent PAS à projeter, mais au rendu :
		//    * position -> éclairage spéculaire, tri par distance, LOD
		//    * forward  -> brouillard directionnel, debug
		v.position = { world[3][0],  world[3][1],  world[3][2] };  // translation = LIGNE 3
		v.forward = { -world[2][0], -world[2][1], -world[2][2] };  // main droite : avant = -Z


		// ════════════════════════════════════════════════════════════════
		//  ÉTAPE 2 — LA MATRICE PROJECTION        (Vue → Clip)
		//
		//  Ancien équivalent : Frustum::setProjectionMatrixV2() (qui était calculé par "getProjectionMatrix()")
		//
		//  2a. Résoudre le FOV vertical : soit donné, soit dérivé du sténopé
		// ════════════════════════════════════════════════════════════════
		//const float fovY = (cam.m_lensModel == ELensModel::Filmback)
		//	? Projection::FovYFromFocal(cam.m_focalLengthMm, cam.m_filmHeightMm)  // focale + pellicule
		//	: cam.m_fovYDeg * TO_RADIAN;                                          // angle direct

		const float fovY = CameraFovY(cam);

		//  2b. Choisir la fabrique. L'ASPECT vient du VIEWPORT, jamais de la lentille.
		if (cam.m_projection == EProjectionType::Orthographic)
			v.projectionMatrix = Projection::OrthographicCentered(cam.m_orthoHeight, aspect,
				cam.m_nearPlane, cam.m_farPlane);
		else if (cam.m_infiniteFar)
			v.projectionMatrix = Projection::PerspectiveInfinite(fovY, aspect, cam.m_nearPlane);
		else
			v.projectionMatrix = Projection::Perspective(fovY, aspect,
				cam.m_nearPlane, cam.m_farPlane);


		// ════════════════════════════════════════════════════════════════
		//  ÉTAPE 3 — LA MATRICE VIEW·PROJECTION   (Monde → Clip)
		//
		//  Identique à ton ancien code.
		//  /!\ En convention VECTEUR-LIGNE, A*B signifie « A puis B ».
		//      Un sommet subit la View, PUIS la Projection : view * projection.
		//      L'ordre inverse est le bug B1 de l'audit.
		// ════════════════════════════════════════════════════════════════
		v.viewProjectionMatrix = v.viewMatrix * v.projectionMatrix;


		// ════════════════════════════════════════════════════════════════
		//  ÉTAPE 4 — LES 6 PLANS DU FRUSTUM       (espace MONDE)
		//
		//  Ancien équivalent : Frustum_ExtractPlan() + NormalizeFrustumPlane()
		//
		//  Extraction Gribb-Hartmann. La matrice source décide de l'espace :
		//      P       -> plans en espace VUE
		//      V·P     -> plans en espace MONDE   ← ici
		//      M·V·P   -> plans en espace OBJET   (optimisation future)
		// ════════════════════════════════════════════════════════════════
		v.frustum.Build(v.viewProjectionMatrix,
						/* reverseZ    */ true,               // échange les étiquettes Near/Far
						/* infiniteFar */ cam.m_infiniteFar); // 5 plans au lieu de 6


		// ════════════════════════════════════════════════════════════════
		//  PAS D'ÉTAPE 5 ICI.
		//
		//  invViewProjection n'est PAS calculée : c'est le seul calcul
		//  coûteux (Gauss-Jordan 4x4) et il ne sert qu'au picking.
		//  ViewData::InvViewProjection() la produira à la demande.
		// 
		// Dans la boucle de rendu, PAR MESH :
		// const Matrix44f mvp = tr.m_worldMatrix * view.viewProjection;
		//                       └─ Model ─────┘   └─ View · Projection ─┘
		// 
		// 
		// ════════════════════════════════════════════════════════════════
	

//#ifdef _DEBUG
//	//	static bool logged = false;
//	//	if (!logged)
//		{
//	//		logged = true;
//			Logger::log("[VIEW] infiniteFar=" + std::to_string(cam.m_infiniteFar)
//				+ "  m22=" + std::to_string(v.projectionMatrix[2][2])
//				+ "  m32=" + std::to_string(v.projectionMatrix[3][2])
//				+ "  planes=" + std::to_string(v.frustum.Count()));
//		}
//#endif



		return v;
	}


	//********************************************************************
	void CameraFollowSystem(Registry& registry, float deltaTime)
	{
		for (auto&& [entity, follow, tr] : registry.ViewGroup<CameraFollowComponent,
			TransformComponent>())
		{
			if (!follow.m_isEnabled)               continue;
			if (follow.m_target == NULL_ENTITY)    continue;      // /!\ pas Entity{} : 0 est valide

			const TransformComponent* tgt = registry.TryGet<TransformComponent>(follow.m_target);
			if (!tgt) { follow.m_isEnabled = false; continue; }   // cible détruite : on s'arrête net

			// --- 1. Position et orientation VOULUES --------------------------
			const Vec3f targetPos{ tgt->m_worldMatrix[3][0],
								   tgt->m_worldMatrix[3][1],
								   tgt->m_worldMatrix[3][2] };

			const Vec3f wantedPos = follow.m_followRotation
				? targetPos + tgt->m_local.rotation.rotate(follow.m_offset)  // reste derrière la cible
				: targetPos + follow.m_offset;                               // direction monde fixe

			const Vec3f lookAt = targetPos + Vec3f(0.0f, follow.m_lookAtHeight, 0.0f);
			const Quatf wantedRot = Quatf::LookAt(wantedPos, lookAt);

			// --- 2. Lissage --------------------------------------------------
			if (!follow.m_isInitialized)
			{
				follow.m_smoothedPos = wantedPos;      // snap à la première frame
				follow.m_smoothedRot = wantedRot;      // (et après chaque bascule de mode)
				follow.m_isInitialized = true;
			}
			else if (follow.m_smoothSpeed <= 0.0f)
			{
				follow.m_smoothedPos = wantedPos;        // 0 = suivi rigide
				follow.m_smoothedRot = wantedRot;
			}
			else
			{
				// Lissage exponentiel INDÉPENDANT du framerate.
				// Un simple lerp(a, b, speed*dt) donne un comportement
				// différent à 30 et à 144 Hz — et diverge si speed*dt > 1.
				const float t = 1.0f - std::exp(-follow.m_smoothSpeed * deltaTime);
				follow.m_smoothedPos = Vec3f::Lerp(follow.m_smoothedPos, wantedPos, t);
				follow.m_smoothedRot = Slerp(follow.m_smoothedRot, wantedRot, t);
			}

			// --- 3. Écriture -------------------------------------------------
			tr.m_local.position = follow.m_smoothedPos;
			tr.m_local.rotation = follow.m_smoothedRot;
			tr.m_dirty = true;
		}
	}

	//********************************************************************
	void FPSControllerSystem(Registry& registry, const InputState& in, float dt)
	{
		for (auto&& [entity, ctrl, tr] : registry.ViewGroup<FPSControllerComponent,
			TransformComponent>())
		{
			if (!ctrl.m_isEnabled) continue;

			// --- 1. ORIENTATION ---------------------------------------------
			//  Le delta souris est un DÉPLACEMENT déjà accompli, en pixels.
			//  Ne JAMAIS le multiplier par dt : la sensibilité deviendrait
			//  dépendante du framerate. (bug B6 de l'audit du legacy)
			ctrl.m_yawDeg -= static_cast<float>(in.mouseDeltaX) * ctrl.m_mouseSensitivity;
			ctrl.m_pitchDeg -= static_cast<float>(in.mouseDeltaY) * ctrl.m_mouseSensitivity;

			ctrl.m_yawDeg = std::fmod(ctrl.m_yawDeg, 360.0f);          // évite la dérive de précision
			ctrl.m_pitchDeg = std::clamp(ctrl.m_pitchDeg,
				-ctrl.m_pitchLimitDeg,
				ctrl.m_pitchLimitDeg);

			Quatf qYaw;   qYaw.SetAxisAngle(Vec3f::Up(), ctrl.m_yawDeg * TO_RADIAN);
			Quatf qPitch; qPitch.SetAxisAngle(Vec3f::Right(), ctrl.m_pitchDeg * TO_RADIAN);
			const Quatf rot = qYaw * qPitch;      // yaw MONDE puis pitch LOCAL — cet ordre, pas l'autre

			// --- 2. DÉPLACEMENT ---------------------------------------------
			//  Lui, en revanche, EST une vitesse : multiplié par dt.
			const Vec3f fwd = rot.rotate(Vec3f::Forward());
			const Vec3f right = rot.rotate(Vec3f::Right());

			Vec3f dir;
			if (in.moveForward)  dir += fwd;
			if (in.moveBackward) dir -= fwd;
			if (in.strafeRight)  dir += right;
			if (in.strafeLeft)   dir -= right;

			if (ctrl.m_lockVertical)
			{
				dir.y = 0.0f;                     // FPS au sol : POLITIQUE du contrôleur,
			}                                     // jamais câblée dans la caméra
			else
			{
				if (in.moveUp)   dir.y += 1.0f;   // vol libre 6 DoF
				if (in.moveDown) dir.y -= 1.0f;
			}

			if (dir.norm() > 0.0f)                // normalise : la diagonale ne doit pas être plus rapide
			{
				const float speed = ctrl.m_moveSpeed
					* (in.sprint ? ctrl.m_sprintMultiplier : 1.0f);
				dir = dir.Normalized() * (speed * dt);
				tr.m_local.position += dir;
			}

			// --- 3. Écriture. Un contrôleur ne fait QUE ça. ------------------
			tr.m_local.rotation = rot;
			tr.m_dirty = true;
		}
	}


	//********************************************************************

	// doit s'exécuter en dernier, après que toutes les worldTransform finales ont été calculées.

	/*
	TODO : optimiser la détection de collision naïve O(N²) en utilisant une broad-phase spatiale (grille, quadtree, etc.) pour réduire le nombre de comparaisons.
	la boucle N² imbriquée(for entity1 ... for entity2 ...) construit - elle deux fois la vue à chaque itération externe, comme je le redoutais dans l'audit initial ? Non — regarde bien : la boucle externe for (auto&& [entity1, ...] : registry.ViewGroup<...>()) crée une vue, et la boucle interne en crée une seconde, mais celle-ci est construite une fois par entité externe, pas par paire. C'est du O(N²) en nombre de comparaisons(normal et attendu pour une détection de collision naïve), mais chaque construction de ComponentView interne est O(K) pour trouver le pivot(K = nombre de types, ici 2) — négligeable comparé au travail de la boucle elle - même.Ce n'est pas le C5d de l'audit initial dans toute sa gravité; le vrai gain serait de passer à une broad - phase spatiale plus tard(grille, quadtree), mais ça sort du cadre de F6 — c'est un chantier d'optimisation algorithmique à part entière, pas une hygiène de code.

	Le coût algorithmique est mal placé. Une détection de collision en O(N²) n'est pas fautive en soi — même Unity fait du N² à petite échelle. Le vrai problème, c'est qu'aucune étape de tri grossier ne précède le test précis. Chaque paire d'entités, même à l'autre bout de la scène, subit le calcul complet de distance et de rayon combiné. Un moteur professionnel sépare toujours en deux phases : une broad-phase rapide et approximative qui élimine 95 % des paires impossibles (grille spatiale, quadtree, ou même un simple tri par axe), puis une narrow-phase précise (ton test sphère-sphère actuel) qui ne s'applique qu'aux survivants. Tu as la seconde, pas la première.

	Le double parcours recrée deux fois le même travail de filtrage. La boucle interne reconstruit une vue ViewGroup<TriggerComponent, TransformComponent> identique à la boucle externe — le pivot est recalculé, mais surtout le contenu est le même ensemble d'entités. Rien ne t'empêcherait de matérialiser ce contenu une seule fois (positions + rayons dans un std::vector plat) avant la double boucle, ce qui coûte une seule passe de filtrage au lieu de N.

	La sémantique événementielle est mélangée à la détection géométrique. Ta boucle fait trois métiers à la fois : trouver les collisions, comparer à l'état précédent, publier des événements. C'est lisible aujourd'hui parce que le système est petit, mais le jour où tu voudras des triggers asymétriques (un trigger qui ne réagit qu'à certains tags, par exemple des ennemis mais pas des astéroïdes), cette fonction devra être réécrite en profondeur plutôt qu'étendue.

	Où ça mène, sans s'y engager aujourd'hui

	Le design cible ressemblerait à ceci : une passe de collecte (entity, position, radius) dans un vecteur local — profitant au passage du fait que ce vecteur serait contigu et cache-friendly — suivie d'un partitionnement spatial (une grille uniforme suffit largement à ton échelle, pas besoin d'un quadtree hiérarchique pour une scène de jeu simple), puis la narrow-phase uniquement sur les paires dans la même cellule ou des cellules voisines. La logique ON_ENTER/ON_STAY/ON_EXIT resterait identique — c'est une bonne nouvelle, cette partie de ton code est déjà propre et n'a pas besoin d'être repensée.

	*/

	void TriggerSystem(Registry& registry, EventBus& eventBus)
	{
		for (auto&& [entity1, trigger1, transform1] : registry.ViewGroup<TriggerComponent, TransformComponent>())
		{
			Vec3f pos1 = Vec3f{ transform1.m_worldMatrix[3][0],
			transform1.m_worldMatrix[3][1],
			transform1.m_worldMatrix[3][2] };

			// 1. Préparer la liste des collisions de CETTE frame
			std::set<Entity> newOverlaps;

			// 2. Boucle N*N pour trouver les collisions

			for (auto&& [entity2, trigger2, transform2] : registry.ViewGroup<TriggerComponent, TransformComponent>())
			{
				if (entity1 == entity2) continue;

				Vec3f pos2 = Vec3f{ transform2.m_worldMatrix[3][0],
								transform2.m_worldMatrix[3][1],
								transform2.m_worldMatrix[3][2] };

				// Test de collision Sphère vs Sphère
				// --- LE TEST DE COLLISION (Sphère vs Sphère) ---
				float distance = (pos1 - pos2).length(); // 	glm::distance(pos1, pos2);
				float combined_radius = trigger1.radius + trigger2.radius;


				if (distance < combined_radius)
				{
					newOverlaps.insert(entity2); // On touche e2
				}
			}

			// 3. Comparer l'état actuel (newOverlaps) avec l'état précédent (trigger1.overlapping_entities)
			auto& oldOverlaps = trigger1.overlapping_entities;

			// --- Logique ON_ENTER et ON_STAY ---
			for (const Entity& newEntity : newOverlaps) {
				if (oldOverlaps.count(newEntity)) {
					// Était déjà là -> ON_STAY
					if (!trigger1.onStayEvent.empty()) {
						std::cout << "[TriggerSystem] PUBLICATION DE L'EVENEMENT: " << trigger1.onStayEvent << std::endl;
						eventBus.publish(trigger1.onStayEvent, entity1, newEntity);
					}
				}
				else {
					// Vient d'arriver -> ON_ENTER
					if (!trigger1.onEnterEvent.empty()) {
						std::cout << "[TriggerSystem] PUBLICATION DE L'EVENEMENT: " << trigger1.onEnterEvent << std::endl;
						eventBus.publish(trigger1.onEnterEvent, entity1, newEntity);
					}
				}
			}

			// --- Logique ON_EXIT ---
			for (const Entity& oldEntity : oldOverlaps) {
				if (!newOverlaps.count(oldEntity)) {
					// Était là, mais n'y est plus -> ON_EXIT
					if (!trigger1.onExitEvent.empty()) {
						std::cout << "[TriggerSystem] PUBLICATION DE L'EVENEMENT: " << trigger1.onExitEvent << std::endl;
						eventBus.publish(trigger1.onExitEvent, entity1, oldEntity);
					}
				}
			}

			// 4. Mettre à jour l'état pour la prochaine frame
			trigger1.overlapping_entities = newOverlaps;
		}

		return;
	}

	//********************************************************************

	void DebugDisplaySystemRecursive(Registry& registry, Entity entity, int ident)
	{
		// Vérifie que l'entité a les composants de base pour s'afficher
		if (!registry.hasComponent<TransformComponent>(entity) && !registry.hasComponent<NameComponent>(entity))
			return;

		for (int i = 0; i < ident; i++)
			std::cout << "  ";

		auto& transform = registry.getComponent<TransformComponent>(entity);
		auto& name = registry.getComponent<NameComponent>(entity);

		// Lecture de la worldMatrix (qui a déjà été calculée par WorldTransformSystem)
		Vec3f worldPosition = Vec3f(transform.m_worldMatrix[3][0], transform.m_worldMatrix[3][1], transform.m_worldMatrix[3][2]);

		std::cout << " - " << name.m_id
			<< " [Pos : " << worldPosition.x << ", " << worldPosition.y << ", " << worldPosition.z << "]" << std::endl;

		if (registry.hasComponent<HierarchyComponent>(entity))
		{
			auto& chidren = registry.getComponent<HierarchyComponent>(entity).m_children;
			for (Entity child : chidren)
			{
				DebugDisplaySystemRecursive(registry, child, ident + 1);
			}
		}


	}

	void DebugDisplaySystem(Registry& registry)//, std::map<Entity, std::string>& name)
	{
		registry.ForEachAlive([&](Entity entity)
			{
				if (registry.hasComponent<TransformComponent>(entity))
				{
					if (!registry.hasComponent<HierarchyComponent>(entity)
						|| registry.getComponent<HierarchyComponent>(entity).m_isRoot)
					{
						DebugDisplaySystemRecursive(registry, entity, 0);
					}
				}
			});


	}

	
	void RenderSystem(Registry& registry, Entity activeCamera, ResourceManager& resourceManager)
	{



		// 1. Obtenir la matrice de Vue
		 
		// TODO: calculer la matrice de vue via l'inverse de activeCamera.worldTransform
			//Matrix44f viewMatrix(1.0f);
			//if (registry.hasComponent<TransformComponent>(activeCamera)) {
			//	viewMatrix = glm::inverse(registry.getComponent<TransformComponent>(activeCamera).worldTransform);
			//}		

		// (Ici, calculer la matrice de Projection...)
		// ...

		std::cout << std::endl;

		// 2. Itération linéaire sur tous les maillages
		SparseSet<TransformComponent>* TransformComponentsPool = registry.getStorage<TransformComponent>();
		auto& Transforms = TransformComponentsPool->GetDenseData();
		auto& Entities = TransformComponentsPool->GetDenseEntities();

		int ident = 0;
		for (size_t tr = 0; tr < Transforms.size(); tr++)
		{
			if (registry.hasComponent<MeshComponent>(Entities[tr]))
			{
				auto& transform = Transforms[tr];
				auto& name = registry.getComponent<NameComponent>(Entities[tr]);
				auto& meshComp = registry.getComponent<MeshComponent>(Entities[tr]);

				// Résolution du handle EN CE POINT PRÉCIS, jamais stockée ailleurs.
				// Le ResourceManager reste l'unique propriétaire : on obtient un pointeur d'observation, valable pour la durée de cette frame seulement.
				//  Recherche l'intérieur de la boucle, à chaque frame — ce n'est pas un gaspillage : c'est une recherche dans une unordered_map (O(1) amorti), et surtout c'est la garantie que si le mesh a été déchargé entre deux frames(UnloadMesh appelé ailleurs), le système le détecte immédiatement au lieu de déréférencer un pointeur mort.
				const MeshClass* mesh = resourceManager.GetMesh(meshComp.m_meshHandle);

				if (!mesh)	// GetMesh peut retourner nullptr 
				{
					// Handle invalide ou périmé (mesh déchargé entre-temps) : on ignore
					// cette entité plutôt que de déréférencer un pointeur nul.
					std::cout << " - " << name.m_id << " [mesh introuvable, handle invalide]" << std::endl;
					continue;
				}

				Vec3f worldPosition = Vec3f(transform.m_worldMatrix[3][0], transform.m_worldMatrix[3][1], transform.m_worldMatrix[3][2]);

				std::cout << " - " << name.m_id
					<< " [Pos : " << worldPosition.x << ", " << worldPosition.y << ", " << worldPosition.z << "]" << std::endl;


				// Appel réel du rendu, une fois le pointeur résolu :
					// monMoteur->dessiner(*mesh, transform.m_worldMatrix, viewMatrix);
					// Chaque SubMesh de *mesh porte déjà son propre MaterialHandle (submesh.material) —
					// résolu à son tour via resourceManager.GetMaterial(submesh.material) au moment du dessin.

			}


			if (registry.hasComponent<CameraComponent>(Entities[tr]))
			{
				//CameraComponent camera = registry.getComponent<CameraComponent>(el);
				auto& transform = Transforms[tr];
				auto& name = registry.getComponent<NameComponent>(Entities[tr]);


				Vec3f worldPosition = Vec3f(transform.m_worldMatrix[3][0], transform.m_worldMatrix[3][1], transform.m_worldMatrix[3][2]);
				std::cout << " - " << name.m_id
					<< " [Pos : " << worldPosition.x << ", " << worldPosition.y << ", " << worldPosition.z << "]" << std::endl;
			}

			ident++;
		}

	}



	//********************************************************************
	void PlayerInputSystem(Registry& registry, float deltaTime) {

		for (auto&& [entity, control, transform] : registry.ViewGroup<PlayerControlComponent, TransformComponent>()) 
		{
			// 'control' et 'transform' sont des références directes et MODIFIABLES
			// Aucune vérification 'hasComponent' ou 'getComponent' dans la boucle.
			// Itération dense et optimisée.
			transform.m_local.position.x += control.m_speed * deltaTime;
		}

	}

	//********************************************************************
	// Fonction utilitaire pour basculer entre le mode FPS et le mode caméra suivie à la volée. 
	// Active/désactive les composants FPSControllerComponent et CameraFollowComponent selon le mode choisi.
	// à appeler depuis le code de gestion d'input ou d'événement, par exemple lors d'un appui sur une touche.
	void SwitchCameraMode(Registry& reg, Entity cam, bool useFollow) noexcept
	{
		if (auto* fps = reg.TryGet<FPSControllerComponent>(cam))  fps->m_isEnabled = !useFollow;
		if (auto* follow = reg.TryGet<CameraFollowComponent>(cam))
		{
			follow->m_isEnabled = useFollow;
			follow->m_isInitialized = false;   // snap à la reprise, évite un vol plané
		}
	}

	// ============================================================
	//  TEST A — à appeler UNE SEULE FOIS, juste après le chargement
	//  de la scène et AVANT la boucle de jeu.
	//
	//  À dt = 0, l'animation ne doit RIEN modifier : la rotation
	//  courante doit être exactement la rotation d'auteur.
	//  Si ça échoue -> m_initialRotation n'a pas été renseigné
	//  dans ParseTransform.
	// ============================================================
	void CheckAnimationBaseline(Registry& registry)
	{
		AnimationSystem(registry, 0.0f);        // dt nul : rien ne doit bouger

		int failures = 0;
		for (auto&& [entity, mesh, tr] : registry.ViewGroup<MeshComponent, TransformComponent>())
		{
			// |Dot| == 1  <=>  meme rotation.
			// On prend la valeur absolue : q et -q representent la MEME rotation
			// (double recouvrement des quaternions).
			const float d = std::fabs(Dot(tr.m_local.rotation, tr.m_initialRotation));

			if (std::fabs(d - 1.0f) > 1e-5f)
			{
				++failures;
				Logger::error("\033[31m[BASELINE] " + registry.getComponent<NameComponent>(entity).m_id
					+ " : rotation != m_initialRotation  (Dot = " + std::to_string(d) + ")\033[0m");
			}
		}
		Logger::log("\033[32m[BASELINE] " + std::to_string(failures) + " echec(s).\033[0m");
		LV3_ASSERT(failures == 0);
	}

	// ============================================================
	//  INVARIANTS — à appeler CHAQUE frame, après WorldTransformSystem.
	//  Ne vérifie que des propriétés qui doivent tenir en permanence :
	//  aucune valeur attendue à calculer à la main.
	// ============================================================
	void CheckSceneInvariants(Registry& registry)
	{
		for (auto&& [entity, mesh, tr] : registry.ViewGroup<MeshComponent, TransformComponent>())
		{
			const std::string& name = registry.getComponent<NameComponent>(entity).m_id;

			// --- INVARIANT 1 : le quaternion reste unitaire ---------------
			//     Une derive ici = accumulation sur soi-meme quelque part.
			const Quatf& q = tr.m_local.rotation;
			const float  n = q.r * q.r + q.v.norm();
			if (std::fabs(n - 1.0f) > 1e-4f)
			{
				Logger::error("\033[31m[INVARIANT] " + name + " : quaternion non unitaire, |q|^2 = " + std::to_string(n) + "\033[0m");
				LV3_ASSERT(false);
			}

			// --- INVARIANT 2 : une orbite conserve son rayon dans le plan XZ
			if (mesh.m_orbitalSpeed != 0.0f)
			{
				if (mesh.m_orbitRadius <= 0.0f)
				{
					Logger::error("\033[31m[INVARIANT] " + name + " : vitesse orbitale sans rayon\033[0m");
					LV3_ASSERT(false);
				}
				else
				{
					const Vec3f& p = tr.m_local.position;
					const float  r = std::sqrt(p.x * p.x + p.z * p.z);
					if (std::fabs(r - mesh.m_orbitRadius) > 1e-3f)
					{
						Logger::error("\033[31m[INVARIANT] " + name + " : rayon derive  "
							+ std::to_string(r) + " au lieu de "
							+ std::to_string(mesh.m_orbitRadius) + "\033[0m");
						LV3_ASSERT(false);
					}
				}
			}

			// --- INVARIANT 3 : l'echelle n'est jamais nulle ---------------
			//     (le piege du sosie de Transform, avec scale = 0)
			if (tr.m_local.scale.norm() < 1e-6f)
			{
				Logger::error("\033[31m[INVARIANT] " + name + " : echelle nulle\033[0m");
				LV3_ASSERT(false);
			}
		}
	}


	// ============================================================
	//  EXCLUSIVITÉ DES CONTRÔLEURS — à appeler CHAQUE frame.
	//
	//  FPSControllerSystem et CameraFollowSystem écrivent tous les deux
	//  dans le MÊME TransformComponent (m_local.position / m_local.rotation).
	//  Si les deux sont actifs simultanément sur la même entité, celui qui
	//  s'exécute en second écrase le premier — SILENCIEUSEMENT, sans erreur
	//  ni warning, juste une caméra qui se comporte mal par intermittence.
	//
	//  L'exclusion était promise par le commentaire de
	//  CameraFollowComponent::m_isEnabled (Component.hpp) mais jamais
	//  vérifiée : bug n°25 du journal (L04 P2, §11). SwitchCameraMode()
	//  la respecte quand on passe par elle, mais rien n'empêche un appel
	//  direct à reg.TryGet<...>()->m_isEnabled = true des deux côtés.
	// Vérifie que le json ne paramètre pas 2 composants différents (FPSController et CameraFollow) pour une même caméra
	// LE JSON serait par exemple :
	// {
	// "id": "Camera_Cassee",
	//	"components" : {
	//	"Transform": { "translation": [0, 5, 10] , "rotation" : [0, 0, 0] , "scale" : [1, 1, 1] },
	//		"Camera" : { "projection": "perspective", "fov" : 45.0, "active" : true, "priority" : 1 },
	//		"CameraFPS" : { "enabled": true },
	//		"CameraFollow" : { "enabled": true, "target" : "Cube1" }
	//	}
	// }
	// ============================================================
	void CheckControllerExclusivity(Registry & registry)
	{
		for (auto&& [entity, fps] : registry.ViewGroup<FPSControllerComponent>())
		{
			if (!fps.m_isEnabled) continue;
			
				const CameraFollowComponent * follow = registry.TryGet<CameraFollowComponent>(entity);
			if (!follow || !follow->m_isEnabled) continue;
			
				const std::string name = registry.hasComponent<NameComponent>(entity)
					? registry.getComponent<NameComponent>(entity).m_id
					: std::string("<sans nom>");
			
				Logger::error("\033[31m[INVARIANT] " + name +
					" : FPSControllerComponent ET CameraFollowComponent actifs simultanement"
						" — le systeme execute en second ecrasera le Transform du premier\033[0m");
			LV3_ASSERT(false);
		}
	}
	
	// ============================================================
	//  TRACE — sonde manuelle sur une entité nommée.
	//  Sert à répondre à la question de l'ordre de composition :
	//  le pole doit-il rester fixe, ou tourner ?
	// ============================================================
	void DebugTraceEntity(Registry& registry, const std::string& name)
	{
		for (auto&& [entity, tr] : registry.ViewGroup<TransformComponent>())
		{
			const NameComponent* n = registry.TryGet<NameComponent>(entity);
			if (!n || n->m_id != name) continue;

			const Vec3f pole = tr.m_local.rotation.rotate(Vec3f::Up());
			const Vec3f world{ tr.m_worldMatrix[3][0], tr.m_worldMatrix[3][1], tr.m_worldMatrix[3][2] };

			std::cout << "  [TRACE " << name << "]"
				<< "  local(" << tr.m_local.position.x << ", "
				<< tr.m_local.position.y << ", "
				<< tr.m_local.position.z << ")"
				<< "  world(" << world.x << ", " << world.y << ", " << world.z << ")"
				<< "  pole(" << pole.x << ", " << pole.y << ", " << pole.z << ")\n";

			if (const MeshComponent* m = registry.TryGet<MeshComponent>(entity))
				std::cout << "                 R=" << m->m_orbitRadius
				<< "  orbit=" << m->m_currentOrbitAngle
				<< "  spin=" << m->m_currentRotationAngle << "\n";
			return;
		}
		Logger::warn("\033[31m[TRACE] entite '" + name + "' introuvable\033[0m");
	}

	// ============================================================

	//void CameraGizmoSystem(Registry& registry, Entity activeCamera, float aspect)
	//{
	//	for (auto&& [e, giz, tr, dbg] :
	//		registry.ViewGroup<CameraGizmoComponent, TransformComponent, DebugVisualComponent>())
	//	{
	//		const CameraComponent* cam = registry.TryGet<CameraComponent>(giz.m_owner);
	//		if (!cam) continue;

	//		// a) forme : derivee du fov REEL, chaque frame -> le zoom suit
	//		const float tanHalf = std::tan(CameraFovY(*cam) * 0.5f);
	//		const float L = giz.m_length;

	//		const Vec3f wanted{ L * tanHalf * aspect, L * tanHalf, L };

	//		if (std::fabs(wanted.x - tr.m_local.scale.x) > 1e-6f ||
	//			std::fabs(wanted.y - tr.m_local.scale.y) > 1e-6f ||
	//			std::fabs(wanted.z - tr.m_local.scale.z) > 1e-6f)
	//		{
	//			tr.m_local.scale = wanted;
	//			tr.m_dirty = true;      // le contrat avec LocalTransformSystem
	//		}

	//		// b) etat : la dissociation demandee
	//		dbg.m_color = (giz.m_owner == activeCamera)
	//			? Color{ 255, 216,  26 }    // ACTIVE  : ambre
	//		: Color{ 110, 112, 128 };   // inactive : gris froid
	//	}
	//}

	//void CameraGizmoSystem(Registry& registry, Entity activeCamera, float aspect, const GizmoAssets& assets)
	//{
	//	for (auto&& [e, giz, tr, mc, dbg] :
	//		registry.ViewGroup<CameraGizmoComponent, TransformComponent,
	//		MeshComponent, DebugVisualComponent>())
	//	{
	//		const CameraComponent* cam = registry.TryGet<CameraComponent>(giz.m_owner);
	//		if (!cam) continue;

	//		const float L = giz.m_length;
	//		Vec3f wanted;

	//		if (cam->m_projection == EProjectionType::Orthographic)
	//		{
	//			// Section CONSTANTE : Sx / Sy ne dependent PAS de L.
	//			const float halfH = cam->m_orthoHeight * 0.5f;
	//			wanted = { halfH * aspect, halfH, L };
	//		}
	//		else
	//		{
	//			// Section PROPORTIONNELLE a z : Sx / Sy sont multiplies par L.
	//			const float tanHalf = std::tan(CameraFovY(*cam) * 0.5f);
	//			wanted = { L * tanHalf * aspect, L * tanHalf, L };
	//		}

	//		if (std::fabs(wanted.x - tr.m_local.scale.x) > 1e-6f ||
	//			std::fabs(wanted.y - tr.m_local.scale.y) > 1e-6f ||
	//			std::fabs(wanted.z - tr.m_local.scale.z) > 1e-6f)
	//		{
	//			tr.m_local.scale = wanted;
	//			tr.m_dirty = true;
	//		}

	//		// Le type de projection peut changer a l'execution : le mesh suit.
	//		const MeshHandle want = assets.For(cam->m_projection);
	//		if (mc.m_meshHandle.id != want.id) mc.m_meshHandle = want;

	//		dbg.m_color = (giz.m_owner == activeCamera)
	//			? Color{ 255, 216,  26 }
	//		: Color{ 110, 112, 128 };
	//	}
	//}

} // namespace LV3