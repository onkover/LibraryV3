#include "pch.h"
#include "LodChainLoader.h"
#include "ResourceManager.h"
#include "../Core/JsonReader.h"      // HORS pch.h (voir son en-tete)

#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

namespace LV3
{
    namespace
    {
        constexpr const char* kFormat = "lv3.lodchain";
        constexpr int         kVersion = 1;
        constexpr float       kNaN = std::numeric_limits<float>::quiet_NaN();

        // Lecture BRUTE d'un niveau : aucune decision ici.
        // Defauts EMPOISONNES : aucune verification ne les laisse passer.
        struct LevelDesc
        {
            std::string mesh;                // "" : refuse en phase 4
            uint32_t    vertices = 0;        // 0  : ne correspond a aucun mesh
            uint32_t    faces = 0;          // 0  : idem
            float       epsilon = kNaN;     // NaN: rejete par RegisterLodChain
        };
    }

    const char* ToString(ELodChainLoadError e) noexcept
    {
        switch (e)
        {
        case ELodChainLoadError::FileNotFound:    return "FileNotFound";
        case ELodChainLoadError::ParseFailed:     return "ParseFailed";
        case ELodChainLoadError::BadHeader:       return "BadHeader";
        case ELodChainLoadError::BadLevels:       return "BadLevels";
        case ELodChainLoadError::LevelMeshFailed: return "LevelMeshFailed";
        case ELodChainLoadError::LevelMismatch:   return "LevelMismatch";
        case ELodChainLoadError::Rejected:        return "Rejected";
        }
        return "?";
    }

    std::expected<LodChainHandle, ELodChainLoadError>     LodChainLoader::Load(const std::string& path, ResourceManager& rm, const OBJLoadOptions& opts)
    {
        namespace fs = std::filesystem;

        auto fail = [&path](ELodChainLoadError e, const std::string& why)
            {
                Logger::error("LodChainLoader [" + std::string(ToString(e)) + "] " + why + " — " + path);
                return std::unexpected(e);
            };

        // ── 1. Fichier -> JSON, sans exception (allow_exceptions = false) ──
        std::ifstream file(path);
        if (!file)
            return fail(ELodChainLoadError::FileNotFound, "fichier introuvable ou illisible");

        const nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
        if (j.is_discarded() || !j.is_object())
            return fail(ELodChainLoadError::ParseFailed, "JSON invalide ou racine non objet");

        JsonReader r(j, "LodChain", path);

        // ── 2. En-tete ──
        const std::string format = r.Read<std::string>("format", std::string{});
        const int         version = r.Read<int>("version", 0);
        const std::string units = r.Read<std::string>("units", std::string{});
        (void)r.Read<std::string>("name", std::string{});   // informatif : lu pour WarnUnread

        if (format != kFormat || version != kVersion)
            return fail(ELodChainLoadError::BadHeader, "attendu " + std::string(kFormat) + " v" + std::to_string(kVersion) +
                                                        ", lu '" + format + "' v" + std::to_string(version));
        if (units != "local")
            return fail(ELodChainLoadError::BadHeader, "unites '" + units + "', attendu 'local'");

        // ── 3. Lecture BRUTE des niveaux : rien n'est charge avant d'avoir tout lu ──
        std::vector<LevelDesc> descs;
        descs.reserve(LodChain::kMaxLevels);

        JsonReader levels = r.Child("levels");
        levels.ForEachElement([&descs](std::size_t, JsonReader e)
            {
                LevelDesc d;
                d.mesh = e.Read<std::string>("mesh", std::string{});
                d.vertices = e.Read<uint32_t>("vertices", 0u);
                d.faces = e.Read<uint32_t>("faces", 0u);
                d.epsilon = e.Read<float>("epsilon", kNaN);
                e.WarnUnread();
                descs.push_back(std::move(d));
            });
        r.WarnUnread();

        if (descs.empty() || descs.size() > LodChain::kMaxLevels)
            return fail(ELodChainLoadError::BadLevels,
                std::to_string(descs.size()) + " niveau(x), attendu 1 a " +
                std::to_string(LodChain::kMaxLevels));
       
        if (descs[0].epsilon != 0.0f)       //Comparer des flottants avec != est d'habitude une faute.
                                            // Ici, c'est voulu : le générateur écrit exactement 0.0, et la règle est qu'ε₀ vaut exactement zéro, puisque L0 est la référence.
            return fail(ELodChainLoadError::BadLevels, "epsilon de L0 doit valoir 0 (niveau de reference)");

        // ── 4. Construction ──
        LodChain chain;                                  // bounds vide, cases neutres
        chain.levelCount = uint32_t(descs.size());
        const fs::path dir = fs::path(path).parent_path();

        for (uint32_t k = 0; k < chain.levelCount; ++k)
        {
            const LevelDesc& d = descs[k];
            const std::string tag = "niveau " + std::to_string(k);

            if (d.mesh.empty())
                return fail(ELodChainLoadError::BadLevels, tag + " : chemin de mesh vide");

            const std::string meshPath = (dir / d.mesh).string();
            const auto m = rm.LoadMeshChecked(meshPath, opts);
            if (!m)
                return fail(ELodChainLoadError::LevelMeshFailed, tag + " : echec du chargement de " + meshPath);

            const MeshClass* mesh = rm.GetMesh(*m);
            if (mesh->faceCount() != d.faces || mesh->vertexCount() != d.vertices)
                return fail(ELodChainLoadError::LevelMismatch,
                                    tag + " : le .obj a " + std::to_string(mesh->vertexCount()) + " sommets / " +
                                    std::to_string(mesh->faceCount()) + " faces, le descripteur annonce " +
                                    std::to_string(d.vertices) + " / " + std::to_string(d.faces) +
                                    " (descripteur perime ?)");

            chain.levels[k] = *m;
            chain.bounds.Expand(mesh->GetMeshAABB());    // union : un niveau grossier peut deborder de L0

            // L0 garde +inf (valeur par defaut). Pour k >= 1 : 0 -> +inf, negatif, NaN :
            // tous rejetes par RegisterLodChain. Aucun test ici, la porte d'entree s'en charge.
            if (k > 0)
                chain.invEps[k] = 1.0f / d.epsilon;
        }

        // ── 5. Porte d'entree unique ──
        const LodChainHandle h = rm.RegisterLodChain(chain);
        if (!h.IsValid())
            return fail(ELodChainLoadError::Rejected, "invariant refuse (detail ci-dessus)");

        Logger::info("LodChainLoader : " + std::to_string(chain.levelCount) + " niveaux — " + path);
        return h;
    }
}