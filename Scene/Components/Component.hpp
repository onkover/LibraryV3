#pragma once

#include <string>
#include <set>
#include <Maths/Vectorlib.h>
#include <Maths/MatrixLib.h>
#include <Maths/QuaternionLib.h>
#include "Entity.hpp"

#include <Objects/Light/Light.h>
#include <Objects/mesh.h>

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

struct MeshComponent
{
	// Données pour l'animation
	float m_orbitalSpeed = 0.0f;
	float m_rotationSpeed = 0.0f;

	std::shared_ptr<MeshClass> m_mesh;
	std::string m_texture;

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

struct TriggerComponent
{
public:
	//Vec3f halfSize{ 1, 1, 1 }; // Taille de la boîte (demi-dimensions)
	//bool isColliding = false; // État

	float radius = 1.0f; // La taille de notre trigger sphérique

	// Noms des événements que ce trigger publiera
	std::string onEnterEvent;// = "";
	std::string onStayEvent;// = "";
	std::string onExitEvent;// = "";

	// État (mis à jour par le TriggerSystem)
	bool is_colliding = false;
	std::set<Entity> overlapping_entities; // Avec qui on collisionne
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


//enum LightType
//{
//	POINT_LIGHT,
//	DIRECTIONAL_LIGHT,
//	SPOT_LIGHT
//};

struct LightComponent
{
	lIGHT_TYPE m_type = lIGHT_TYPE::POINT_LIGHT;
	Vec3f m_color = { 1.0f, 1.0f, 1.0f };
	float m_intensity = 1.0f;	
};
