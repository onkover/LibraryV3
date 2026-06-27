#pragma once
//#include "pch.h"
//#include <cstdint>

namespace LibV3 {
  
    /// <summary>
    /// Handle léger pour une ressource, basé sur un identifiant 32 bits. Fournit validation et opérations de comparaison.
	/// Les handles sont gérés par le ResourceManager, qui attribue des IDs uniques à chaque ressource chargée et les stocke dans des tables de hachage pour un accès rapide.
    /// </summary>
    /// <typeparam name="Tag">Type utilisé comme étiquette pour différencier au niveau du type les catégories de ressources.</typeparam>
    template<typename Tag>
    struct ResourceHandle
    {
        uint32_t id = 0u;
        [[nodiscard]] bool IsValid() const noexcept {return id != 0u; }
        static ResourceHandle Invalid() noexcept { return {0u}; }
        bool operator==(const ResourceHandle& o) const noexcept { return id==o.id; }
        bool operator!=(const ResourceHandle& o) const noexcept { return id!=o.id; }
    };

    // Chaque type de ressource (Mesh, Material, Texture) a son propre tag struct vide pour différencier les handles à la compilation.
    struct MeshTag{};
    struct MaterialTag{};
    struct TextureTag{};
    using MeshHandle     = ResourceHandle<MeshTag>;
    using MaterialHandle = ResourceHandle<MaterialTag>;
    using TextureHandle  = ResourceHandle<TextureTag>;

} // namespace LibV3
