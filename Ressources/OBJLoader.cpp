#include "pch.h"
#include "OBJLoader.h"
#include "MTLLoader.h"
#include "../Geometry/MeshClass.h"
#include "../Geometry/SubMesh.h"
#include "ResourceManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace LV3 {

namespace {
    inline Vec3f V3Cross(const Vec3f& a,const Vec3f& b) noexcept{ return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
    inline float V3Dot  (const Vec3f& a,const Vec3f& b) noexcept{ return a.x*b.x+a.y*b.y+a.z*b.z; }
    inline Vec3f V3Norm (const Vec3f& v) noexcept
    {
        float l=V3Dot(v,v); if(l<1e-12f) return{0,1,0};
        float i=1.f/std::sqrt(l); return{v.x*i,v.y*i,v.z*i};
    }
    inline Vec3f V3Sub(const Vec3f& a,const Vec3f& b) noexcept{ return{a.x-b.x,a.y-b.y,a.z-b.z}; }
    inline Vec3f V3Add(const Vec3f& a,const Vec3f& b) noexcept{ return{a.x+b.x,a.y+b.y,a.z+b.z}; }
    inline Vec3f V3Scl(const Vec3f& v,float s)        noexcept{ return{v.x*s,v.y*s,v.z*s}; }
    inline Vec3f V3Min(const Vec3f& a,const Vec3f& b) noexcept{ return{std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)}; }
    inline Vec3f V3Max(const Vec3f& a,const Vec3f& b) noexcept{ return{std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}; }

    // ResolveOBJ convertit les indices de vertex, UV et normales du format OBJ (1-based, avec possibilité d'indices négatifs) en indices 0-based utilisés dans les tableaux C++.
    inline int ResolveOBJ(int raw,int count) noexcept
    {
        if(raw==0) return -1; 
        if (raw < 0) return count + raw;
        return raw-1;
    }

	// VertexKey représente une combinaison unique d'indices de position, UV, normale et groupe de lissage pour un sommet dans le fichier OBJ.
    struct VertexKey
    { 
        int32_t pos,uv,nrm,smoothGroup;
        bool operator==(const VertexKey& o) const noexcept
        {
            return pos == o.pos && uv == o.uv && nrm == o.nrm && smoothGroup == o.smoothGroup;
        }
    };

    // Hash personnalisé pour VertexKey, combinant les hash des champs avec une constante de mélange pour réduire les collisions
    struct VKHash
    {
        size_t operator()(const VertexKey& k) const noexcept
        {
            size_t h=std::hash<int32_t>{}(k.pos);
            h^=std::hash<int32_t>{}(k.uv)          +0x9e3779b9u+(h<<6)+(h>>2);
            h^=std::hash<int32_t>{}(k.nrm)         +0x9e3779b9u+(h<<6)+(h>>2);
            h^=std::hash<int32_t>{}(k.smoothGroup) +0x9e3779b9u+(h<<6)+(h>>2);
            return h;
        }
    };
}

//***********************************************************************************************
/// <summary>
/// Charge un fichier OBJ, charge les matériaux associés et construit un MeshHandle.
/// </summary>
/// <param name="filepath">Chemin vers le fichier OBJ à charger.</param>
/// <param name="rm">ResourceManager utilisé pour charger/obtenir les matériaux et autres ressources.</param>
/// <param name="options">Options de chargement (OBJLoadOptions) influençant l'analyse et la construction du maillage.</param>
/// <returns>MeshHandle représentant le maillage construit, ou MeshHandle::Invalid() en cas d'échec.</returns>
MeshHandle OBJLoader::Load(const std::string& filepath, ResourceManager& rm, const OBJLoadOptions& options)
{
	// Parsing du fichier OBJ pour extraire les données brutes (positions, normales, UVs, faces, matériaux référencés)
    ParseResult parsedDataOBJ;
    if (!ParseFile(filepath, parsedDataOBJ))
        return MeshHandle::Invalid();


    // à piori, on ne tombera jamais sur ce cas
    // ParseFile ne retourne true que si !out.rawVertex.empty() && !out.rawFaces.empty() — donc si on arrive après le premier if, parsedDataOBJ.rawFaces est garanti non vide
	// to do : optimiser la gestion des erreurs : si le fichier est vide ou ne contient pas de vertex/faces, on pourrait retourner un MeshHandle invalide plus tôt, mais pour l'instant on laisse passer cette vérification ici
    if (parsedDataOBJ.rawFaces.empty())
        return MeshHandle::Invalid();

	// Si ok, on charge les matériaux référencés dans le fichier OBJ (via les fichiers MTL) et on construit le mesh final
    namespace fs = std::filesystem;
    const std::string baseDir = fs::path(filepath).parent_path().string();
    const MaterialMap matMap  = LoadMaterials(parsedDataOBJ, baseDir, rm);
   
	// et pour finir, on construit le mesh à partir des données parsées et des matériaux chargés, en appliquant les options de chargement spécifiées
    return BuildMesh(parsedDataOBJ, matMap, options, rm);
}

