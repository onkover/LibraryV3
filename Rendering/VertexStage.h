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
}