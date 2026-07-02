#include "pch.h"
#include "ResourceManager.h"

namespace LV3 
{

ResourceManager::~ResourceManager()     // Cf. commentaire dans ResourceManager.h sur la raison de définir le destructeur dans le .cpp
{
    UnloadAll();
}

// ── Meshes ────────────────────────────────────────────

/// <summary>
/// La fonction retourne un MeshHandle pour filepath en réutilisant un cache ou en chargeant le mesh depuis un fichier OBJ.
/// </summary>
/// <param name="filepath">Chemin du fichier OBJ à charger (const std::string&).</param>
/// <param name="opt">Options de chargement (OBJLoadOptions) à appliquer.</param>
/// <returns>MeshHandle du mesh chargé ou récupéré depuis le cache. Peut être invalide en cas d'échec.</returns>
MeshHandle ResourceManager::LoadMesh(const std::string& filepath, const OBJLoadOptions& opt)
{
	// 1. Vérifie si le mesh est déjà chargé dans le cache
    auto it = m_pathToMesh.find(filepath);
    if (it != m_pathToMesh.end()) 
        return it->second;  // Si trouvé dans le cache, retourne le handle existant

	// 2. Sinon charge le mesh via OBJLoader et l'enregistre dans le cache
    MeshHandle h = OBJLoader::Load(filepath, *this, opt);
    if (h.IsValid()) 
        m_pathToMesh.emplace(filepath, h);  // Insertion via emplace évite une copie inutile si l'entrée n'existe pas

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

MeshHandle ResourceManager::FindMesh(const std::string& filepath) const
{
    auto it = m_pathToMesh.find(filepath);
    return (it != m_pathToMesh.end()) ? it->second : MeshHandle::Invalid();
}

/// <summary>
/// Vérifie si un maillage est déjà chargé pour le chemin fourni.
/// </summary>
/// <param name="filepath">Chemin du fichier du maillage utilisé comme clé de recherche.</param>
/// <returns>true si un maillage correspondant au chemin est présent, sinon false.</returns>
bool ResourceManager::IsMeshLoaded(const std::string& filepath) const
{
    return m_pathToMesh.find(filepath) != m_pathToMesh.end();
}

void ResourceManager::UnloadMesh(MeshHandle h)
{
    if (!h.IsValid()) return;
    for (auto it = m_pathToMesh.begin(); it != m_pathToMesh.end(); ++it)
        if (it->second == h) 
            { 
                m_pathToMesh.erase(it); 
                break;
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

// ── API interne (Loaders) ─────────────────────────────

MeshHandle ResourceManager::RegisterMesh(std::unique_ptr<MeshClass> mesh) {
    assert(mesh != nullptr);
    const MeshHandle h = AllocateMeshHandle();
    m_meshes.emplace(h.id, std::move(mesh));
    return h;
}

MaterialHandle ResourceManager::RegisterMaterial(std::unique_ptr<Material> mat){
    assert(mat!=nullptr);
    const std::string& name=mat->GetName();
    auto it=m_nameToMaterial.find(name);
    if(it!=m_nameToMaterial.end()) return it->second;
    const MaterialHandle h=AllocateMaterialHandle();
    m_nameToMaterial.emplace(name,h);
    m_materials.emplace(h.id,std::move(mat));
    return h;
}

// ── API interne (Loaders) ─────────────────────────────

void ResourceManager::UnloadAll(){
    m_meshes.clear(); m_pathToMesh.clear();
    m_materials.clear(); m_nameToMaterial.clear();
}
size_t ResourceManager::GetMeshCount()     const noexcept { return m_meshes.size(); }
size_t ResourceManager::GetMaterialCount() const noexcept { return m_materials.size(); }
MeshHandle     ResourceManager::AllocateMeshHandle()     noexcept { return MeshHandle{m_nextMeshId++}; }
MaterialHandle ResourceManager::AllocateMaterialHandle() noexcept { return MaterialHandle{m_nextMaterialId++}; }

ResourceManager::ResourceManager(ResourceManager&&) noexcept = default;
ResourceManager& ResourceManager::operator=(ResourceManager&&) noexcept = default;

} // namespace LV3
