#include "pch.h"          // ? première ligne, toujours
#include <map>	// pour le debug
#include <set>

#include "Registry.hpp"
#include "../Core/EventBus.hpp"
#include "../Ressources/ResourceManager.h"


namespace LV3
{

	/// <summary>
	/// Met à jour les angles d'orbite et de rotation des composants Mesh dans le registre en fonction du temps écoulé.
	/// </summary>
	/// <param name="registry">Le registre contenant les entités et leurs composants.</param>
	/// <param name="deltaTime">Temps écoulé (en secondes) depuis la dernière mise à jour, utilisé pour incrémenter les angles selon les vitesses orbitales et de rotation.</param>
	void AnimationSystem(Registry& registry, float deltaTime)
	{
		// On utilise la ViewGroup (ComponentView) qui retourne (Entity, Component&...)
		for (auto&& [entity, mesh] : registry.ViewGroup<MeshComponent>())
		{
			mesh.m_currentOrbitAngle += mesh.m_orbitalSpeed * deltaTime;
			mesh.m_currentRotationAngle += mesh.m_rotationSpeed * deltaTime;
		}

	}

	//********************************************************************
	/// <summary>
	/// Met à jour la matrice locale de chaque TransformComponent présent dans le registry : construit une matrice locale à partir des données initiales (position, rotation, échelle), puis laisse les composants animés (par ex. MeshComponent, CameraComponent) écraser la rotation ou la position si nécessaire (gestion d'orbite, rotation animée, position/rotation lissées pour la caméra).
	/// </summary>
	/// <param name="registry">Référence au Registry qui contient les composants. </param>
	void LocalTransformSystem(Registry& registry)
	{
		SparseSet<TransformComponent>* TransformComponentsPool = registry.getStorage<TransformComponent>();
		auto& Transforms = TransformComponentsPool->GetDenseData();
		auto& Entities = TransformComponentsPool->GetDenseEntities();

		for (size_t i = 0; i < Transforms.size(); i++)
		{
			TransformComponent& transform = Transforms[i];
			Entity entity = Entities[i];

			// --- 1. CONSTRUIRE LA MATRICE LOCALE STATIQUE ---
			// On utilise les données lues du JSON (m_initial...)
			// C'est la transformation par défaut du composant
			Matrix44f scaleMatrix;
			scaleMatrix.scale(transform.m_initialLocalScale);

			Quatf RotationQuat = Quatf(transform.m_initialLocalRotation);

			Matrix44f translationMatrix;
			translationMatrix.translate(transform.m_initialLocalPosition);

			// --- 2. LAISSER LES COMPOSANTS ANIMÉS ÉCRASER CETTE MATRICE ---
			// Si un composant (comme MeshComponent) a une logique d'update, il va maintenant écraser m_localTransform avec sa propre logique d'animation.
			// Gestion de l'animation du mesh si elle existe
			if (registry.hasComponent<MeshComponent>(entity))
			{
				MeshComponent& mesh = registry.getComponent<MeshComponent>(entity);

				Quatf animatedRotation;
				animatedRotation.SetAxisAngle({ 0, 1, 0 }, mesh.m_currentRotationAngle);
				RotationQuat = animatedRotation * RotationQuat;
				// rotationQuat = glm::rotate(rotationQuat, glm::radians(mesh.m_currentRotationAngle), {0, 1, 0});

				Matrix44f AnimatedTranslationMatrix; // Pour l'orbite
				float radius = transform.m_initialLocalPosition.length();
				if (radius > 0.001f) {
					Vec3f orbitalPosition = transform.m_initialLocalPosition;
					orbitalPosition.x = radius * cosf(mesh.m_currentOrbitAngle);
					orbitalPosition.z = radius * sinf(mesh.m_currentOrbitAngle);

					AnimatedTranslationMatrix.translate(orbitalPosition);
					translationMatrix = AnimatedTranslationMatrix;
				}
			}

			// Si c'est une caméra lissée, elle écrase la position/rotation
			if (registry.hasComponent<CameraComponent>(entity))
			{
				CameraComponent& camera = registry.getComponent<CameraComponent>(entity);

				translationMatrix.translate(camera.currentSmoothedPos);
				RotationQuat = camera.currentSmoothedRot;

			}

			// --- 3. Composition et mise à jour de la matrice locale ---
			transform.m_localTransform = scaleMatrix * RotationQuat.ToMatrix44() * translationMatrix;

			//std::cout << "transform.m_localTransform : " << transform.m_localTransform << std::endl;
			//std::cout << "\n" << std::endl;
			//
			//std::cout << "raw angles: " << transform.m_initialLocalRotation
			//	<< " | quat: r=" << RotationQuat.r << " v=" << RotationQuat.v << std::endl;
			//std::cout << "\n" << std::endl;

		}


	}

