#pragma once
#include <string>
#include <unordered_map>
#include "ResourceHandle.h"
namespace LibV2 {
    class ResourceManager;
    using MaterialMap = std::unordered_map<std::string,MaterialHandle>;
    class MTLLoader {
    public:
        MTLLoader() = delete;
        static MaterialMap Load(const std::string& mtlPath,
                                const std::string& baseDir,
                                ResourceManager& rm);
    private:
        static std::string ParseToken (const std::string& line, size_t& cursor);
        static std::string ParseRestOf(const std::string& line, size_t  cursor);
        static std::string ResolvePath(const std::string& baseDir, const std::string& rel);
        static TextureHandle LoadTexture(const std::string& path, ResourceManager& rm);
    };
} // namespace LibV2
