#include "pch.h"
#include "MTLLoader.h"
#include "../Geometry/Material.h"
#include "ResourceManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace LibV3 {

MaterialMap MTLLoader::Load(const std::string& mtlPath, const std::string& baseDir, ResourceManager& rm)
{
    MaterialMap result;
    std::ifstream file(mtlPath);
    if (!file.is_open()) return result;

    std::unique_ptr<Material> pCurrentMat;
    std::string currentName, line;
    line.reserve(256);

	// Fonction lambda pour enregistrer le matériau courant dans le ResourceManager et réinitialiser les variables de suivi
    auto flush=[&]()
    {
        if (!pCurrentMat) return;
        result[currentName] = rm.RegisterMaterial(std::move(pCurrentMat));
        currentName.clear();
    };

	// Lecture ligne par ligne du fichier MTL
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        size_t cur=0;    
        const std::string tok=ParseToken(line,cur);

        if (tok=="newmtl")
        {
            flush();
            currentName=ParseRestOf(line,cur);
            pCurrentMat =std::make_unique<Material>(currentName);
        }
        else if (!pCurrentMat)
            continue;
        else if (tok=="Ka"||tok=="Kd"||tok=="Ks"||tok=="Ke")
        {
            Vec3f v{};
            std::istringstream ss(line.substr(cur));
            ss >> v.x >> v.y >> v.z;

            if(tok=="Ka") pCurrentMat->SetAmbientColor(v);
            else if(tok=="Kd") pCurrentMat->SetDiffuseColor(v);
            else if(tok=="Ks") pCurrentMat->SetSpecularColor(v);
            else pCurrentMat->SetEmissiveColor(v);
        }
        else if (tok=="Ns") { float v=10.f; std::istringstream ss(line.substr(cur)); ss>>v; pCurrentMat->SetSpecularExp(v); }
        else if (tok=="d")  { float v=1.f;  std::istringstream ss(line.substr(cur)); ss>>v; pCurrentMat->SetOpacity(v); }
        else if (tok=="Tr") { float v=0.f;  std::istringstream ss(line.substr(cur)); ss>>v; pCurrentMat->SetOpacity(1.f-v); }
        else if (tok=="illum"){ int v=2;    std::istringstream ss(line.substr(cur)); ss>>v; pCurrentMat->SetIllumModel(v); }
        else if (tok=="map_Kd") pCurrentMat->SetDiffuseMap (LoadTexture(ResolvePath(baseDir,ParseRestOf(line,cur)),rm));
        else if (tok=="map_Ks") pCurrentMat->SetSpecularMap(LoadTexture(ResolvePath(baseDir,ParseRestOf(line,cur)),rm));
        else if (tok=="map_Ka") pCurrentMat->SetAmbientMap (LoadTexture(ResolvePath(baseDir,ParseRestOf(line,cur)),rm));
        else if (tok=="map_bump"||tok=="bump") pCurrentMat->SetNormalMap(LoadTexture(ResolvePath(baseDir,ParseRestOf(line,cur)),rm));
        else if (tok=="map_d")  pCurrentMat->SetOpacityMap (LoadTexture(ResolvePath(baseDir,ParseRestOf(line,cur)),rm));
    }

    file.close();  // ← Fermeture explicite
    flush();
    return result;
}

std::string MTLLoader::ParseToken(const std::string& line, size_t& cur)
{
    while(cur<line.size()&&(line[cur]==' '||line[cur]=='\t')) ++cur;
    size_t s=cur;
    while(cur<line.size()&&line[cur]!=' '&&line[cur]!='\t') ++cur;
    return line.substr(s,cur-s);
}

std::string MTLLoader::ParseRestOf(const std::string& line, size_t cur)
{
    while(cur<line.size()&&(line[cur]==' '||line[cur]=='\t')) ++cur;
    size_t end=line.size();
    while(end>cur&&(line[end-1]==' '||line[end-1]=='\t')) --end;
    return line.substr(cur,end-cur);
}

std::string MTLLoader::ResolvePath(const std::string& baseDir, const std::string& rel)
{
    if(rel.empty()) return {};
    return (std::filesystem::path(baseDir)/std::filesystem::path(rel)).lexically_normal().string();
}

TextureHandle MTLLoader::LoadTexture(const std::string&, ResourceManager&)
{
    return TextureHandle::Invalid(); // TODO Phase 3
}

} // namespace LibV3
