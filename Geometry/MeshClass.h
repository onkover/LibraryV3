#pragma once
#include <vector>
#include <string>
#include <cstdint>

#include "../Maths/Vectorlib.h"
#include "../Maths/geometry/AABB3d.h"
#include "../Core/Types.h"
#include "../Core/AlignedAllocator.h"   // B3 : SimdVector

#include "FaceView.h"
#include "SubMesh.h"
namespace LV3
{
    class ResourceManager; // forward

    // ── B3 : positions en SoA pour l'etage sommets AVX2 ──────────
    // x[], y[], z[] alignes 64 o, taille PaddedCount() = multiple de 8.
    // Bourrage : couloirs count..padded-1 = DERNIER sommet repete
    //   -> un lot lit toujours des positions reelles (w > 0 sur Inside),
    //   -> aucune boucle scalaire de queue (meme idee que le dernier lot de B4).
    // CUIT une fois par RegisterMesh. vertexPositions (AoS) reste la SOURCE :
    // chemin clippe, normales, AABB, filet Debug. Cout : 12 o par sommet, en double.
    struct PositionsSoA
    {
        SimdVector<float> x, y, z;
        size_t            count = 0;     // vrais sommets (== vertexCount() apres cuisson)
        [[nodiscard]] size_t PaddedCount() const noexcept { return x.size(); }
    };

    class MeshClass {
    public:
        MeshClass()  = default;
        ~MeshClass() = default;
        MeshClass(const MeshClass&)            = delete;
        MeshClass& operator=(const MeshClass&) = delete;
        MeshClass(MeshClass&&)                 = default;
        MeshClass& operator=(MeshClass&&)      = default;

        // ── SoA faces ─────────────────────────────────────────
        std::vector<Vec3f>    faceNormals;
        std::vector<AABB3d>   faceAABBs;
        std::vector<uint8_t>  faceFlags;
        std::vector<float>    cosLights;
        std::vector<uint16_t> matIndices;
        std::vector<int>      faceSmoothingGroups; // un par face, 0 = off

        // ── SoA vertices ──────────────────────────────────────
        std::vector<Vec3f>    vertexPositions;
        std::vector<Vec3f>    vertexNormals;
        std::vector<Vec2f>    vertexUVs;
        PositionsSoA          positionsSoA;   // B3 : copie SoA de vertexPositions, cuite par RegisterMesh

        // ── Connectivité ──────────────────────────────────────        
		std::vector<uint32_t> indices;                              // liste des indices de vertex pour chaque face (3 par triangle, 4 par quad)
        uint8_t               vertsPerFace = 3;

        // ── SubMesh (par matériau) ─────────────────────────────
        std::vector<SubMesh>  submeshes;

        // ── Données froides ───────────────────────────────────
        std::string  name;
        AABB3d       meshAABB;

        // ── API ───────────────────────────────────────────────
		// il n'y a pas de liste de faces explicite, mais le nombre de faces est égal au nombre de normales de face (et d'AABBs, etc.)
        [[nodiscard]] size_t faceCount() const noexcept
        {
            if (indices.empty()) return 0;          // mesh vide : 0 face, quel que soit vertsPerFace
            LV3_ASSERT(vertsPerFace > 0);
            LV3_ASSERT(indices.size() % vertsPerFace == 0);
            return indices.size() / vertsPerFace;
        }

        [[nodiscard]] size_t vertexCount() const noexcept { return vertexPositions.size(); }
        [[nodiscard]] const AABB3d& GetMeshAABB() const noexcept { return meshAABB; }

        void Reserve(size_t faceCount_, size_t vertexCount_);
        void AddFace(uint32_t i0, uint32_t i1, uint32_t i2, uint16_t matIndex=0);
        void AddFaceQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3, uint16_t matIndex=0);
        uint32_t AddVertex(const Vec3f& pos,
                           const Vec3f& normal = Vec3f::Up(),
                           const Vec2f& uv     = Vec2f::Zero());

        void ComputeFaceNormals()  noexcept;
        void ComputeFaceAABBs()    noexcept;
        void ComputeMeshAABB()     noexcept;
        void BakePositionsSoA();             // B3 : AoS -> SoA + bourrage. Appele par RegisterMesh SEUL.

        [[nodiscard]] FaceView GetFaceView(size_t i, Material* mat=nullptr) const noexcept;

        void ResetFaceFlags() noexcept;
        [[nodiscard]] bool IsValid() const noexcept;
    };
} // namespace LV3
