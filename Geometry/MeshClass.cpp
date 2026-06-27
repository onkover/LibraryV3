#include "pch.h"
#include "MeshClass.h"
#include "Material.h"

namespace LibV3
{

/// <summary>
/// Reserves memory for the mesh data structures.
/// </summary>
/// <param name="fc">Nombre de faces</param>
/// <param name="vc">Nombre de sommets</param>
void MeshClass::Reserve(size_t fc, size_t vc)
{
    // face
    faceNormals.reserve(fc);
    faceAABBs.reserve(fc);
    faceFlags.reserve(fc);
    cosLights.reserve(fc);
    matIndices.reserve(fc);
    faceSmoothingGroups.reserve(fc);
    indices.reserve(fc*vertsPerFace);
    
    // vertex
    vertexPositions.reserve(vc);
    vertexNormals.reserve(vc);
    vertexUVs.reserve(vc);
}

//**************************************************************************************

/// <summary>
/// Adds a vertex to the mesh.
/// </summary>
/// <param name="pos">The position of the vertex.</param>
/// <param name="normal">The normal of the vertex.</param>
/// <param name="uv">The UV coordinates of the vertex.</param>
/// <returns>The index of the added vertex.</returns>
uint32_t MeshClass::AddVertex(const Vec3f& pos, const Vec3f& normal, const Vec2f& uv)
{
    uint32_t idx = static_cast<uint32_t>(vertexPositions.size());

    vertexPositions.push_back(pos);
    vertexNormals.push_back(normal);
    vertexUVs.push_back(uv);

    return idx;
}

//**************************************************************************************

/// <summary>
/// Adds a triangular face to the mesh.
/// </summary>
/// <param name="i0">The index of the first vertex.</param>
/// <param name="i1">The index of the second vertex.</param>
/// <param name="i2">The index of the third vertex.</param>
/// <param name="matIndex">The index of the material for the face.</param>
void MeshClass::AddFace(uint32_t i0, uint32_t i1, uint32_t i2, uint16_t matIndex)
{
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);

	// calcul la normale de la face à partir des positions des sommets
    const Vec3f& p0=vertexPositions[i0];
    const Vec3f& p1=vertexPositions[i1];
    const Vec3f& p2=vertexPositions[i2];
    Vec3f n = (p1 - p0).cross(p2 - p0).normalize();
    faceNormals.push_back(n);

	// calcul l'AABB de la face à partir des positions des sommets
    AABB3d box;
    box.Expand(p0);
    box.Expand(p1);
    box.Expand(p2);
    faceAABBs.push_back(box);
    
	faceFlags.push_back(0u);            // flags init à 0 (face visible, non-culled, non-clipped, etc.)
	cosLights.push_back(0.f);           // cosinus de l'angle entre la normale de la face et la direction de la lumière (calculé plus tard dans ProjectionPass)
	matIndices.push_back(matIndex);     // index du matériau utilisé par la face (pour le rendu)
	faceSmoothingGroups.push_back(0);   // groupe de lissage (0 = off, 1,2,... = groupes actifs pour le lissage des normales)
}

//**************************************************************************************

/// <summary>
/// Adds a quadrilateral face to the mesh.
/// </summary>
/// <param name="i0">The index of the first vertex.</param>
/// <param name="i1">The index of the second vertex.</param>
/// <param name="i2">The index of the third vertex.</param>
/// <param name="i3">The index of the fourth vertex.</param>
/// <param name="matIndex">The index of the material for the face.</param>
void MeshClass::AddFaceQuad(uint32_t i0,uint32_t i1,uint32_t i2,uint32_t i3,uint16_t matIndex)
{
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);
    indices.push_back(i3);
    const Vec3f& p0= vertexPositions[i0]; const Vec3f& p1= vertexPositions[i1];
    const Vec3f& p2=vertexPositions[i2]; const Vec3f& p3=vertexPositions[i3];
    Vec3f n=(p1-p0).cross(p3-p0).normalize();    
    faceNormals.push_back(n);

    AABB3d box;
    box.Expand(p0);
    box.Expand(p1);
    box.Expand(p2);
    box.Expand(p3);
    faceAABBs.push_back(box);

    faceFlags.push_back(0u);
    cosLights.push_back(0.f);
    matIndices.push_back(matIndex);
    faceSmoothingGroups.push_back(0);
}