//***************************************************************************************

/// <summary>
/// Analyse un fichier OBJ et remplit une structure ParseResult avec les positions, normales, coordonnées UV, faces, bibliothèques de matériaux et changements de matériau.
/// </summary>
/// <param name="filepath">Chemin vers le fichier OBJ à ouvrir et analyser.</param>
/// <param name="out">Référence vers ParseResult qui sera remplie. Champs affectés : rawVertex, rawNormals, rawUVs, rawFaces, hasUVs, hasNormals, mtllibs, materialChanges.</param>
/// <returns>true si le fichier a été ouvert et que des positions et des faces ont été parsées ; false sinon.</returns>
bool OBJLoader::ParseFile(const std::string& filepath, ParseResult& out)
{
    std::ifstream file(filepath);
    if (!file.is_open())
        return false;   // fichier introuvable/inaccessible
    
	// Réserve de la capacité pour éviter les reallocations fréquentes lors de l'ajout d'éléments
    out.rawVertex.reserve(8192);
    out.rawNormals.reserve(4096);
    out.rawUVs.reserve(8192);
    out.rawFaces.reserve(8192);

    std::string currentMaterial, currentGroup, line;
	bool matDirty = false;                              // Indique si un changement de matériau/groupe a eu lieu et doit être enregistré pour la face suivante
    int currentSmoothGroup = 0;
    line.reserve(256);

	// Lecture ligne par ligne du fichier OBJ
    while (std::getline(file, line))
    {
		// std::getline(file, line) retire le \n mais il peut rester un \r à la fin de la ligne si le fichier a des fins de ligne Windows (\r\n). 
        // On gère ça en retirant le \r s'il est présent à la fin de la ligne.
		if (!line.empty() && line.back() == '\r') // Gère les fins de ligne Windows (\r\n) en supprimant le caractère de retour chariot
			line.pop_back();    //	Effet pratique : line.pop_back() enlève ce \r, ce qui permet de traiter correctement les fichiers OBJ même s'ils ont été créés sur Windows avec des fins de ligne CRLF. 
                                // Sans cette étape, le \r pourrait causer des problèmes lors de l'analyse des lignes, par exemple en faisant que les tokenens extraits contiennent un \r à la fin, ce qui les rendrait incorrects.

		if (line.empty() || line[0] == '#') // Ignore les lignes vides et les commentaires
            continue;

        size_t cur=0;
        const std::string token = ParseToken(line, cur);

        if (token=="v")                                       // Vertex
        {
            Vec3f vertex{};
            std::istringstream ss(line.substr(cur));
            ss >> vertex.x >> vertex.y >> vertex.z;
            out.rawVertex.push_back(vertex);
        }
        else if (token=="vt")                                 // coord texture
        {
            Vec2f uv{};
            std::istringstream ss(line.substr(cur));
            ss >> uv.x >> uv.y;
            out.rawUVs.push_back(uv);
            out.hasUVs = true;
        }
        else if (token=="vn")                                 // normal coords
        {
            Vec3f normal{};
            std::istringstream ss(line.substr(cur));
            ss >> normal.x >> normal.y >> normal.z;
            out.rawNormals.push_back(normal);
            out.hasNormals = true;
        }
        else if (token=="mtllib")                           // nom du fichier MTL
        {
            const std::string mn = ParseRestOf(line, cur);
            if (!mn.empty()) 
                out.mtllibs.push_back(mn);
        } 
        else if (token=="usemtl")                           // nom du materiau utilisé
        {
            currentMaterial = ParseRestOf(line, cur);
            matDirty=true;
        }
        else if (token=="g"||token=="o")                    // g = Groupe name, o = Object name
        {
            const std::string name = ParseRestOf(line, cur);
            if (!name.empty() && name!="off")
            {
                currentGroup=name;
                matDirty=true;
            }
        }
        else if (token == "s")                            // smoothing group
        {
            const std::string val = ParseRestOf(line, cur);
            if (val == "off" || val == "0")
                currentSmoothGroup = 0;
            else
            {
                try { currentSmoothGroup = std::stoi(val); }    // Convertit une séquence de caractères en entier
                catch (...) { currentSmoothGroup = 0; }
            }
        }
        else if (token=="f")                              // Faces
        {
			// Avant de traiter la face, on vérifie s'il y a eu un changement de matériau ou de groupe qui doit être enregistré pour les faces suivantes
            if (matDirty)
            {
                out.materialChanges.push_back({ out.rawFaces.size(), currentMaterial, currentGroup });
                matDirty = false;
            }

            RawFace face;
            face.reserve(4);
            std::istringstream ss(line.substr(cur));
            std::string vtoken;
           
            // Composition de chaque face avec 4 côtés : v0/vt0/vn0 v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
            // index Face, UV et Normal de chaque face 
			while (ss >> vtoken)  // lit le prochain token de la ligne (ex: "v0/vt0/vn0") et le stocke dans vtoken
            {
                
                face.push_back(ParseVertex(vtoken,
                                            static_cast<int>(out.rawVertex.size()),
                                            static_cast<int>(out.rawUVs.size()),
                                            static_cast<int>(out.rawNormals.size())));
            }
          
            // Seules les faces avec au moins 3 sommets sont valides pour la construction du mesh ; les autres sont ignorées
            if (face.size()>=3)
            {
                out.rawFaces.push_back(std::move(face));
                out.faceSmoothingGroups.push_back(currentSmoothGroup);
            }
        }
    }

    file.close();  // ← Fermeture explicite
	return !out.rawVertex.empty() && !out.rawFaces.empty(); // Retourne true si le fichier a été parsé avec succès et contient au moins un vertex et une face
}

