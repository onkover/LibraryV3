#pragma once
#include <string>
#include <unordered_map>
#include "ResourceHandle.h"

namespace LV3 
{
    class ResourceManager;
    using MaterialMap = std::unordered_map<std::string,MaterialHandle>;
    class MTLLoader {
    public:
        MTLLoader() = delete;
        // Les chemins de textures sont resolus par rapport au dossier du MTL.
        static MaterialMap Load(const std::string& mtlPath, ResourceManager& rm);

    private:
        static std::string ParseToken (const std::string& line, size_t& cursor);
        static std::string ParseRestOf(const std::string& line, size_t  cursor);
        static std::string ResolvePath(const std::string& baseDir, const std::string& rel);
        static TextureHandle LoadTexture(const std::string& path, ResourceManager& rm);
    };
} // namespace LV3
