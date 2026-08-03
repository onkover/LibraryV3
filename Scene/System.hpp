#pragma once
#include <iostream>

#include "Entity.hpp"
#include "../Core/EventBus.hpp"
#include "Registry.hpp"
#include "rendering/viewport.h"
#include "rendering/viewdata.h"
#include "../Core/InputState.h"

namespace LV3
{
	void DebugDisplaySystem(Registry& registry);
	void PlayerInputSystem(Registry& registry, float deltaTime);
	void TriggerSystem(Registry& registry, EventBus& eventBus);
	void CameraSystem(Registry& registry, float deltaTime);
	void FPSControllerSystem(Registry& reg, const InputState& in, float deltaTime);
	void CameraFollowSystem(Registry& registry, float deltaTime);
	//void WorldTransformSystem(Registry& registry);
	void WorldTransformSystem(Registry& registry);
	//void WorldTransformSystem(Registry& registry, const Matrix44f& worldIdentityMatrix);
	//void WorldTransformSystem(Registry& registry);

	void LocalTransformSystem(Registry& registry);
	void AnimationSystem(Registry& registry, float deltaTime);
//	void RenderSystem(Registry& registry, Entity activeCamera);
	void RenderSystem(Registry& registry, Entity activeCamera, ResourceManager& resourceManager);

	ViewData BuildViewData(const TransformComponent& tr, const CameraComponent& cam, const Viewport& vp);


	class HealthSystem
	{

	public:
		HealthSystem(Registry* registry, EventBus& eventBus)
		{
			eventBus.subscribe("TAKING_DAMAGE",
				[this, registry]([[maybe_unused]] Entity trigger, Entity entity)	// [[maybe_unused]] pour taire l'avertissement
				{
					// La logique à exécuter quand l'événement est reçu
					if (registry->hasComponent<HealthComponent>(entity))
					{
						auto& health = registry->getComponent<HealthComponent>(entity);
						health.m_currentHealth -= 5;

						std::cout << "[HealthSystem] Événement 'DAMAGE_PLAYER' reçu ! Vie restante : " << health.m_currentHealth << std::endl;
					}
				}
			);
		}
	};

	//********************************************************************

	class AudioSystem
	{
	public:
		AudioSystem(EventBus& eventBus)
		{
			eventBus.subscribe("STARTED_TAKING_DAMAGE",
				[]([[maybe_unused]] Entity e1, [[maybe_unused]] Entity e2)	// [[maybe_unused]] pour taire l'avertissement
				{
					std::cout << "[AudioSystem] *Joue son de grésillement (début)*" << std::endl;
				});
			eventBus.subscribe("STOPPED_TAKING_DAMAGE",
				[]([[maybe_unused]] Entity e1, [[maybe_unused]] Entity e2)	// [[maybe_unused]] pour taire l'avertissement
				{
					std::cout << "[AudioSystem] *Arrête le son de grésillement (fin)*" << std::endl;
				});

		}

	};

}