	//********************************************************************

	/// <summary>
	/// Met à jour la transformation mondiale d'une entité en composant sa transformation locale avec la transformation mondiale du parent, puis applique la mise à jour de manière récursive à ses enfants.
	/// </summary>
	/// <param name="registry">Le Registry contenant les composants des entités</param>
	/// <param name="entity">L'identifiant de l'entité dont on met à jour la transformation mondiale.</param>
	/// <param name="parentWorldTransform">Référence constante à la transformation mondiale du parent utilisée pour composer la transformation mondiale de l'entité.</param>
	void UpdateWorldTransforms(Registry& registry, Entity entity, const Matrix44f& parentWorldTransform)
	{
		if (!registry.hasComponent<TransformComponent>(entity))
			return;

		auto& transform = registry.getComponent<TransformComponent>(entity);

		// --- 3. Composition et mise à jour de la matrice locale du propriétaire ---
		// Ordre Row-Major pour un effet SRT (Scale -> Rotate -> Translate)
		transform.m_worldTransform = transform.m_localTransform * parentWorldTransform;

		// --- 4. MISE À JOUR RÉCURSIVE DES ENFANTS ---
		if (registry.hasComponent<HierarchyComponent>(entity))
		{
			// F6 optim : HierarchyComponent children = registry.getComponent<HierarchyComponent>(entity); // HierarchyComponent children = ... copie la struct entière — donc son std::vector<Entity> m_children — à chaque nœud, à chaque frame, dans une récursion.
			const auto& children = registry.getComponent<HierarchyComponent>(entity);   // référence, zéro copie

			for (Entity childID : children.m_children)
			{
				UpdateWorldTransforms(registry, childID, transform.m_worldTransform);
			}
		}
	}

	/// <summary>
	/// Parcourt les composants de transformation et initie la mise à jour des matrices de transformation mondiales pour les entités racines (ou sans parent) en appelant UpdateWorldTransforms avec une matrice identité.
	/// </summary>
	/// <param name="registry">Référence au registre d'entités et de composants.</param>
	void WorldTransformSystem(Registry& registry, const Matrix44f& worldIdentityMatrix)
	{

		SparseSet<TransformComponent>* TransformComponentsPool = registry.getStorage<TransformComponent>();
		auto& Transforms = TransformComponentsPool->GetDenseData();
		auto& Entities = TransformComponentsPool->GetDenseEntities();

		for (size_t i = 0; i < Transforms.size(); i++)
		{

			// On ne prend que les entités qui n'ont pas de parent, afin de démarrer le calcul récursif des matrices à partir d'elles
			// 1. les entités solitaires (pas de hiérarchie). La transformation de ces entités ne dépend pas d'autruit (comme un astéroide ou un vaisseau spatial par exemple)
			// 2. ou les entités patriarches (root = true). Ces entités sont le début d'une hiérarchie dont la transformations des enfants dépendent de lui.
			if (!registry.hasComponent<HierarchyComponent>(Entities[i]) || registry.getComponent<HierarchyComponent>(Entities[i]).m_isRoot)
			{
				// On peut démarrer l'itération
				UpdateWorldTransforms(registry, Entities[i], worldIdentityMatrix);
			}
			//else
				//assert(false && "WorldTransformSystem : une entité enfant ne devrait pas être traitée ici, elle sera traitée par la récursion de son parent.");
		}
	}

	//********************************************************************

