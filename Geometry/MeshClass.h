#pragma once
#include <vector>
#include <string>
#include <cstdint>

#include "../Maths/Vectorlib.h"
#include "../Maths/AABB3d.h"
#include "../Core/Types.h"

#include "FaceView.h"
#include "SubMesh.h"
namespace LV3
{
    class ResourceManager; // forward

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

        // ── Connectivité ──────────────────────────────────────        
		std::vector<uint32_t> indices;                              // liste des indices de vertex pour chaque face (3 par triangle, 4 par quad)
        uint8_t               vertsPerFace = 3;

        // ── SubMesh (par matériau) ─────────────────────────────
        std::vector<SubMesh>  submeshes;

        // ── Données froides ───────────────────────────────────
        std::string  name;
        AABB3d       meshAABB;

        // ── API ───────────────────────────────────────────────
		[[nodiscard]] size_t faceCount()   const noexcept { return faceNormals.size(); }    // il n'y a pas de liste de faces explicite, mais le nombre de faces est égal au nombre de normales de face (et d'AABBs, etc.)
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

        [[nodiscard]] FaceView GetFaceView(size_t i, Material* mat=nullptr) const noexcept;

        void ResetFaceFlags() noexcept;
        [[nodiscard]] bool IsValid() const noexcept;
    };
} // namespace LV3
