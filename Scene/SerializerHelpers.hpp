#pragma once

#include <fstream>
#include <filesystem>
#include <vector>
#include "Maths/Projection.h"      // EProjectionType, ELensModel, EGateFit
//#include "../Ressources/json.hpp"
#include "Registry.hpp"
#include "../core/JsonReader.h"

namespace LV3
{
	/**********************************************

		Helper pour la sérialisation du graph scene

	**********************************************/

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

	static ECameraCategory ReadCameraCategory(JsonReader& r, const char* key)
	{
		const std::string s = r.Read(key, std::string("gameplay"));
		if (s == "gameplay") return ECameraCategory::Gameplay;
		if (s == "debug")    return ECameraCategory::Debug;
		Logger::warn("[Camera] categorie inconnue '" + s + "' -> gameplay");
		return ECameraCategory::Gameplay;
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


}