#pragma once
#include <iostream>

#include "Entity.hpp"
#include "../Core/EventBus.hpp"
#include "Registry.hpp"


namespace LibV3
{
	void DebugDisplaySystem(Registry& registry);
	void PlayerInputSystem(Registry& registry, float deltaTime);
	void TriggerSystem(Registry& registry, EventBus& eventBus);
	void CameraSystem(Registry& registry, float deltaTime);
	//void WorldTransformSystem(Registry& registry);
	void WorldTransformSystem(Registry& registry, Matrix44f& worldIdentityMatrix);
	//void WorldTransformSystem(Registry& registry);

	void LocalTransformSystem(Registry& registry);
	void AnimationSystem(Registry& registry, float deltaTime);

	class HealthSystem
	{

	public:
		HealthSystem(Registry* registry, EventBus& eventBus)
		{
			eventBus.subscribe("TAKING_DAMAGE",
				[this, registry](Entity trigger, Entity entity)
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
		AudioSystem(Registry* registry, EventBus& eventBus)
		{
			eventBus.subscribe("STARTED_TAKING_DAMAGE",
				[](Entity e1, Entity e2)
				{
					std::cout << "[AudioSystem] *Joue son de grésillement (début)*" << std::endl;
				});
			eventBus.subscribe("STOPPED_TAKING_DAMAGE",
				[](Entity e1, Entity e2)
				{
					std::cout << "[AudioSystem] *Arrête le son de grésillement (fin)*" << std::endl;
				});

		}

	};

}