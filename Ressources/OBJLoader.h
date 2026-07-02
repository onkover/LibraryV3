#pragma once
//#include "pch.h"
#include <string>
#include <vector>
#include <unordered_map>
#include "ResourceHandle.h"
#include "OBJLoadOptions.h"

#include "MathTypes.h"

namespace LV3
{
	// Forward declarations pour éviter les inclusions circulaires
    class ResourceManager;
    class MeshClass;
//    class Material;

    ///// <summary>
    ///// Options pour le chargement de fichiers OBJ.
    ///// </summary>
    //struct OBJLoadOptions
    //{
    //    bool  generateNormalsIfMissing = true;
    //    bool  generateSmoothNormals    = false;
    //    bool  flipWindingOrder         = false;
    //    bool  flipUVsVertically        = true;
    //    float scale                    = 1.0f;
    //};

    /// <summary>
    /// Charge un fichier OBJ, construit le maillage et enregistre les ressources via le ResourceManager.
    /// </summary>
    class OBJLoader
    {
    public:
        OBJLoader() = delete;
        static MeshHandle Load(const std::string& filepath,
                               ResourceManager& rm,
                               const OBJLoadOptions& options = {});
    private:
        struct RawIndex { int pos=-1, uv=-1, nrm=-1; };
        using RawFace = std::vector<RawIndex>;

        /// <summary>
        /// Structure décrivant un changement de matériau appliqué à une face.
        /// </summary>
        struct MaterialChange
        {
            size_t faceIndex;
            std::string materialName, groupName;
        };

        /// <summary>
        /// Contient le résultat du parsing d'un fichier OBJ : positions, normales, UV, faces et informations de matériau.
        /// </summary>
        struct ParseResult
        {
            std::vector<Vec3f>          rawVertex;
            std::vector<Vec3f>          rawNormals;
            std::vector<Vec2f>          rawUVs;
            std::vector<RawFace>        rawFaces;
            std::vector<std::string>    mtllibs;
            std::vector<MaterialChange> materialChanges;
            std::vector<int>            faceSmoothingGroups; // un par rawFace, 0 = off
            bool hasNormals=false, hasUVs=false;
        };
        
		
        using MaterialMap = std::unordered_map<std::string,MaterialHandle>;
        static bool        ParseFile    (const std::string& fp, ParseResult& out);
        static MaterialMap LoadMaterials(const ParseResult& p, const std::string& baseDir, ResourceManager& rm);
        static MeshHandle  BuildMesh    (const ParseResult& p, const MaterialMap& mm, const OBJLoadOptions& opt, ResourceManager& rm);
        static void ComputeFlatNormals  (MeshClass& mesh);
        static void ComputeSmoothNormals(MeshClass& mesh);
        static void ComputePerFaceData  (MeshClass& mesh);
        static std::string ParseToken  (const std::string& line, size_t& cursor);
        static std::string ParseRestOf (const std::string& line, size_t  cursor);
        static RawIndex    ParseVertex (const std::string& token, int pc, int uc, int nc);
    };

} // namespace LV3
