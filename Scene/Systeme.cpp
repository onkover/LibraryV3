#pragma once
#include "pch.h"          // ? première ligne, toujours

#include "Registry.hpp"
#include <Maths/MatrixLib.h>
#include "../Core/EventBus.hpp"
#include <map>	// pour le debug
#include <set>



/// <summary>
/// Met à jour les angles d'orbite et de rotation des composants Mesh dans le registre en fonction du temps écoulé.
/// </summary>
/// <param name="registry">Le registre contenant les entités et leurs composants.</param>
/// <param name="deltaTime">Temps écoulé (en secondes) depuis la dernière mise à jour, utilisé pour incrémenter les angles selon les vitesses orbitales et de rotation.</param>
void AnimationSystem(Registry &registry, float deltaTime)
{	
	// On utilise la ViewGroup (ComponentView) qui retourne (Entity, Component&...)
	for (auto& [entity, mesh] : registry.ViewGroup<MeshComponent>())
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
			MeshComponent &mesh = registry.getComponent<MeshComponent>(entity);

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
			CameraComponent &camera = registry.getComponent<CameraComponent>(entity);

			translationMatrix.translate(camera.currentSmoothedPos);
			RotationQuat = camera.currentSmoothedRot;

		}

		// --- 3. Composition et mise à jour de la matrice locale ---
		transform.m_localTransform = scaleMatrix * RotationQuat.ToMatrix44() * translationMatrix;
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
		HierarchyComponent children = registry.getComponent<HierarchyComponent>(entity);
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
void WorldTransformSystem(Registry& registry, Matrix44f &worldIdentityMatrix)
{
//	Matrix44f worldIdentityMatrix;
	worldIdentityMatrix.rotateX(45 * TO_RADIAN);

	SparseSet<TransformComponent>* TransformComponentsPool = registry.getStorage<TransformComponent>();
	auto& Transforms = TransformComponentsPool->GetDenseData();
	auto& Entities = TransformComponentsPool->GetDenseEntities();

	for (size_t i = 0; i < Transforms.size(); i++)
	{
		//TransformComponent& transform = Transforms[i];
		//Entity entity = Entities[i];

		// On ne prend que les entités qui n'ont pas de parent, afin de démarrer le calcul récursif des matrices à partir d'elles
		// 1. les entités solitaires (pas de hiérarchie). La transformation de ces entités ne dépend pas d'autruit (comme un astéroide ou un vaisseau spatial par exemple)
		// 2. ou les entités patriarches (root = true). Ces entités sont le début d'une hiérarchie dont la transformations des enfants dépendent de lui.
		if (!registry.hasComponent<HierarchyComponent>(Entities[i]) || registry.getComponent<HierarchyComponent>(Entities[i]).m_isRoot)
		{
			// On peut démarrer l'itération
			UpdateWorldTransforms(registry, Entities[i], worldIdentityMatrix);
		}
		else
			int b = 0;
	}
}

//********************************************************************
 
// Fonction d'aide pour le Lerp
Vec3f lerp(const Vec3f& initial, const Vec3f& final, float t)
{
	return initial + (final - initial) * t;
}

// Doit s'exécuter avant que la matrice locale finale ne soit construite.
void CameraSystem(Registry & registry, float deltaTime)
{
	for (auto& [entity, camera, transform] : registry.ViewGroup< CameraComponent, TransformComponent>())
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
void TriggerSystem(Registry& registry, EventBus& eventBus)
{
	for (auto& [entity1, trigger1, transform1] : registry.ViewGroup<TriggerComponent, TransformComponent>())
	{
		Vec3f pos1 = Vec3f{ transform1.m_worldTransform[3][0],
		transform1.m_worldTransform[3][1],
		transform1.m_worldTransform[3][2] };

		// 1. Préparer la liste des collisions de CETTE frame
		std::set<Entity> newOverlaps;

		// 2. Boucle N*N pour trouver les collisions

		for (auto& [entity2, trigger2, transform2] : registry.ViewGroup<TriggerComponent, TransformComponent>())
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
	for (Entity entity = 0; entity < registry.getEntityCount(); entity++)
	{
		if (registry.hasComponent<TransformComponent>(entity))
		{
			if (!registry.hasComponent<HierarchyComponent>(entity) || registry.getComponent<HierarchyComponent>(entity).m_isRoot)
			{
				DebugDisplaySystemRecursive(registry, entity, 0);
			}
		}
	}
}


void RenderSystem(Registry& registry, Entity activeCamera)
{
	// 1. Obtenir la matrice de Vue
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
			
			Vec3f worldPosition = Vec3f(transform.m_worldTransform[3][0], transform.m_worldTransform[3][1], transform.m_worldTransform[3][2]);

			std::cout << " - " << name.m_id
				<< " [Pos : " << worldPosition.x << ", " << worldPosition.y << ", " << worldPosition.z << "]" << std::endl;

			// Appel réel du rendu:
			// monMoteur->dessiner(mesh.model, transform.worldTransform, viewMatrix);

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

	for (auto& [entity, control, transform] : registry.ViewGroup<PlayerControlComponent, TransformComponent>()) {
		// 'control' et 'transform' sont des références directes et MODIFIABLES
		// Aucune vérification 'hasComponent' ou 'getComponent' dans la boucle.
		// Itération dense et optimisée.
		transform.m_initialLocalPosition.x += control.m_speed * deltaTime;
	}

}