#pragma once
// ============================================================
//  Ressources/LodChainLoader.h — lecture d'un descripteur .lod.json
//  (format lv3.lodchain v1, annexe A13 bis).
//  Le CACHE par chemin n'est pas ici : il vit dans le ResourceManager
//  (LoadLodChainChecked), comme pour les meshes.
// ============================================================
#include <cstdint>
#include <expected>
#include <string>
#include "ResourceHandle.h"

namespace LV3
{
    class ResourceManager;

    enum class ELodChainLoadError : std::uint8_t
    {
        FileNotFound,     // fichier absent ou illisible
        ParseFailed,      // JSON invalide, ou racine qui n'est pas un objet
        BadHeader,        // format / version / unites inattendus
        BadLevels,        // tableau absent, vide, trop long, chemin vide, epsilon de L0 non nul
        LevelMeshFailed,  // un .obj de niveau n'a pas pu etre charge
        LevelMismatch,    // faces ou sommets du .obj != descripteur (descripteur perime)
        Rejected          // RegisterLodChain a refuse un invariant
    };

    [[nodiscard]] const char* ToString(ELodChainLoadError e) noexcept;

    class LodChainLoader
    {
    public:
        // Les chemins des meshes sont RELATIFS au dossier du descripteur.
        [[nodiscard]] static std::expected<LodChainHandle, ELodChainLoadError> Load(const std::string& path, ResourceManager& rm);
    };
}