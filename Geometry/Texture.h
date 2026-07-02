#pragma once
#include "../Core/Types.h"
#include <string>
#include <vector>
#include <memory>

namespace LV3 
{
    struct Texture {
        int width=0, height=0, bpp=4;
        std::vector<uint32_t> pixels;
        [[nodiscard]] bool IsValid() const noexcept { return width>0&&height>0&&!pixels.empty(); }
        [[nodiscard]] ARGB4f Sample(float u, float v) const noexcept;
        static std::unique_ptr<Texture> Load(const std::string& path);
    };
} // namespace LV3