	// Fonction d'aide pour le Lerp
	Vec3f lerp(const Vec3f& initial, const Vec3f & final, float t)
	{
		return initial + (final - initial) * t;
	}

	// Doit s'exécuter avant que la matrice locale finale ne soit construite.
	void CameraSystem(Registry& registry, float deltaTime)
	{
		for (auto&& [entity, camera, transform] : registry.ViewGroup< CameraComponent, TransformComponent>())
		{
			// La cible est la position statique définie dans le JSON
			const Vec3f& targetPos = transform.m_initialLocalPosition;
			const Quatf targetRot = Quatf(transform.m_initialLocalRotation, false);

			if (!camera.isInitialized)
			{
				camera.currentSmoothedPos = targetPos;
				camera.currentSmoothedRot = targetRot;
				camera.isInitialized = true;
			}

			// Lissage (Lerp/Slerp)
			float t = deltaTime * camera.smoothSpeed;
			camera.currentSmoothedPos = transform.m_initialLocalPosition;
			//camera.currentSmoothedPos = lerp(camera.currentSmoothedPos, targetPos, t);
			camera.currentSmoothedRot = Quatf(transform.m_initialLocalRotation, false);//
			//camera.currentSmoothedRot = Slerp(camera.currentSmoothedRot, targetRot, t);


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
			Vec3f pos1 = Vec3f{ transform1.m_worldTransform[3][0],
			transform1.m_worldTransform[3][1],
			transform1.m_worldTransform[3][2] };

			// 1. Préparer la liste des collisions de CETTE frame
			std::set<Entity> newOverlaps;

			// 2. Boucle N*N pour trouver les collisions

			for (auto&& [entity2, trigger2, transform2] : registry.ViewGroup<TriggerComponent, TransformComponent>())
			{
				if (entity1 == entity2) continue;

				Vec3f pos2 = Vec3f{ transform2.m_worldTransform[3][0],
								transform2.m_worldTransform[3][1],
								transform2.m_worldTransform[3][2] };

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

		// Lecture de la worldTransform (qui a déjà été calculée par WorldTransformSystem)
		Vec3f worldPosition = Vec3f(transform.m_worldTransform[3][0], transform.m_worldTransform[3][1], transform.m_worldTransform[3][2]);

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

				Vec3f worldPosition = Vec3f(transform.m_worldTransform[3][0], transform.m_worldTransform[3][1], transform.m_worldTransform[3][2]);

				std::cout << " - " << name.m_id
					<< " [Pos : " << worldPosition.x << ", " << worldPosition.y << ", " << worldPosition.z << "]" << std::endl;


				// Appel réel du rendu, une fois le pointeur résolu :
					// monMoteur->dessiner(*mesh, transform.m_worldTransform, viewMatrix);
					// Chaque SubMesh de *mesh porte déjà son propre MaterialHandle (submesh.material) —
					// résolu à son tour via resourceManager.GetMaterial(submesh.material) au moment du dessin.

			}


			if (registry.hasComponent<CameraComponent>(Entities[tr]))
			{
				//CameraComponent camera = registry.getComponent<CameraComponent>(el);
				auto& transform = Transforms[tr];
				auto& name = registry.getComponent<NameComponent>(Entities[tr]);


				Vec3f worldPosition = Vec3f(transform.m_worldTransform[3][0], transform.m_worldTransform[3][1], transform.m_worldTransform[3][2]);
				std::cout << " - " << name.m_id
					<< " [Pos : " << worldPosition.x << ", " << worldPosition.y << ", " << worldPosition.z << "]" << std::endl;
			}

			ident++;
		}

	}

	//********************************************************************
	void PlayerInputSystem(Registry& registry, float deltaTime) {

		for (auto&& [entity, control, transform] : registry.ViewGroup<PlayerControlComponent, TransformComponent>()) {
			// 'control' et 'transform' sont des références directes et MODIFIABLES
			// Aucune vérification 'hasComponent' ou 'getComponent' dans la boucle.
			// Itération dense et optimisée.
			transform.m_initialLocalPosition.x += control.m_speed * deltaTime;
		}

	}

} // namespace LV3