//**************************************************************************************

/// <summary>
/// Computes the normals for all faces in the mesh.
/// </summary>
void MeshClass::ComputeFaceNormals() noexcept
{
    const size_t fc = faceCount();
    faceNormals.resize(fc);
    
    for (size_t i=0;i<fc;++i)
    {
        const uint32_t b = static_cast<uint32_t>(i) * vertsPerFace;
        faceNormals[i] = (vertexPositions[indices[b + 1]] - vertexPositions[indices[b]]).cross(vertexPositions[indices[b + 2]] - vertexPositions[indices[b]]).normalize();
    }
}

//**************************************************************************************

/// <summary>
/// Computes the AABBs for all faces in the mesh.
/// </summary>
void MeshClass::ComputeFaceAABBs() noexcept
{
    const size_t fc = faceCount();
    faceAABBs.resize(fc);
    
    for (size_t i=0;i<fc;++i)
    {
        faceAABBs[i].Reset();
        const uint32_t b = static_cast<uint32_t>(i) * vertsPerFace;
        for (uint8_t j = 0; j < vertsPerFace; ++j)
                faceAABBs[i].Expand(vertexPositions[indices[b + j]]);
    }
}

//**************************************************************************************

/// <summary>
/// Computes the AABB for the entire mesh.
/// </summary>
void MeshClass::ComputeMeshAABB() noexcept
{
    meshAABB.Reset();
    for (const auto& p : vertexPositions) 
        meshAABB.Expand(p);
}

//**************************************************************************************

/// <summary>
/// Gets a view of a specific face in the mesh.
/// </summary>
/// <param name="i">face number</param>
/// <param name="mat">material for the face</param>
/// <returns>view of the face</returns>
FaceView MeshClass::GetFaceView(size_t i, Material* mat) const noexcept
{
    FaceView fv;
    fv.faceIdx = static_cast<uint32_t>(i);
    fv.vertCount = vertsPerFace;
    fv.flags     = faceFlags[i];
    fv.material  = mat;
    fv.aabb      = faceAABBs[i];
    const uint32_t base = static_cast<uint32_t>(i) * vertsPerFace;
    fv.verts = &vertexPositions[indices[base]];
    fv.norms = &vertexNormals[indices[base]];
    fv.uvs   = &vertexUVs[indices[base]];

    return fv;
}

//**************************************************************************************

/// <summary>
/// Resets the flags for all faces in the mesh.
/// </summary>
void MeshClass::ResetFaceFlags() noexcept
{
    std::fill(faceFlags.begin(), faceFlags.end(), uint8_t{ 0 });
    std::fill(cosLights.begin(), cosLights.end(), 0.0f);
}

//**************************************************************************************

/// <summary>
/// Checks if the mesh is valid.
/// </summary>
/// <returns>true if the mesh is valid, false otherwise</returns>
bool MeshClass::IsValid() const noexcept
{
	if (faceNormals.size() != faceAABBs.size())            return false;  // chaque face doit avoir une normale et une AABB
	if (faceNormals.size() != faceFlags.size())            return false;  // chaque face doit avoir des flags
	if (faceNormals.size() != cosLights.size())            return false;  // chaque face doit avoir un cosLight
	if (faceNormals.size() != matIndices.size())           return false;   // chaque face doit avoir un matIndex
	if (faceNormals.size() != faceSmoothingGroups.size())  return false;  // chaque face doit avoir un groupe de lissage
	if (indices.size() != faceNormals.size() * vertsPerFace) return false;  // le nombre d'indices doit être égal au nombre de faces * nombre de sommets par face
	if (vertexPositions.size() != vertexNormals.size())     return false;  // chaque sommet doit avoir une position, une normale et des UVs
	if (vertexUVs.size() != vertexPositions.size())         return false;  // chaque sommet doit avoir une position, une normale et des UVs
    
	return true;    // tous les tests de validité passés -> le mesh est considéré comme valide
}

} // namespace LibV3