//***************************************************************************************

/// <summary>
/// Charge les fichiers MTL référés dans parsed.mtllibs, résout leurs chemins par rapport à baseDir, les charge via MTLLoader en utilisant le ResourceManager fourni et agrége les résultats.
/// </summary>
/// <param name="parsed">Résultat de l'analyse contenant la liste parsed.mtllibs des fichiers de matériaux (.mtl) à charger.</param>
/// <param name="baseDir">Répertoire de base utilisé pour résoudre les chemins des fichiers MTL.</param>
/// <param name="rm">Gestionnaire de ressources utilisé pour charger les matériaux (passé à MTLLoader).</param>
/// <returns>OBJLoader::MaterialMap contenant tous les matériaux chargés, mappés par nom vers leurs handles.</returns>
OBJLoader::MaterialMap OBJLoader::LoadMaterials(const ParseResult& parsed, const std::string& baseDir, ResourceManager& rm)
{
    MaterialMap result;
    namespace fs = std::filesystem;

    for (const auto& mtlName : parsed.mtllibs)
    {
        const std::string full = (fs::path(baseDir) / fs::path(mtlName)).lexically_normal().string();
        
        MaterialMap loaded = MTLLoader::Load(full, baseDir, rm);
        
        for (auto& [name, handle] : loaded)
            result.emplace(std::move(name), handle);
    }

    return result;
}

