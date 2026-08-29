#pragma once
#include <iostream>

#include "Entity.hpp"
#include "../Core/EventBus.hpp"
#include "Registry.hpp"
#include "rendering/viewport.h"
#include "rendering/viewdata.h"
#include "rendering/framebuffer.h"
#include "rendering/depthbuffer.h"
#include "../Core/InputState.h"
#include "Core/EventNames.h"
#include "../Core/Logger.h"
#include "../Ressources/ResourceManager.h"
#include "CameraBinding.hpp"

namespace LV3
{
// --- Camera ---
	void FPSControllerSystem(Registry& reg, const InputState& in, float deltaTime);
	void CameraFollowSystem(Registry& registry, float deltaTime);
	[[nodiscard]] Entity FindCameraByName(Registry& registry, const std::string& name);
	[[nodiscard]] Entity FindActiveCamera(Registry& registry);
	
	// LA source unique de l'angle vertical. Personne ne recalcule ceci ailleurs.
	[[nodiscard]] inline float CameraFovY(const CameraComponent& cam) noexcept
	{
		LV3_ASSERT(cam.m_lensModel == ELensModel::Filmback ||
			(cam.m_fovYDeg >= 1.0f && cam.m_fovYDeg <= 179.0f));
		
		return (cam.m_lensModel == ELensModel::Filmback)
			? Projection::FovYFromFocal(cam.m_focalLengthMm, cam.m_filmHeightMm)
			: cam.m_fovYDeg * TO_RADIAN;
	}
	// --- Transformation ---
	void WorldTransformSystem(Registry& registry);
	void LocalTransformSystem(Registry& registry);

	// --- Diagnostics (actifs en Debug uniquement) ---
	void CheckAnimationBaseline(Registry& registry);            // UNE fois, après chargement
	void CheckSceneInvariants(Registry& registry);            // CHAQUE frame
	void CheckControllerExclusivity(Registry & registry);       // CHAQUE frame — invariant FPS/Follow
	void DebugTraceEntity(Registry& registry, const std::string& name);
	void DebugDisplaySystem(Registry& registry);

	// --- Les systèmes---
	void AnimationSystem(Registry& registry, float deltaTime);
	//	void RenderSystem(Registry& registry, Entity activeCamera);
	void RenderSystem(Registry& registry, Entity activeCamera, ResourceManager& resourceManager);
//	void CameraGizmoSystem(Registry& registry, Entity activeCamera, float aspect, const GizmoAssets& assets);
	void PlayerInputSystem(Registry& registry, float deltaTime);
	void TriggerSystem(Registry& registry, EventBus& eventBus);
//	ViewData BuildViewData(const TransformComponent& tr, const CameraComponent& cam, const Viewport& vp);
//	ViewData BuildViewData(const TransformComponent& tr, const CameraComponent& cam, const Viewport& vp, Entity camEntity) noexcept;
	ViewData BuildViewData(const Registry& registry, const CameraBinding& b) noexcept;

	class HealthSystem
	{

	public:
			static constexpr int kDamagePerHit = 5;

			HealthSystem(Registry* registry, EventBus& eventBus)
			{
				eventBus.subscribe(Events::TakingDamage,
					[registry]([[maybe_unused]] Entity source, Entity target)
					{
						HealthComponent* health = registry->TryGet<HealthComponent>(target);
						if (!health) return;

						health->m_currentHealth = std::max(0, health->m_currentHealth - kDamagePerHit);

						// Le log cite la MEME constante que le subscribe.
						// Il lui est desormais impossible de diverger.
						Logger::log(std::string("[HealthSystem] ") + Events::TakingDamage
							+ " sur idx " + std::to_string(EntityIndex(target))
							+ " — vie restante : " + std::to_string(health->m_currentHealth));
					});
			}
	};

	//********************************************************************

	class AudioSystem
	{
	public:
		AudioSystem(EventBus& eventBus)
		{
			eventBus.subscribe(Events::StartedTakingDamage,
				[]([[maybe_unused]] Entity e1, [[maybe_unused]] Entity e2)	// [[maybe_unused]] pour taire l'avertissement
				{
					std::cout << "[AudioSystem] *Joue son de grésillement (début)*" << std::endl;
				});
			eventBus.subscribe(Events::StoppedTakingDamage,
				[]([[maybe_unused]] Entity e1, [[maybe_unused]] Entity e2)	// [[maybe_unused]] pour taire l'avertissement
				{
					std::cout << "[AudioSystem] *Arrête le son de grésillement (fin)*" << std::endl;
				});

		}

	};


}