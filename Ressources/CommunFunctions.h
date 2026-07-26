#pragma once
#include <filesystem>


namespace LV3 {
	namespace fs = std::filesystem;


	/*
		"assets/cube.obj", "Assets/cube.obj", "assets\\cube.obj" et "./assets/cube.obj" désignent tous le même fichier physique, mais ce sont quatre chaînes différentes, donc quatre clés différentes pour unordered_map<std::string, MeshHandle>.
		solution canoniser le chemin pour que toutes les variantes pointent vers la même clé.
	*/
	/// <summary>
	/// Retourne la version canonicalisée du chemin fourni.
	/// </summary>
	/// <param name="filepath">Chemin du fichier à canonicaliser.</param>
	/// <returns>Chemin canonicalisé.</returns>
	inline std::string CanonicalKey(const std::string& filepath)
	{
		std::error_code ec;

		// weakly_canonical ne nécessite pas que le fichier existe déjà (contrairement à canonical()), ce qui compte pour un chemin dont l'existence n'est pas encore vérifiée à cet instant.
		fs::path canonical = fs::weakly_canonical(filepath, ec);	// résout les . et .., normalise les séparateurs, et ne requiert pas que le fichier existe

		// generic_string() force les '/' même sous Windows -> une seule convention de séparateur.
		return ec ? filepath : canonical.generic_string();	// ec ? protège aussi contre un chemin syntaxiquement invalide sans jamais faire planter le chargement.

	}
}