//***************************************************************************************

/// <summary>
/// Construit un mesh à partir du résultat d'analyse OBJ, applique les matériaux et options, calcule les données de surface et enregistre le mesh dans le ResourceManager.
/// </summary>
/// <param name="parsed">Référence aux donnes produites par le parseur OBJ (positions, normales, UV, faces brutes, changements de matériau, etc.).</param>
/// <param name="matMap">Table de correspondance du nom de matériau vers MaterialHandle utilisé pour attribuer les matériaux aux faces.</param>
/// <param name="options">Options de chargement (inversion de l'ordre de winding,chelle, inversion verticale des UV, génération de normales, etc.).</param>
/// <param name="rm">Référence au ResourceManager utilisé pour enregistrer et retourner le mesh final.</param>
/// <returns>MeshHandle identifiant le mesh enregistré dans le ResourceManager.</returns>
MeshHandle OBJLoader::BuildMesh(const ParseResult& parsed, const MaterialMap& matMap, const OBJLoadOptions& options, ResourceManager& rm) {

    auto mesh = std::make_unique<MeshClass>();                                  // Crée un nouveau mesh vide    
    const bool fileHasNormals = parsed.hasNormals, fileHasUVs = parsed.hasUVs;  // Enregistre si le fichier contient déjà des normales et des coordonnées UV
    const Vec3f zN{ 0,0,0 }; const Vec2f zUV{ 0,0 };                            // Valeurs par défaut : normale nulle et UV nul si l'attribut est absent dans le fichier

    // Structure pour associer à chaque face son matériau et son groupe
    struct FaceCtx
    { 
        MaterialHandle material; 
        std::string groupName;
    };

	// Table de contexte pour chaque face : on associe à chaque face le matériau et le groupe qui lui sont appliqués, en tenant compte des changements de matériau et de groupe dans le fichier
	// Cette table nous permettra de regrouper les faces en plages contiguës (SubMesh) avec le même matériau et groupe, ce qui est plus efficace pour le rendu
	// On parcourt les faces dans l'ordre et on applique les changements de matériau et de groupe au fur et à mesure pour construire le contexte de chaque face
    std::vector<FaceCtx> faceCtx(parsed.rawFaces.size());
    {
        // Parcourt toutes les faces et applique les changements de matériau/groupe
        MaterialHandle matHdl = MaterialHandle::Invalid();
        std::string GpeNm; 
        size_t ci = 0;

        for (size_t faceIdx = 0; faceIdx < parsed.rawFaces.size(); ++faceIdx)
        {
            // Traite chaque changement de matériau qui s'applique à cette face
            while (ci < parsed.materialChanges.size() && parsed.materialChanges[ci].faceIndex <= faceIdx)
            {
                const auto& mc=parsed.materialChanges[ci];
                
                // Cherche le matériau dans la map ; utilise Invalid() s'il n'existe pas
                auto it = matMap.find(mc.materialName);
                matHdl = (it != matMap.end()) ? it->second : MaterialHandle::Invalid();
                
                // Met à jour le groupe s'il n'est pas vide
                if (!mc.groupName.empty()) GpeNm = mc.groupName;
                ++ci;
            }

            // Sauvegarde le contexte (matériau et groupe) pour cette face
            faceCtx[faceIdx] = { matHdl,GpeNm };
        }
    }
	// => en sortie, nous avons une liste de faceCtx[face du fichier OBJ] et contient le matériau et le groupe qui lui sont appliqués, ce qui nous permettra de regrouper les faces en SubMesh plus tard.
    
    // Structure pour représenter une plage contigüe de faces avec même matériau et groupe
    struct SMRange
    { 
        size_t first,last; 
        MaterialHandle matHdl; 
        std::string grp; 
    };


	// Parcourt les faces dans l'ordre et on détecte les changements de matériau ou de groupe pour créer des plages contiguës de faces qui partagent le même matériau et groupe, ce qui correspond à un SubMesh dans notre structure finale.
    std::vector<SMRange> ranges;
    {
        // Parcourt les faces et détecte les changements de matériau ou groupe
        // pour créer des plages contigus (SubMesh)
        size_t start=0;
        for (size_t faceIdx = 1; faceIdx <= parsed.rawFaces.size(); ++faceIdx)
        {
            bool atEnd = (faceIdx == parsed.rawFaces.size());
            
            // Vérifie si le matériau change
            bool mc = !atEnd && faceCtx[faceIdx].material != faceCtx[faceIdx - 1].material;
            
            // Vérifie si le groupe change
            bool gc = !atEnd && faceCtx[faceIdx].groupName != faceCtx[faceIdx - 1].groupName;
            
            // Crée une nouvelle plage si on détecte un changement ou la fin du fichier
            if(atEnd || mc || gc)
            {
                ranges.push_back({ start,faceIdx - 1,faceCtx[start].material,faceCtx[start].groupName });
                start = faceIdx;
            }
        }
    }
	// => en sortie, nous avons une liste de plages de faces (ranges) où chaque plage correspond à un SubMesh potentiel avec un matériau et un groupe uniformes.


    // Table de déduplication des sommets : chaque (position, UV, normale) unique reçoit un index unique
    std::unordered_map<VertexKey,uint32_t,VKHash> vertMap;
    vertMap.reserve(parsed.rawFaces.size() * 3);

    // Traite chaque plage de faces avec matériau/groupe uniforme
    for (const SMRange& range:ranges)
    {
        // Crée un nouveau submesh pour cette plage
        SubMesh sm; 
        sm.startIndex=static_cast<uint32_t>(mesh->indices.size());
        sm.material=range.matHdl;
        sm.groupName=range.grp;

        // Parcourt les faces de cette plage
        for(size_t fi=range.first;fi<=range.last;++fi)
        {
            const RawFace& face = parsed.rawFaces[fi];
            const int fsz = static_cast<int>(face.size());
        
            // Valeur réelle du groupe à stocker dans le mesh (toujours depuis le fichier)
            const int sgStore = fi < parsed.faceSmoothingGroups.size() ? parsed.faceSmoothingGroups[fi] : 0;
            
            // Clé de déduplication : 0 quand le fichier a ses propres normales
            // (elles suffisent à différencier les sommets, pas besoin de splitter)
            const int sgKey = fileHasNormals ? 0 : sgStore;

            // Triangule la face en fan triangle (utile pour les polygones n-sided)
            for(int t=0;t<fsz-2;++t)
            {
                // Crée un triangle : sommet 0, sommet t+1, sommet t+2
                const int ti[3] = { 0,t + 1,t + 2 };

                // Traite les 3 sommets du triangle
                for(int v=0; v<3; ++v)
                {
                    const RawIndex& ri=face[ti[v]];
                    // Clé unique : (position, UV, normale, smoothGroup)
                    // Le smoothGroup sépare les sommets partagés entre groupes différents
                    VertexKey key{ static_cast<int32_t>(ri.pos),static_cast<int32_t>(ri.uv),static_cast<int32_t>(ri.nrm),sgKey };

                    // Cherche si ce sommet a déjà été ajouté au mesh
                    auto it = vertMap.find(key);
                    if(it!=vertMap.end())
                    {
                        // Sommet déjà existant : ajoute son index dans les indices
                        mesh->indices.push_back(it->second);
                    }
                    else
                    {
                        // Nouveau sommet : ajoute les données et enregistre l'index
                        const uint32_t ni = static_cast<uint32_t>(mesh->vertexPositions.size());
                        assert(ri.pos >= 0 && ri.pos < static_cast<int>(parsed.rawVertex.size()));
                        
                        // Ajoute la position
                        mesh->vertexPositions.push_back(parsed.rawVertex[ri.pos]);
                        
                        // Ajoute la normale (fichier ou par défaut zN)
                        mesh->vertexNormals.push_back(fileHasNormals&& ri.nrm >= 0 ? parsed.rawNormals[ri.nrm] : zN);
                        
                        // Ajoute les coordonnées UV (fichier ou par défaut zUV)
                        mesh->vertexUVs.push_back(fileHasUVs&& ri.uv >= 0 ? parsed.rawUVs[ri.uv] : zUV);
                        
                        // Enregistre la correspondance pour déduplication future
                        vertMap[key]=ni;
                        mesh->indices.push_back(ni);
                    }
                };

                // Inverse l'ordre des indices si demandé (change le winding order)
                if(options.flipWindingOrder){
                    size_t last = mesh->indices.size();
                    std::swap(mesh->indices[last - 2], mesh->indices[last - 1]);
                }

                // Enregistre le smoothing group réel de ce triangle dans le mesh
                mesh->faceSmoothingGroups.push_back(sgStore);
            }
        }

        // Calcule le nombre d'indices pour ce submesh et l'ajoute si non vide
        sm.indexCount = static_cast<uint32_t>(mesh->indices.size()) - sm.startIndex;
        if (sm.indexCount > 0) mesh->submeshes.push_back(std::move(sm));
    }

    // *******************************************************************
	// A ce stade, le mesh est construit avec les données du fichier et les matériaux appliqués, 
    //  mais il peut nécessiter un post-traitement selon les options spécifiées (mise à l'échelle, inversion des UV, génération de normales, etc.).

    // 1. Applique le facteur d'échelle si présent
    if (options.scale != 1.f)
        for (auto& p : mesh->vertexPositions)
            p = V3Scl(p, options.scale);

    // 2. Inverse verticalement les coordonnées UV si demandé
    if (options.flipUVsVertically && fileHasUVs)
        for (auto& uv : mesh->vertexUVs)
            uv.y = 1.f - uv.y;

    // 3. Génère les normales si le fichier n'en contenait pas et que l'option est activée
    if(!fileHasNormals && options.generateNormalsIfMissing)
    {
        if(options.generateSmoothNormals) 
            ComputeSmoothNormals(*mesh);
        else
            ComputeFlatNormals(*mesh);
    }

    // 4. Calcule les données par face (normales de face, boîtes englobantes, etc.)
    ComputePerFaceData(*mesh);

    // 5. Calcule la boîte englobante globale du maillage
    mesh->ComputeMeshAABB();

    // Enregistre le mesh dans le ResourceManager et retourne le handle
    return rm.RegisterMesh(std::move(mesh));
}

