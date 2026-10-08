#pragma once
/**********************************************

	Helper pour la sérialisation du graph scene

**********************************************/

#include <fstream>
#include <filesystem>
#include <vector>
#include <optional>
#include <format>
#include <cmath>
#include "Maths/Projection.h"      // EProjectionType, ELensModel, EGateFit
#include "Registry.hpp"
#include "../core/JsonReader.h"

namespace LV3
{

	namespace fs = std::filesystem;

// ***********************************************************************************

// Résout un chemin relatif au fichier JSON
	[[nodiscard]] inline std::string ResolvePath(const std::string& baseDir, const std::string& rel)
	{
		if (rel.empty())
			return {};

		fs::path p = fs::path(baseDir) / fs::path(rel);

		return p.lexically_normal().string();
	}

// ***********************************************************************************

	[[nodiscard]] inline EProjectionType ReadProjectionType(LV3::JsonReader& r, const char* key)
	{
		const std::string s = r.Read(key, std::string("perspective"));
		if (s == "orthographic" || s == "ortho") return EProjectionType::Orthographic;
		if (s == "perspective" || s == "persp") return EProjectionType::Perspective;

		Logger::warn("[Camera] projection inconnue '" + s + "' -> perspective");
		return EProjectionType::Perspective;
	}

	// ***********************************************************************************

	[[nodiscard]] inline ECameraCategory ReadCameraCategory(JsonReader& r, const char* key)
	{
		const std::string s = r.Read(key, std::string("gameplay"));
		if (s == "gameplay") return ECameraCategory::Gameplay;
		if (s == "debug")    return ECameraCategory::Debug;
		Logger::warn("[Camera] categorie inconnue '" + s + "' -> gameplay");
		return ECameraCategory::Gameplay;
	}

	// ***********************************************************************************
	// Surcharge d'une valeur MOTEUR, strictement positive et finie.
	//   absente / null / type invalide : JsonReader::TryRead a deja parle -> nullopt (suit le moteur)
	//   <= 0 ou non finie (1e39 -> inf)   : souci signale ICI             -> nullopt (suit le moteur)
	// Validee UNE fois, a l'entree dans le moteur : le chemin chaud n'assert plus que l'invariant.
	[[nodiscard]] inline std::optional<float> ReadPositiveOverride(JsonReader& r, const char* key, float engineValue, const char* comp, const std::string& owner)
	{
		const std::optional<float> v = r.TryRead(key, engineValue);

		if (!v)
			return std::nullopt;

		if (!(*v > 0.0f) || !std::isfinite(*v))      // forme positive niee : rejette aussi NaN
		{
			Logger::warn(std::format("[{}] cle '{}' invalide ({}, attendu > 0 et fini) sur {} — defaut pris : {} (moteur)", comp, key, *v, owner, engineValue));
			return std::nullopt;
		}

		return v;
	}

	// ***********************************************************************************

	// Étiquette lisible d'une entité pour les diagnostics.
	// Entity étant un uint32_t empaqueté, l'afficher brut donne 16777218
	// au lieu de « index 2, génération 1 » — illisible.
	[[nodiscard]] inline std::string EntityLabel(Registry& reg, Entity e)
	{
		if (e == NULL_ENTITY) return "<NULL_ENTITY>";

		const std::string name = reg.hasComponent<NameComponent>(e)
			? reg.getComponent<NameComponent>(e).m_id
			: std::string("<sans nom>");

		return name + " (idx " + std::to_string(EntityIndex(e))
			+ ", gen " + std::to_string(EntityGeneration(e)) + ")";
	}

	[[nodiscard]] inline ETriggerRole ReadTriggerRole(LV3::JsonReader& r, const char* key, const std::string& owner)
	{
		const std::string roleStr = r.Read(key, std::string("zone"));
		if (roleStr == "zone")  return ETriggerRole::Zone;
		if (roleStr == "probe") return ETriggerRole::Probe;

		Logger::warn("[Trigger] " + owner + " : role inconnu '" + roleStr + "' — traite comme 'zone'.");
		return ETriggerRole::Zone;
	}

}