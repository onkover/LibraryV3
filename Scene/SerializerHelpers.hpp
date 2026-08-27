#pragma once

#include <fstream>
#include <filesystem>
#include <vector>
#include "Maths/Projection.h"      // EProjectionType, ELensModel, EGateFit
#include "../Ressources/json.hpp"
#include "Registry.hpp"


namespace LV3
{
	/**********************************************

		Helper pour la sérialisation du graph scene

	**********************************************/

	namespace fs = std::filesystem;
	using json = nlohmann::json;

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

	// Lit un tableau JSON de 3 nombres et retourne un Vec3f. Si la clé est absente ou invalide, retourne la valeur par défaut.
	[[nodiscard]] inline Vec3f ReadVec3(const json& j, const char* key, const Vec3f& def) noexcept
	{
		if (!j.contains(key)) return def;
		const json& a = j[key];
		if (!a.is_array() || a.size() < 3) return def;
		if (!a[0].is_number() || !a[1].is_number() || !a[2].is_number()) return def;
		return Vec3f(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());
	}

// ***********************************************************************************

	[[nodiscard]] inline EProjectionType ReadProjection(const json& j, const char* key) noexcept
	{
		const std::string s = j.value(key, std::string("perspective"));
		return (s == "orthographic" || s == "ortho")
			? EProjectionType::Orthographic
			: EProjectionType::Perspective;
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