//***************************************************************************************

/// <summary>
/// Calcule des normales plates (une normale par face) et les stocke dans mesh.normals.
/// </summary>
/// <param name="mesh">R�f�rence au maillage � traiter. La fonction utilise mesh.positions et mesh.indices (triplets d'indices pour des triangles) et �crit des normales unitaires dans mesh.normals, modifiant le maillage en place.</param>
void OBJLoader::ComputeFlatNormals(MeshClass& mesh)
{
	// Initialise toutes les normales à zéro avant de calculer les normales de face
    std::fill(mesh.vertexNormals.begin(), mesh.vertexNormals.end(), Vec3f{ 0,0,0 });
    const size_t tc = mesh.indices.size() / 3;

	// Parcourt chaque triangle du maillage, calcule sa normale de face et l'assigne aux 3 sommets du triangle
    for(size_t t=0;t<tc;++t)
    {
        uint32_t i0 = mesh.indices[t * 3], i1 = mesh.indices[t * 3 + 1], i2 = mesh.indices[t * 3 + 2];
        Vec3f n = V3Norm(V3Cross(V3Sub(mesh.vertexPositions[i1], mesh.vertexPositions[i0]), V3Sub(mesh.vertexPositions[i2], mesh.vertexPositions[i0])));
        mesh.vertexNormals[i0] = mesh.vertexNormals[i1] = mesh.vertexNormals[i2] = n;
    }
}

