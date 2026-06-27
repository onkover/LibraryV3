#pragma once
#include <string>
#include <cstdint>
#include "../Ressources/ResourceHandle.h"
namespace LibV3 
{
    struct SubMesh {
        uint32_t       startIndex = 0u;
        uint32_t       indexCount = 0u;
        MaterialHandle material   = MaterialHandle::Invalid();
        std::string    groupName;
        [[nodiscard]] uint32_t TriangleCount() const noexcept { return indexCount/3u; }
        [[nodiscard]] bool     HasMaterial()   const noexcept { return material.IsValid(); }
    };
} // namespace LibV3
