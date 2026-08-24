#pragma once

#include <string>
#include <set>
#include "../Maths/Vectorlib.h"
#include "../Maths/MatrixLib.h"
#include "../Maths/QuaternionLib.h"
#include "Entity.hpp"

#include "Lighting/LightTypes.h"
#include "../Lighting/Color.h"
#include "../../Ressources/ResourceHandle.h"
#include "Maths/Projection.h"      // EProjectionType, ELensModel, EGateFit
#include "Maths/Transform.h" 

namespace LV3
{
	//********************************************************************
	// L'ancienne hiérarchie de `node` dans les anciennes vesion du scène graph devient un simple composant ici
	struct HierarchyComponent
	{
		Entity m_parent = NULL_ENTITY;
		std::vector <Entity> m_children;
		bool m_isRoot = false;
	};

	//********************************************************************

	struct NameComponent
	{
		std::string m_id;
	};

	
	//********************************************************************
	/*
		LA LENTILLE, et rien d'autre.

		Aucune position, aucune rotation, aucune matrice : c'est le
		TransformComponent qui porte le placement, exactement comme pour
		n'importe quelle autre entité. Une caméra est un objet de la scène
		équipé d'un objectif — pas une classe à part.

		Aucun état dérivé non plus : view, projection et frustum sont
		recalculés chaque frame par le CameraSystem dans un ViewData.
		Ce composant est une donnée d'AUTEUR, pure et sérialisable.
	*/
	struct CameraComponent
	{
		// --- Type de projection ---
		EProjectionType m_projection = EProjectionType::Perspective;		// projection par défaut de type Perspective

		// --- Plans de clipping (communs aux deux projections) ---
		float m_nearPlane = 0.1f;					// plan proche, en unités monde
		float m_farPlane = 1000.0f;					// plan lointain, en unités monde
		bool  m_infiniteFar = false;				// ignore m_farPlane : plus aucune limite lointaine

		// --- Perspective : paramétrage ---
		ELensModel m_lensModel = ELensModel::FieldOfView;

		//   ... modèle FieldOfView
		float m_fovYDeg = 45.0f;          // VERTICAL, en degrés (convention moteur)

		//   ... modèle Filmback (sténopé). m_fovYDeg en est alors dérivé.
		float    m_focalLengthMm = 35.0f;		// 1.378" — focale standard
		float    m_filmWidthMm = 24.892f;		// 0.980" — 35 mm Full Aperture
		float    m_filmHeightMm = 18.669f;		// 0.735"
		EGateFit m_gateFit = EGateFit::Fill;	// Fill = la pellicule tient dans la fenêtre (on rogne), 
												// Overscan = la pellicule déborde de la fenêtre (on remplit)

		// --- Orthographique ---
		float m_orthoHeight = 10.0f;      // hauteur visible en unités monde

		// --- Sélection ---
		bool m_isActive = true;	
		int  m_priority = 0;              // la plus haute gagne quand plusieurs sont actives

		//  -- affichage de gizmo dans l'éditeur ---
		float m_gizmoLength = 0.0f;     // 0 = pas de gizmo
	};

	// Verrou : ce composant doit rester 
	// * une donnée brute, 
	// * copiable sans logique, 
	// * sérialisable telle quelle.
	static_assert(std::is_aggregate_v<CameraComponent>, "CameraComponent doit rester un agregat : aucune logique dedans");
	static_assert(std::is_trivially_copyable_v<CameraComponent>, "CameraComponent doit rester trivialement copiable");



	//********************************************************************
	/*
		L'état de lissage sort de la caméra : c'est du CONTRÔLEUR.
		Unity : Camera + Cinemachine.  Unreal : UCameraComponent + APlayerCameraManager. Jamais dans la même classe.

		Ce composant se pose sur la même entité que CameraComponent, ou pas du tout — une caméra libre n'en a pas besoin.

		"m_isEnabled" n'est pas un mécanisme général d'activation de composant. Il existe pour résoudre un conflit précis : FPSControllerSystem et CameraFollowSystem écrivent tous les deux dans le même TransformComponent. Si les deux tournent, le second écrase le premier.
		"m_followRotation" : c'est le choix entre deux caméras classiques :
			* true → l'offset est exprimé dans le repère de la cible : la caméra reste derrière le véhicule quand il tourne (3e personne).
			* false → l'offset est en espace monde : la caméra garde une direction fixe, la cible tourne devant elle (isométrique, RTS).
	*/
	struct CameraFollowComponent
	{
		bool m_isEnabled = true;					// si FPSControllerComponent.m_isEnabled = True, celui-ci sera mi à False
		Entity m_target = NULL_ENTITY;				// résolu en différé depuis le nom JSON
		Vec3f  m_offset{ 0.0f, 2.0f, -6.0f };		// en espace local de la cible
		float  m_lookAtHeight = 0.0f;	            // élève le point visé au-dessus de l'origine
		float  m_smoothSpeed = 5.0f;				// 0 = suivi rigide, sans lissage
		bool   m_followRotation = true;             // l'offset pivote-t-il avec la cible ?


		// --- État (mis à jour par le CameraFollowSystem jamais par le JSON) ---
		Vec3f m_smoothedPos;
		Quatf m_smoothedRot;
		bool  m_isInitialized = false;
	};

	//********************************************************************
	struct FPSControllerComponent
	{
		bool  m_isEnabled = true;
		float m_moveSpeed = 5.0f;				// unités monde par SECONDE
		float m_sprintMultiplier = 3.0f;		// facteur appliqué quand Shift est tenu
		float m_mouseSensitivity = 0.15f;		// DEGRÉS par pixel
		bool  m_lockVertical = true;			// true = FPS au sol | false = vol libre 6 DoF
		float m_pitchLimitDeg = 89.0f;			// limite de l'angle de pitch pour éviter le retournement complet

		// État accumulé (écrit par le système)
		float m_yawDeg = 0.0f;
		float m_pitchDeg = 0.0f;
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
		// optimisation au profit du ressourcemanager : on ne stocke pas le MeshClass ici, juste un handle vers le ResourceManager
//		std::shared_ptr<MeshClass> m_mesh;
//		std::string m_texture;
		MeshHandle m_meshHandle;

		// Données pour l'animation
		float m_orbitalSpeed = 0.0f;
		float m_rotationSpeed = 0.0f;

		float m_orbitRadius = 0.0f;      // NOUVEAU : figé au chargement, jamais recalculé

		// Les variables d'état (angles)
		float m_currentOrbitAngle = 0.0f;
		float m_currentRotationAngle = 0.0f;

	};

	//********************************************************************
	struct TransformComponent
	{
	public:
		Transform m_local;
		Quatf     m_initialRotation;       // NOUVEAU : rotation d'auteur, base de l'animation
		Matrix44f m_localMatrix;
		Matrix44f m_worldMatrix;
		bool      m_dirty = true;			// évite de reconstruire les matrices pour rien
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

	//********************************************************************

	// Le gizmo sait quelle camera il decrit.
	struct CameraGizmoComponent
	{
		Entity m_owner = NULL_ENTITY;
		float  m_length = 3.0f;          // longueur d'affichage, PAS le farPlane
	};

	static_assert(std::is_trivially_copyable_v<CameraGizmoComponent>);

	// GENERIQUE : la couche rendu ignore ce qu'est une camera.
	struct DebugVisualComponent
	{
		Color  m_color;
		Entity m_hideForCamera = NULL_ENTITY;   // masque si la vue vient de cette camera
	};

	static_assert(std::is_trivially_copyable_v<DebugVisualComponent>);

}