//***************************************************************************************

/// <summary>
/// Calcule des normales lisses par sommet en accumulant les normales de faces puis en normalisant chaque normale.
/// </summary>
/// <param name="mesh">R�f�rence vers MeshClass contenant positions, indices et normals. normals est r�initialis� puis mis � jour avec les normales liss�es par sommet. Les tableaux positions et indices doivent �tre valides et normals doit avoir la taille appropri�e.</param>
void OBJLoader::ComputeSmoothNormals(MeshClass& mesh)
{
    std::fill(mesh.vertexNormals.begin(),mesh.vertexNormals.end(),Vec3f{0,0,0});
    const size_t tc=mesh.indices.size()/3;
   
    for(size_t t=0;t<tc;++t)
    {
        uint32_t i0=mesh.indices[t*3],i1=mesh.indices[t*3+1],i2=mesh.indices[t*3+2];
        Vec3f w = V3Cross(V3Sub(mesh.vertexPositions[i1], mesh.vertexPositions[i0]), V3Sub(mesh.vertexPositions[i2], mesh.vertexPositions[i0]));
        
        mesh.vertexNormals[i0] = V3Add(mesh.vertexNormals[i0], w);
        mesh.vertexNormals[i1] = V3Add(mesh.vertexNormals[i1], w);
        mesh.vertexNormals[i2] = V3Add(mesh.vertexNormals[i2], w);
    }
    for(auto& n:mesh.vertexNormals) n=V3Norm(n);
}

