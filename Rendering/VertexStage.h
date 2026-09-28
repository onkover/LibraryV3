#pragma once
// ============================================================
//  Rendering/VertexStage.h — l'etage SOMMETS (chantier 2, phase G)
//
//  Regle : un calcul qui ne depend que du sommet se fait une fois par
//  SOMMET, jamais par COIN. Un sommet partage par 6 faces etait transforme
//  6 fois par vue, avec le meme resultat.
//
//  TransformPositions est le point UNIQUE ou vivra la version AVX2
//  (chantier 4) : un tableau contigu d'entree, un de sortie, aucune branche.
// ============================================================
#include "../Maths/MatrixLib.h"       // Matrix44f, Vec3f, Vec4f
#include "../Core/EngineSettings.h"   // kMaxMeshVerticesHard
#include "Viewport.h"                 // ToRaster : la projection vit desormais ICI
#include <memory>

namespace LV3
{
    // Tampon de sortie de l'etage sommets, reutilise pour chaque (instance, vue).
    // Possede par l'APPELANT (comme fb/db) : jamais de static cache, jamais
    // d'allocation dans la boucle. 65 536 x 16 o = 1 Mio, alloue une fois.
    // Seuls les premiers vertexCount() elements sont touches : pour une roche,
    // 1,8 Kio, qui restent en L1 pendant tout le parcours des faces.
    class ClipSpaceBuffer
    {
    public:
        ClipSpaceBuffer() : m_data(std::make_unique<Vec4f[]>(kMaxMeshVerticesHard)) {}
        ClipSpaceBuffer(const ClipSpaceBuffer&) = delete;
        ClipSpaceBuffer& operator=(const ClipSpaceBuffer&) = delete;

        [[nodiscard]] Vec4f* Data()       noexcept { return m_data.get(); }
        [[nodiscard]] const Vec4f* Data() const noexcept { return m_data.get(); }
        [[nodiscard]] static constexpr size_t Capacity() noexcept { return kMaxMeshVerticesHard; }

    private:
        std::unique_ptr<Vec4f[]> m_data;
    };

    // dst[i] = MulRow(mvp, src[i]) pour i < count. Pas de division par w :
    // le clipping near a besoin de l'espace clip.
    void TransformPositions(const Matrix44f& mvp, const Vec3f* src, size_t count, Vec4f* dst) noexcept;

    // ── Chantier 2b ─────────────────────────────────────────────
    // Sommet PROJETE, ce que consomme le rasterizer :
    //   x, y : pixels (Y deja flippe par Viewport::ToRaster)
    //   z    : z_ndc reverse-Z, interpole affinement
    //   invW : 1/w_clip, denominateur perspectif
    // 16 octets : un quart de ligne de cache, meme empreinte que Vec4f.
    // TYPE DISTINCT de Vec4f : un clip et un raster ne sont jamais interchangeables.
    struct alignas(16) RasterVertex
    {
        float x, y, z, invW;
    };

    static_assert(sizeof(RasterVertex) == 16, "RasterVertex : 16 octets, 4 par ligne de cache");

    // LE SEUL endroit du moteur ou la division par w a lieu.
    // Appele par ProjectPositions (par SOMMET, meshes Inside) ET par
    // EmitClipTriangle (par COIN, apres clipping) : une seule formule,
    // donc une image identique au bit pres entre les deux chemins.
    // Precondition : c.w > 0.
    [[nodiscard]] LV3_FORCEINLINE RasterVertex ProjectClip(const Vec4f& c, const Viewport& vp) noexcept
    {
        const float invW = 1.0f / c.w;
        const Vec3f r = vp.ToRaster({ c.x * invW, c.y * invW, c.z * invW });
        return { r.x, r.y, r.z, invW };
    }

    // Tampon de sortie projete, meme politique que ClipSpaceBuffer :
    // possede par l'APPELANT, alloue une fois, jamais partage entre threads.
    class RasterSpaceBuffer
    {
    public:
        RasterSpaceBuffer() : m_data(std::make_unique<RasterVertex[]>(kMaxMeshVerticesHard)) {}
        RasterSpaceBuffer(const RasterSpaceBuffer&) = delete;
        RasterSpaceBuffer& operator=(const RasterSpaceBuffer&) = delete;

        [[nodiscard]] RasterVertex* Data()       noexcept { return m_data.get(); }
        [[nodiscard]] const RasterVertex* Data() const noexcept { return m_data.get(); }
        [[nodiscard]] static constexpr size_t Capacity() noexcept { return kMaxMeshVerticesHard; }

    private:
        std::unique_ptr<RasterVertex[]> m_data;
    };

    // dst[i] = ProjectClip(MulRow(mvp, src[i]), vp) pour i < count.
    // PRECONDITION : w > 0 pour TOUT sommet. Garantie UNIQUEMENT pour un mesh
    // classe Inside par le frustum. Un mesh Intersect passe par TransformPositions,
    // puis par le clipping near : pour lui, la division n'a de sens qu'APRES.
    void ProjectPositions(const Matrix44f& mvp, const Viewport& vp, const Vec3f* src, size_t count, RasterVertex* dst) noexcept;
}