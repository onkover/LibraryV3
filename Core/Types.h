#pragma once
//#include <cstdint>
//#include <cstddef>

namespace LV3 {

    using INT8   = int8_t;   using UINT8   = uint8_t;
    using INT16  = int16_t;  using UINT16  = uint16_t;
    using INT32  = int32_t;  using UINT32  = uint32_t;
    using INT64  = int64_t;  using UINT64  = uint64_t;

    struct ARGB4f  { float a=1.f, r=0.f, g=0.f, b=0.f; };
    struct ARGB32  {
        uint32_t value = 0xFF000000u;
        static ARGB32 FromFloats(float a, float r, float g, float b) noexcept;
    };

    enum class ObjectType : uint8_t { Mesh, Light, Camera, World, BSP, Unknown };
    enum class LightType  : uint8_t { Distant, Point, Spot };
    enum class ShadeMode  : uint8_t { Flat, Gouraud, BlinnPhong, Phong, Texture };
    enum class DrawMode   : uint8_t { Solid, Wireframe, Points };

    // ── FaceFlag ─────────────────────────────────────────────
    enum class FaceFlag : uint8_t {
        None      = 0,
        Visible   = 1 << 0,
        Clipped   = 1 << 1,
        Culled    = 1 << 2,   // backface-culled
        InFrustum = 1 << 3,
        Tesselated= 1 << 4
    };
    inline FaceFlag operator|(FaceFlag a, FaceFlag b) noexcept {
        return static_cast<FaceFlag>(static_cast<uint8_t>(a)|static_cast<uint8_t>(b));
    }
    inline FaceFlag operator&(FaceFlag a, FaceFlag b) noexcept {
        return static_cast<FaceFlag>(static_cast<uint8_t>(a)&static_cast<uint8_t>(b));
    }

} // namespace LV3