//***************************************************************************************

/// <summary>
/// Calcule et met à jour les données par face d'un maillage : normales de face et boîtes englobantes alignées sur les axes (AABB), et initialise les flags, cosLights et indices de matériau.
/// </summary>
/// <param name="mesh">Référence au maillage à traiter. mesh.indices et mesh.vertexPositions sont lus ; mesh.faceNormals, mesh.faceAABBs, mesh.faceFlags, mesh.cosLights et mesh.matIndices sont redimensionnés et remplis.</param>
void OBJLoader::ComputePerFaceData(MeshClass& mesh)
{
    const size_t tc = mesh.indices.size() / 3;
    mesh.faceNormals.resize(tc);
    mesh.faceAABBs.resize(tc);
    mesh.faceFlags.assign(tc, 0u);
    mesh.cosLights.assign(tc, 0.f);
    mesh.matIndices.assign(tc, 0u);

	// Assure que faceSmoothingGroups a la bonne taille pour stocker les groupes de lissage de chaque face
    if (mesh.faceSmoothingGroups.size() != tc)
        mesh.faceSmoothingGroups.assign(tc, 0);

	// Parcourt chaque face (triangle) du maillage pour calculer sa normale et son AABB
    for(size_t t=0;t<tc;++t)
    {
        uint32_t i0 = mesh.indices[t * 3], i1 = mesh.indices[t * 3 + 1], i2 = mesh.indices[t * 3 + 2];
        const Vec3f& p0 = mesh.vertexPositions[i0], & p1 = mesh.vertexPositions[i1], & p2 = mesh.vertexPositions[i2];
        
        mesh.faceNormals[t] = V3Norm(V3Cross(V3Sub(p1, p0), V3Sub(p2, p0)));
        mesh.faceAABBs[t].min = V3Min(V3Min(p0, p1), p2);
        mesh.faceAABBs[t].max = V3Max(V3Max(p0, p1), p2);
    }
}
//***************************************************************************************

/// <summary>
/// Extrait le token suivant d'une ligne en sautant les espaces et tabulations.
/// </summary>
/// <param name="line">La cha�ne � analyser.</param>
/// <param name="cur">Indice (r�f�rence) de d�part dans la cha�ne ; apr�s l'appel, positionn� juste apr�s le token extrait (ou � la fin de la ligne).</param>
/// <returns>Le token extrait (sous-cha�ne) commen�ant au premier caract�re non blanc trouv� � partir de cur.</returns>
std::string OBJLoader::ParseToken(const std::string& line, size_t& cur)
{
	// Skip les espaces et tabulations - D�termine le debut de la ligne
    while(cur < line.size() && (line[cur] ==' ' || line[cur] == '\t')) 
        ++cur;
    size_t s = cur; 

    // Détermine la fin de la ligne
    while(cur < line.size() && line[cur] != ' ' && line[cur] != '\t')
        ++cur;
    
    return line.substr(s,cur-s);
}

