#include "pch.h"
#include "ResourceManager.h"
#include "CommunFunctions.h"
#include "core/logger.h"
#include "Core/EngineSettings.h"

namespace LV3 
{

ResourceManager::~ResourceManager()     // Cf. commentaire dans ResourceManager.h sur la raison de définir le destructeur dans le .cpp
{
    Logger::info("[ResourceManager] Destruction — " + std::to_string(GetMeshCount()) + " mesh(es) libéré(s) via UnloadAll()\n");
    UnloadAll();
}

// ── Meshes ────────────────────────────────────────────

/// <summary>
/// Charge un mesh depuis un fichier OBJ et le retourne dans un std::expected, ou un error si le chargement échoue.
/// </summary>
/// <param name="filepath">Chemin du fichier OBJ à charger (const std::string&).</param>
/// <param name="opt">Options de chargement (OBJLoadOptions) à appliquer.</param>
/// <returns>MeshHandle du mesh chargé ou récupéré depuis le cache. Peut être invalide en cas d'échec.</returns>
std::expected<MeshHandle, EMeshLoadError> ResourceManager::LoadMeshChecked(const std::string& filepath, const OBJLoadOptions& opt)
{
	// 0. Canonise l'URL pour avoir la même clé quelque soit le chemin d'accès (ex: "assets/cube.obj", "Assets/cube.obj", "assets\\cube.obj" et "./assets/cube.obj" désignent tous le même fichier physique, mais ce sont quatre chaînes différentes, donc quatre clés différentes pour unordered_map<std::string, MeshHandle>).
    const std::string key = CanonicalKey(filepath);

    // 1. Vérifie si le mesh est déjà chargé dans le cache
    if (auto it = m_pathToMesh.find(key); it != m_pathToMesh.end())
        return it->second;

    if (!fs::exists(filepath))
        return std::unexpected(EMeshLoadError::FileNotFound);

    // 2. Sinon charge le mesh via OBJLoader et l'enregistre dans le cache
    MeshHandle h = OBJLoader::Load(filepath, *this, opt);
    if (!h.IsValid())
        return std::unexpected(EMeshLoadError::ParseFailed);   // OBJLoader ne distingue pas encore parse échoué / mesh vide — à affiner 
    
    m_pathToMesh.emplace(key, h);
    m_meshIdToPath.emplace(h.id, key);

    // 3. Retourne le handle du mesh chargé (ou Invalid() en cas d'échec)
    return h;
}

//***********************************************************************************************
/// <summary>
/// Retourne un pointeur constant vers le MeshClass associé au handle donné, ou nullptr si le handle est invalide ou si le mesh n'est pas trouvé. Méthode const.
/// </summary>
/// <param name="h">Handle du mesh à rechercher ; la validité est vérifiée via h.IsValid().</param>
/// <returns>Un pointeur constant vers le MeshClass correspondant au handle, ou nullptr si le handle est invalide ou si le mesh n'existe pas.</returns>
const MeshClass* ResourceManager::GetMesh(MeshHandle h) const 
{
    if(!h.IsValid()) return nullptr;

    auto it = m_meshes.find(h.id);
	return (it != m_meshes.end()) ? it->second.get() : nullptr; // Retourne un pointeur vers le MeshClass correspondant au handle, 
                                                                // ou nullptr si le handle est invalide ou non trouvé
}

//***********************************************************************************************
/// <summary>
/// Récupère un pointeur modifiable vers le Mesh associé au handle en appelant la surcharge const puis en supprimant la constance.
/// </summary>
/// <param name="h">Handle identifiant le mesh à récupérer.</param>
/// <returns>Pointeur non-const vers MeshClass correspondant au handle (obtenu via const_cast).</returns>
MeshClass* ResourceManager::GetMesh(MeshHandle h)
{
    return const_cast<MeshClass*>(static_cast<const ResourceManager*>(this)->GetMesh(h));
}

//***********************************************************************************************

MeshHandle ResourceManager::FindMesh(const std::string& filepath) const
{
    auto it = m_pathToMesh.find(CanonicalKey(filepath));
    return (it != m_pathToMesh.end()) ? it->second : MeshHandle::Invalid();
}

//***********************************************************************************************
/// <summary>
/// Vérifie si un maillage est déjà chargé pour le chemin fourni.
/// </summary>
/// <param name="filepath">Chemin du fichier du maillage utilisé comme clé de recherche.</param>
/// <returns>true si un maillage correspondant au chemin est présent, sinon false.</returns>
bool ResourceManager::IsMeshLoaded(const std::string& filepath) const
{
    return m_pathToMesh.find(CanonicalKey(filepath)) != m_pathToMesh.end();
}

//***********************************************************************************************
/// <summary>
/// Décharge un maillage en particulier du gestionnaire de ressources pendant que le programme tourne, sans toucher au reste.
/// Les scénarios qui justifieraient un tel appel sont typiquement : 
/// * un changement de niveau (décharger les assets du niveau précédent tout en gardant ceux partagés)
/// * une contrainte mémoire sur une plateforme limitée, 
/// * ou un rechargement à chaud pendant le développement
/// </summary>
/// <param name="h">Handle du maillage à décharger.</param>
void ResourceManager::UnloadMesh(MeshHandle h)
{
    if (!h.IsValid()) return;
	
    // décharge le mesh du cache et de la map des chemins (via la map inverse m_meshIdToPath pour retrouver le chemin associé au handle)
    if (auto pathIt = m_meshIdToPath.find(h.id); pathIt != m_meshIdToPath.end())
    {
        m_pathToMesh.erase(pathIt->second);
        m_meshIdToPath.erase(pathIt);
    }
    m_meshes.erase(h.id);
}






// ── Matériaux ─────────────────────────────────────────

MaterialHandle ResourceManager::FindMaterialByName(const std::string& name) const {
    auto it=m_nameToMaterial.find(name);
    return (it!=m_nameToMaterial.end())?it->second:MaterialHandle::Invalid();
}
const Material* ResourceManager::GetMaterial(MaterialHandle h) const {
    if(!h.IsValid()) return nullptr;
    auto it=m_materials.find(h.id);
    return (it!=m_materials.end())?it->second.get():nullptr;
}
Material* ResourceManager::GetMaterial(MaterialHandle h){
    return const_cast<Material*>(static_cast<const ResourceManager*>(this)->GetMaterial(h));
}

// ── Chaines de LOD ────────────────────────────────────
// Meme patron que LoadMeshChecked : cle canonique, cache, puis delegation au chargeur.
std::expected<LodChainHandle, ELodChainLoadError>
ResourceManager::LoadLodChainChecked(const std::string& filepath)
{
    const std::string key = CanonicalKey(filepath);
    if (auto it = m_pathToLodChain.find(key); it != m_pathToLodChain.end())
        return it->second;

    auto result = LodChainLoader::Load(filepath, *this);
    if (result)                                   // on ne met en cache QUE les succes :
        m_pathToLodChain.emplace(key, *result);   // un echec corrige sur disque pourra etre recharge
    return result;
}

// Chaine de longueur 1 : la porte d'entree (RegisterLodChain) verifie tout,
// comme pour une chaine decrite. Aucun chemin de code parallele.
LodChainHandle ResourceManager::GetOrCreateSingleLevelChain(MeshHandle h)
{
    if (auto it = m_meshToSingleChain.find(h.id); it != m_meshToSingleChain.end())
        return it->second;

    const MeshClass* mesh = GetMesh(h);
    if (!mesh)
    {
        Logger::error("GetOrCreateSingleLevelChain — mesh introuvable (id " + std::to_string(h.id) + ")");
        return LodChainHandle::Invalid();
    }

    LodChain c;                          // invEps : +inf puis -inf (valeurs par defaut)
    c.bounds = mesh->GetMeshAABB();  // un seul niveau : l'union est sa propre boite
    c.levels[0] = h;
    c.levelCount = 1;

    const LodChainHandle ch = RegisterLodChain(c);
    if (ch.IsValid())
        m_meshToSingleChain.emplace(h.id, ch);   // on ne met en cache que les succes
    return ch;
}

const LodChain* ResourceManager::GetLodChain(LodChainHandle h) const noexcept
{
    // Un seul acces memoire : l'id EST l'index. Case vide (dechargee) => nullptr.
    if (h.id >= m_lodChains.size()) return nullptr;       // couvre aussi un vecteur vide
    const LodChain& c = m_lodChains[h.id];
    return (c.levelCount != 0) ? &c : nullptr;            // couvre la sentinelle (id 0)
}



// ── API interne (Loaders) ─────────────────────────────

MeshHandle ResourceManager::RegisterMesh(std::unique_ptr<MeshClass> mesh) {
    LV3_ASSERT(mesh != nullptr);

    // Chantier 2 : plafond de sommets verifie UNE fois, a la porte d'entree
    // unique de tout mesh (OBJLoader aujourd'hui, meshes proceduraux demain).
    // RenderView s'appuie ensuite sur ClipSpaceBuffer::Capacity() sans payer
    // de verification a chaque frame.
    if (mesh->vertexCount() > kMaxMeshVerticesHard)
    {
        Logger::error("RegisterMesh — '" + mesh->name + "' : "
            + std::to_string(mesh->vertexCount()) + " sommets > plafond "
            + std::to_string(kMaxMeshVerticesHard) + " (LV3_MAX_MESH_VERTICES) — mesh refuse");

        return MeshHandle::Invalid();
    }

    const MeshHandle h = AllocateMeshHandle();
    m_meshes.emplace(h.id, std::move(mesh));
    return h;
}

MaterialHandle ResourceManager::RegisterMaterial(std::unique_ptr<Material> mat){
    LV3_ASSERT(mat!=nullptr);
    const std::string& name=mat->GetName();
    auto it=m_nameToMaterial.find(name);
    if(it!=m_nameToMaterial.end()) return it->second;
    const MaterialHandle h=AllocateMaterialHandle();
    m_nameToMaterial.emplace(name,h);
    m_materials.emplace(h.id,std::move(mat));
    return h;
}

// Porte d'entree UNIQUE de toute chaine (lecteur JSON aujourd'hui, code procedural demain).
// Tout invariant dont depend SelectLodLevel est verifie ICI, une fois :
// RenderView lui fait ensuite confiance sans aucun test.
LodChainHandle ResourceManager::RegisterLodChain(const LodChain& chain)
{
    constexpr float kInf = std::numeric_limits<float>::infinity();
    const uint32_t  n = chain.levelCount;

    auto reject = [](const std::string& why) {
        Logger::error("RegisterLodChain — " + why + " — chaine refusee");
        return LodChainHandle::Invalid();
        };

    if (n < 1 || n > LodChain::kMaxLevels)
        return reject("levelCount = " + std::to_string(n) + " hors de [1, " + std::to_string(LodChain::kMaxLevels) + "]");

    if (chain.invEps[0] != kInf)
        return reject("invEps[0] doit valoir +inf (L0 exact)");

    for (uint32_t k = 0; k < n; ++k)
        if (GetMesh(chain.levels[k]) == nullptr)
            return reject("niveau " + std::to_string(k) + " : mesh absent");

    // Strictement decroissant, fini et positif : condition du PREFIXE de SelectLodLevel.
    for (uint32_t k = 1; k < n; ++k)
        if (!(chain.invEps[k] > 0.0f && chain.invEps[k] < chain.invEps[k - 1]))
            return reject("invEps[" + std::to_string(k) + "] doit etre > 0 et < invEps[" + std::to_string(k - 1) + "]");

    // Cases inutilisees : jamais admises, jamais dessinees.
    for (uint32_t k = n; k < LodChain::kMaxLevels; ++k)
        if (chain.invEps[k] != -kInf || chain.levels[k].IsValid())
            return reject("case " + std::to_string(k) + " inutilisee mais non neutre");

    if (m_lodChains.empty())
        m_lodChains.emplace_back();                        // case 0 : sentinelle, levelCount = 0

    const LodChainHandle h{ uint32_t(m_lodChains.size()) };
    m_lodChains.push_back(chain);
    return h;
}


// ── API interne (Loaders) ─────────────────────────────

void ResourceManager::UnloadAll(){
    //m_meshes.clear(); 
    //m_pathToMesh.clear();
    //m_meshIdToPath.clear();

    //m_materials.clear(); 
    //m_nameToMaterial.clear();
    
    m_meshes.clear();
    m_pathToMesh.clear();
    m_meshIdToPath.clear();

    m_materials.clear();
    m_nameToMaterial.clear();

    // PAS de clear() : la taille EST le prochain id. Retrecir recyclerait les ids (ABA).
    for (LodChain& c : m_lodChains) c = LodChain{};       // cases vides, ids preserves
    m_pathToLodChain.clear();     // sinon le cache rendrait des handles vers des cases vides
    m_meshToSingleChain.clear();  // idem pour les chaines implicites

    // todo unload la map inverse des matériaux
}
size_t ResourceManager::GetMeshCount()     const noexcept { return m_meshes.size(); }
size_t ResourceManager::GetMaterialCount() const noexcept { return m_materials.size(); }
size_t ResourceManager::GetLodChainCount() const noexcept
{
    return size_t(std::count_if(m_lodChains.begin(), m_lodChains.end(),
        [](const LodChain& c) { return c.levelCount != 0; }));
}
MeshHandle     ResourceManager::AllocateMeshHandle()     noexcept { return MeshHandle{m_nextMeshId++}; }
MaterialHandle ResourceManager::AllocateMaterialHandle() noexcept { return MaterialHandle{m_nextMaterialId++}; }

ResourceManager::ResourceManager(ResourceManager&&) noexcept = default;
ResourceManager& ResourceManager::operator=(ResourceManager&&) noexcept = default;

} // namespace LV3
