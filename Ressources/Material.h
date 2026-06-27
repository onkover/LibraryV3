#pragma once
#include <string>
#include "ResourceHandle.h"
#include "MathTypes.h"
namespace LibV2 {
    class Material {
    public:
        Material() = default;
        explicit Material(std::string name) : m_name(std::move(name)) {}
        Material(const Material&)            = delete;
        Material& operator=(const Material&) = delete;
        Material(Material&&)                 = default;
        Material& operator=(Material&&)      = default;

        // ── Accesseurs complets ───────────────────────────────
        [[nodiscard]] const std::string& GetName()            const noexcept { return m_name; }
        [[nodiscard]] const Vec3f& GetAmbientColor()          const noexcept { return m_Ka; }
        [[nodiscard]] const Vec3f& GetDiffuseColor()          const noexcept { return m_Kd; }
        [[nodiscard]] const Vec3f& GetSpecularColor()         const noexcept { return m_Ks; }
        [[nodiscard]] const Vec3f& GetEmissiveColor()         const noexcept { return m_Ke; }
        [[nodiscard]] float        GetSpecularExponent()      const noexcept { return m_Ns; }
        [[nodiscard]] float        GetOpacity()               const noexcept { return m_d; }
        [[nodiscard]] int          GetIlluminationModel()     const noexcept { return m_illum; }

        // ── Aliases courts (nommage MTL) ──────────────────────
        [[nodiscard]] const Vec3f& GetKa()    const noexcept { return m_Ka; }
        [[nodiscard]] const Vec3f& GetKd()    const noexcept { return m_Kd; }
        [[nodiscard]] const Vec3f& GetKs()    const noexcept { return m_Ks; }
        [[nodiscard]] const Vec3f& GetKe()    const noexcept { return m_Ke; }
        [[nodiscard]] float        GetNs()    const noexcept { return m_Ns; }
        [[nodiscard]] float        GetD()     const noexcept { return m_d;  }
        [[nodiscard]] int          GetIllum() const noexcept { return m_illum; }

        // ── Handles textures ──────────────────────────────────
        [[nodiscard]] TextureHandle GetDiffuseMap()   const noexcept { return m_diffuseMap;  }
        [[nodiscard]] TextureHandle GetSpecularMap()  const noexcept { return m_specularMap; }
        [[nodiscard]] TextureHandle GetNormalMap()    const noexcept { return m_normalMap;   }
        [[nodiscard]] TextureHandle GetAmbientMap()   const noexcept { return m_ambientMap;  }
        [[nodiscard]] TextureHandle GetOpacityMap()   const noexcept { return m_opacityMap;  }
        [[nodiscard]] TextureHandle GetEmissiveMap()  const noexcept { return m_emissiveMap; }

        // ── Mutateurs (MTLLoader) ─────────────────────────────
        void SetAmbientColor (const Vec3f& v) noexcept { m_Ka=v; }
        void SetDiffuseColor (const Vec3f& v) noexcept { m_Kd=v; }
        void SetSpecularColor(const Vec3f& v) noexcept { m_Ks=v; }
        void SetEmissiveColor(const Vec3f& v) noexcept { m_Ke=v; }
        void SetSpecularExp  (float v)        noexcept { m_Ns=v; }
        void SetOpacity      (float v)        noexcept { m_d =v; }
        void SetIllumModel   (int   v)        noexcept { m_illum=v; }
        void SetDiffuseMap   (TextureHandle h) noexcept { m_diffuseMap =h; }
        void SetSpecularMap  (TextureHandle h) noexcept { m_specularMap=h; }
        void SetNormalMap    (TextureHandle h) noexcept { m_normalMap  =h; }
        void SetAmbientMap   (TextureHandle h) noexcept { m_ambientMap =h; }
        void SetOpacityMap   (TextureHandle h) noexcept { m_opacityMap =h; }
        void SetEmissiveMap  (TextureHandle h) noexcept { m_emissiveMap=h; }

    private:
        std::string m_name;
        Vec3f m_Ka{0.2f,0.2f,0.2f}, m_Kd{0.8f,0.8f,0.8f};
        Vec3f m_Ks{0.f,0.f,0.f},    m_Ke{0.f,0.f,0.f};
        float m_Ns=10.f, m_d=1.f;
        int   m_illum=2;
        TextureHandle m_diffuseMap, m_specularMap, m_normalMap;
        TextureHandle m_ambientMap, m_opacityMap,  m_emissiveMap;
    };
} // namespace LibV2
