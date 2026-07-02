#pragma once
#include "../Maths/Vectorlib.h"
#include "../Maths/AABB3d.h"
#include "../Core/Types.h"

namespace LV3
{
    class Material; // forward

    struct FaceView {
        // ── Index et géométrie ────────────────────────────────
        uint32_t faceIdx   = 0;
        uint8_t  vertCount = 3;
        uint8_t  flags     = 0;

        // Pointeurs non-propriétaires (données SoA de MeshClass)
        const Vec3f* verts    = nullptr;
        const Vec3f* norms    = nullptr;
        const Vec2f* uvs      = nullptr;
        Material*    material = nullptr;

        // AABB de la face (copie depuis faceAABBs[faceIdx])
        AABB3d aabb;

        // Coordonnées projetées (écrites par ProjectionPass)
        struct ScreenVert {
            float sx=0.f, sy=0.f, sz=0.f, invW=0.f;
        } screenVerts[4];

        // ── Accesseurs flags ──────────────────────────────────
        LV3_FORCEINLINE bool IsVisible()   const noexcept { return (flags & static_cast<uint8_t>(FaceFlag::Visible))   != 0; }
        LV3_FORCEINLINE bool IsClipped()   const noexcept { return (flags & static_cast<uint8_t>(FaceFlag::Clipped))   != 0; }
        LV3_FORCEINLINE bool IsCulled()    const noexcept { return (flags & static_cast<uint8_t>(FaceFlag::Culled))    != 0; }
        LV3_FORCEINLINE bool IsInFrustum() const noexcept { return (flags & static_cast<uint8_t>(FaceFlag::InFrustum)) != 0; }

        LV3_FORCEINLINE void SetVisible(bool v) noexcept {
            if(v) flags|=static_cast<uint8_t>(FaceFlag::Visible);
            else  flags&=~static_cast<uint8_t>(FaceFlag::Visible);
        }
        LV3_FORCEINLINE void SetClipped(bool v) noexcept {
            if(v) flags|=static_cast<uint8_t>(FaceFlag::Clipped);
            else  flags&=~static_cast<uint8_t>(FaceFlag::Clipped);
        }
        LV3_FORCEINLINE void SetCulled(bool v) noexcept {
            if(v) flags|=static_cast<uint8_t>(FaceFlag::Culled);
            else  flags&=~static_cast<uint8_t>(FaceFlag::Culled);
        }
    };
} // namespace LV3
