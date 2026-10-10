#pragma once
#include <memory>
#include <string>

#include <unordered_map>
#include <expected>	
#include "ResourceHandle.h"

#include "LodChain.h"
#include "LodChainLoader.h"     // ELodChainLoadError (type complet requis par std::expected)
#include "OBJLoader.h"

#include "geometry/Material.h"
#include "OBJLoadOptions.h" 
#include "geometry/SubMesh.h"
#include "geometry/MeshClass.h"

namespace LV3 {
    class MeshClass;
    class Material;
   

    enum class EMeshLoadError : std::uint8_t
    {
        FileNotFound,
        ParseFailed,
        EmptyMesh
    };


    class ResourceManager {
    public:
        ResourceManager()  = default;
        ~ResourceManager(); 
        /* 
            Destructeur déclaré dans le header mais défini dans le .cpp). 
		    ceci pour une raison importante liée aux types incomplets : MeshClass et Material sont déclarés mais pas définis dans ce header (= forward-déclarés)
		    Il ne peut donc pas générer le code pour déruire les destructeurs de std::unique_ptr<MeshClass> et std::unique_ptr<Material> dans ce header, car il ne connaît pas la taille de ces types.
            ... Il a besoin de la définition complète de MeshClass et material. Au moment où le compilateur compile le .cpp, il a accès aux définitions complètes de MeshClass et material, donc il peut générer le bon code de destruction.
                

            Cela réduit aussi les dépendances de compilation : les clients du header n'ont pas besoin d'inclure MeshClass.h.
            C'est une bonne pratique en C++ moderne avec les smart pointers ! 👍
        */


        ResourceManager(const ResourceManager&)            = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        //ResourceManager(ResourceManager&&)                 = default;
        //ResourceManager& operator=(ResourceManager&&)      = default;
        ResourceManager(ResourceManager&&)            noexcept;
        ResourceManager& operator=(ResourceManager&&) noexcept;

        // ── Meshes ────────────────────────────────────────────
//        MeshHandle       LoadMesh(const std::string& filepath, const OBJLoadOptions& opt={});
        [[nodiscard]] const MeshClass* GetMesh(MeshHandle h) const;
        [[nodiscard]] MeshClass*       GetMesh(MeshHandle h);
        [[nodiscard]] MeshHandle       FindMesh(const std::string& filepath, const OBJLoadOptions& opt) const;
        [[nodiscard]] bool             IsMeshLoaded(const std::string& filepath, const OBJLoadOptions& opt) const;        // Nouvelle méthode, canal d'erreur explicite — coexiste avec LoadMesh() pour ne pas casser les appelants existants qui se contentent d'un MeshHandle invalide.
        [[nodiscard]] std::expected<MeshHandle, EMeshLoadError> LoadMeshChecked(const std::string& filepath, const OBJLoadOptions& opt = {});

        void UnloadMesh(MeshHandle h);


        // ── Matériaux ─────────────────────────────────────────
        [[nodiscard]] MaterialHandle   FindMaterialByName(const std::string& name) const;
        [[nodiscard]] const Material*  GetMaterial(MaterialHandle h) const;
  //      [[nodiscard]]       Material*  GetMaterial(MaterialHandle h);

  //      // ── API interne (Loaders) ─────────────────────────────
  //      MeshHandle     RegisterMesh    (std::unique_ptr<MeshClass> mesh);
  //      MaterialHandle RegisterMaterial(std::unique_ptr<Material> mat);

		//// ── Utilitaires ─────────────────────────────────────────
  //      [[nodiscard]] size_t GetMeshCount()     const noexcept;
  //      [[nodiscard]] size_t GetMaterialCount() const noexcept;

        [[nodiscard]] Material* GetMaterial(MaterialHandle h);

        // ── Chaines de LOD (A13 bis) ──────────────────────────
        // Charge un descripteur .lod.json, ou rend la chaine deja chargee
        // (cache par chemin canonique, comme LoadMeshChecked).
        [[nodiscard]] std::expected<LodChainHandle, ELodChainLoadError>
            LoadLodChainChecked(const std::string& filepath, const OBJLoadOptions& opts);

        // Chaine de longueur 1 pour un mesh SANS descripteur (planetes, gizmos...).
        // UNE chaine par mesh, partagee : mille entites -> une seule ligne de cache.
        // SelectLodLevel y rend toujours 0 (cases inutilisees a -inf).
        [[nodiscard]] LodChainHandle GetOrCreateSingleLevelChain(MeshHandle h);

        // Pointeur TEMPORAIRE : valable jusqu'au prochain RegisterLodChain
        // (stockage contigu, le vecteur peut realouer). Le handle est la
        // reference durable. Pas de version non-const : immuable apres chargement.
        [[nodiscard]] const LodChain* GetLodChain(LodChainHandle h) const noexcept;


        // ── API interne (Loaders) ─────────────────────────────
        MeshHandle     RegisterMesh(std::unique_ptr<MeshClass> mesh);
        MaterialHandle RegisterMaterial(std::unique_ptr<Material> mat);
        LodChainHandle RegisterLodChain(const LodChain& chain);   // verifie les invariants de SelectLodLevel

        // ── Utilitaires ─────────────────────────────────────────
        [[nodiscard]] size_t GetMeshCount()     const noexcept;
        [[nodiscard]] size_t GetMaterialCount() const noexcept;
        [[nodiscard]] size_t GetLodChainCount() const noexcept;

        // todo créer la map inverse des matériaux

		// ── UnloadAll : décharge toutes les ressources (meshes et matériaux) et vide les caches
        void UnloadAll();

    private:
		uint32_t m_nextMeshId = 1u; // Compteur pour générer des handles uniques pour les meshes
		std::unordered_map<uint32_t, std::unique_ptr<MeshClass>>    m_meshes;   // Carte des handles vers les meshes
        std::unordered_map<std::string, MeshHandle>                 m_pathToMesh;   // Carte des chemins vers les handles de mesh
		std::unordered_map<uint32_t, std::string>                   m_meshIdToPath;   // Carte des chemins inverse pour retrouver rapidement le chemin d'un mesh à partir de son handle sans parcourir toute la map inutilement
		                                                                              // optimisation pour UnloadMesh() : on peut retrouver le chemin du mesh à partir de son handle en O(1) au lieu de faire un scan linéaire de m_pathToMesh        
        MeshHandle     AllocateMeshHandle()     noexcept;

		uint32_t m_nextMaterialId = 1u; // Compteur pour générer des handles uniques pour les matériaux
        std::unordered_map<uint32_t,    std::unique_ptr<Material>>  m_materials;
        std::unordered_map<std::string, MaterialHandle>             m_nameToMaterial;        
        MaterialHandle AllocateMaterialHandle() noexcept;

        // Index = id du handle. Case 0 = sentinelle de l'id invalide, jamais lue.
        // Jamais retreci : un id n'est JAMAIS recycle (meme regle que m_nextMeshId).
        // Case vide = levelCount == 0.
        std::vector<LodChain> m_lodChains;

        // Cache chemin canonique -> chaine. Pas de miroir id -> chemin :
        // aucun UnloadLodChain, donc aucun lecteur pour ce miroir.
        std::unordered_map<std::string, LodChainHandle> m_pathToLodChain;
        // Cache des chaines IMPLICITES : id du mesh -> sa chaine de longueur 1.
        std::unordered_map<uint32_t, LodChainHandle>    m_meshToSingleChain;



    };

} // namespace LV3