//***************************************************************************************

/// <summary>
/// Retourne la partie restante de la cha�ne � partir de l'indice cur, en supprimant les espaces et tabulations en d�but et en fin.
/// </summary>
/// <param name="line">La ligne source (r�f�rence constante).</param>
/// <param name="cur">Indice de d�part dans la ligne; si cur >= line.size() la fonction retourne une cha�ne vide.</param>
/// <returns>Une std::string contenant la sous-cha�ne extraite, sans espaces ni tabulations en t�te ni en queue.</returns>
std::string OBJLoader::ParseRestOf(const std::string& line, size_t cur)
{
	// Skip les espaces et tabulations - D�termine le debut de la partie restante
    while (cur < line.size() && (line[cur] == ' ' || line[cur] == '\t'))
        ++cur;
    size_t end = line.size();
    
	// Détermine la fin de la partie restante en supprimant les espaces et tabulations en fin de ligne
    while (end > cur && (line[end - 1] == ' ' || line[end - 1] == '\t'))
        --end;

    return line.substr(cur,end-cur);
}

//***************************************************************************************

/// <summary>
// Analyse un jeton de sommet OBJ (formats "v", "v/vt", "v//vn", "v/vt/vn") et résout les indices en utilisant les compteurs fournis. 
/// </summary>
/// <param name="tokenJeton de face OBJ � analyser (ex. "1", "1/2", "1//3", "1/2/3")."></param>
/// <param name="pcContexte/compteur utilis� par ResolveOBJ pour r�soudre l'indice de position (par ex. nombre total de positions ou base pour indices n�gatifs)."></param>
/// <param name="ucContexte/compteur utilis� par ResolveOBJ pour r�soudre l'indice des coordonn�es de texture (UV)."></param>
/// <param name="ncContexte/compteur utilis� par ResolveOBJ pour r�soudre l'indice des normales."></param>
/// <returnRawIndex contenant les indices r�solus pour pos, uv et nrm ; les composantes absentes dans le jeton restent � leur valeur par d�faut.s></returns>
OBJLoader::RawIndex OBJLoader::ParseVertex(const std::string& token, int pc, int uc, int nc)
{
    RawIndex ri{};
    const size_t s1 = token.find('/');

	// Si le jeton ne contient pas de '/', il s'agit du format "v" : seul l'indice de position est présent
    if(s1==std::string::npos)
    { 
        ri.pos = ResolveOBJ(std::stoi(token), pc);
    }
    else
    {
		// Le jeton contient au moins un '/' : le format peut être "v/vt", "v//vn" ou "v/vt/vn"
        ri.pos = ResolveOBJ(std::stoi(token.substr(0, s1)), pc);
        const size_t s2 = token.find('/', s1 + 1);
        
		// Si s2 == npos, le format est "v/vt" : il y a un '/' mais pas de second '/' après, donc la partie après le '/' est l'indice UV
        if(s2==std::string::npos)
        {
			// Format "v/vt" : il y a un '/' mais pas de second '/' après, donc la partie après le '/' est l'indice UV
            if (s1 + 1 < token.size()) ri.uv = ResolveOBJ(std::stoi(token.substr(s1 + 1)), uc);
        }
        else
        {
			// Format "v//vn" ou "v/vt/vn" : il y a un second '/' après s1, donc la partie entre s1 et s2 est l'indice UV (si présente) et la partie après s2 est l'indice de normale (si présente)
            if (s2 > s1 + 1) ri.uv = ResolveOBJ(std::stoi(token.substr(s1 + 1, s2 - s1 - 1)), uc);

            // La partie après s2 est l'indice de normale, même si le format est "v//vn" (dans ce cas, la partie entre s1 et s2 est vide et ne sera pas utilisée)
            if (s2 + 1 < token.size()) ri.nrm = ResolveOBJ(std::stoi(token.substr(s2 + 1)), nc);
        }
    }
    return ri;
}

} // namespace LV3
