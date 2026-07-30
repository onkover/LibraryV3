#pragma once

#include <string>
#include <set>
#include "../Maths/Vectorlib.h"
#include "../Maths/MatrixLib.h"
#include "../Maths/QuaternionLib.h"
#include "Entity.hpp"

#include "Lighting/LightTypes.h"
//#include "Geometry/MeshClass.h"
#include "../../Ressources/ResourceHandle.h"

namespace LV3
{
	//********************************************************************
	// L'ancienne hiérarchie de `node` dans les anciennes vesion du scène graph devient un simple composant ici
	struct HierarchyComponent
	{
		Entity m_parent;
		std::vector <Entity> m_children;
		bool m_isRoot = false;
	};

	//********************************************************************

	struct NameComponent
	{
		std::string m_id;
	};

	//********************************************************************

	struct CameraComponent
	{
		float m_fov = 45.0f;
		float m_nearPlane = 0.01f;
		float m_farPlane = 1000.0f;

		float smoothSpeed = 5.0f; // Vitesse du lissage

		// Données d'état (mises à jour par le CameraSystem)
		Vec3f currentSmoothedPos;
		Quatf currentSmoothedRot;
		bool isInitialized = false;
	};

	//********************************************************************
	/* 
	RÉFÉRENCE, pas de propriété : le ResourceManager reste l'unique propriétaire.
	Résolution : resourceManager.GetMesh(m_mesh) au moment de l'usage (rendu, culling, etc.)
	Les matériaux ne sont PAS dupliqués ici : chaque MeshClass::SubMesh porte déjà son
	propre MaterialHandle (voir SubMesh.h) — un mesh multi-matériaux fonctionne nativement.
	*/
	struct MeshComponent
	{
		// optilisation au profit du ressourcemanager : on ne stocke pas le MeshClass ici, juste un handle vers le ResourceManager
//		std::shared_ptr<MeshClass> m_mesh;
//		std::string m_texture;
		MeshHandle m_meshHandle;

		// Données pour l'animation
		float m_orbitalSpeed = 0.0f;
		float m_rotationSpeed = 0.0f;

		// Les variables d'état (angles)
		float m_currentOrbitAngle = 0.0f;
		float m_currentRotationAngle = 0.0f;

	};

	//********************************************************************

	struct TransformComponent
	{
	public:
		// Données statiques
		Vec3f m_initialLocalPosition{ 0.0f };
		//Quatf m_initialLocalRotation;
		Vec3f m_initialLocalRotation{ 0.0f };
		Vec3f m_initialLocalScale{ 1.0f };

		// Transformations
		Matrix44f m_localTransform;
		Matrix44f m_worldTransform;

	};

	//********************************************************************



//	Attention : aucun constructeur explicite, seulement des valeurs par défaut sur certains membres (radius = 1.0f, is_colliding = false).C'est important, car Emplace transmet ses arguments à un constructeur — et TriggerComponent n'a que le constructeur agrégé implicite(agrégat C++).
// Ça fonctionne, mais avec une règle stricte : l'ordre des arguments doit suivre exactement l'ordre de déclaration des membres
// L'agrégat ne permet pas de sauter un membre au milieu pour garder le suivant à sa valeur par défaut si tu en fournis un après.
	struct TriggerComponent
	{
	public:
		//Vec3f halfSize{ 1, 1, 1 }; // Taille de la boîte (demi-dimensions)
		//bool isColliding = false; // État

		float radius = 1.0f; // La taille de notre trigger sphérique

		// Noms des événements que ce trigger publiera
		std::string onEnterEvent = "";
		std::string onStayEvent = "";
		std::string onExitEvent = "";

		// État (mis à jour par le TriggerSystem)
		bool is_colliding =false;
		std::set<Entity> overlapping_entities; // Avec qui on collisionne

		// Constructeur explicite : seuls radius et onEnterEvent sont obligatoires
	/*	explicit TriggerComponent(float r, 
									std::string onEnter = "", 
									std::string onStay = "", 
									std::string onExit = "",
									bool isColliding)
									: radius(r), 
									onEnterEvent(std::move(onEnter)), 
									onStayEvent(std::move(onStay)), 
									onExitEvent(std::move(onExit)),
									is_colliding(isColliding)
		{
		}*/
	};

	//********************************************************************

	struct HealthComponent {
		int m_maxHealth = 100;
		int m_currentHealth = 100;
	};

	//********************************************************************

	struct PlayerControlComponent
	{
		float m_speed = 1.0f;		// Pour déplacer le vaisseau
	};

	//********************************************************************

	struct LightComponent
	{
		ELightType m_type = ELightType::Point;
		Vec3f m_color = { 1.0f, 1.0f, 1.0f };
		float m_intensity = 1.0f;
	};
}