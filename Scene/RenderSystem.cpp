#include "pch.h"
#include "Scene/RenderSystem.h"

#include "Scene/Registry.hpp"
#include "Scene/Components/Component.hpp"
#include "Ressources/ResourceManager.h"
#include "Geometry/MeshClass.h"
#include "Rendering/Renderer.h"
#include "Rendering/Rasterizer.h"      // MulRow, EdgeFunction, FaceColor
#include "Rendering/ViewData.h"

namespace LV3
{
    void RenderView(Registry& registry, ResourceManager& rm,
        Renderer& renderer, const ViewData& view)
    {
        renderer.SetViewport(view.viewport);        // l'état de la VUE

        for (auto&& [entity, meshComp, transform] : registry.ViewGroup<MeshComponent, TransformComponent>())
        {
            const MeshClass* mesh = rm.GetMesh(meshComp.m_meshHandle);
            if (!mesh || mesh->faceCount() == 0) continue;

            const Matrix44f& modelMatrix = transform.m_worldMatrix;

            if (view.frustum.Classify(mesh->GetMeshAABB().Transformed(modelMatrix))
                == EIntersect::Outside) continue;

            const Matrix44f mvp = modelMatrix * view.viewProjectionMatrix;
            const uint8_t   vpf = mesh->vertsPerFace;

            for (size_t f = 0; f < mesh->faceCount(); ++f)
            {
                const uint32_t base = uint32_t(f) * vpf;

                Vec4f c[4];
                for (uint8_t k = 0; k < vpf; ++k)
                    c[k] = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);

                bool behind = false;
                for (uint8_t k = 0; k < vpf; ++k)
                    if (c[k].w <= view.nearPlane) { behind = true; break; }
                if (behind) continue;

                Vec3f r[4];
                for (uint8_t k = 0; k < vpf; ++k)
                {
                    const float inv = 1.0f / c[k].w;
                    r[k] = view.viewport.ToRaster({ c[k].x * inv, c[k].y * inv, c[k].z * inv });
                }

                if (EdgeFunction(r[0], r[1], r[2]) <= 0.0f) continue;   // backface

                const Color col = FaceColor(int(f));

                // Triangulation en éventail, puis SOUMISSION. Rien d'autre.
                // A savoir que Triangle2D => Agrégat POD, impact négligeable
				// Passer tout en paramètres signifierait que les 4 premiers flottants dans XMM0-XMM3. Les cinq suivants partent sur la pile, plus this et color.
                // Une struct passée par référence est la forme la moins chère, pas la plus chère
                for (uint8_t t = 0; t + 2 < vpf; ++t)
                    renderer.DrawTriangle(
                        Triangle2D{ { r[0].x,     r[0].y     },
                                    { r[t + 1].x,   r[t + 1].y   },     
                                    { r[t + 2].x,   r[t + 2].y   },
                                      r[0].z, r[t + 1].z, r[t + 2].z },
                                     col);
            }
        }
    }
}