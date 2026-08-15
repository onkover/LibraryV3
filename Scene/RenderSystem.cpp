#include "pch.h"
#include "Scene/RenderSystem.h"

#include "Scene/Registry.hpp"
#include "Scene/Components/Component.hpp"
#include "Ressources/ResourceManager.h"
#include "Geometry/MeshClass.h"
#include "Rendering/Renderer.h"
#include "Rendering/Rasterizer.h"      // MulRow, EdgeFunction, FaceColor
#include "Rendering/ViewData.h"
#include "Rendering/clipper.h"

namespace LV3
{
    //void RenderView(Registry& registry, ResourceManager& rm,
    //    Renderer& renderer, const ViewData& view)
    //{
    //    renderer.SetViewport(view.viewport);        // l'état de la VUE

    //    for (auto&& [entity, meshComp, transform] : registry.ViewGroup<MeshComponent, TransformComponent>())
    //    {
    //        const MeshClass* mesh = rm.GetMesh(meshComp.m_meshHandle);
    //        if (!mesh || mesh->faceCount() == 0) continue;

    //        const Matrix44f& modelMatrix = transform.m_worldMatrix;

    //        if (view.frustum.Classify(mesh->GetMeshAABB().Transformed(modelMatrix))
    //            == EIntersect::Outside) continue;

    //        const Matrix44f mvp = modelMatrix * view.viewProjectionMatrix;
    //        const uint8_t   vpf = mesh->vertsPerFace;

    //        for (size_t f = 0; f < mesh->faceCount(); ++f)
    //        {
    //            const uint32_t base = uint32_t(f) * vpf;

    //            Vec4f c[4];
    //            for (uint8_t k = 0; k < vpf; ++k)
    //                c[k] = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);

    //            bool behind = false;
    //            for (uint8_t k = 0; k < vpf; ++k)
    //                if (c[k].w <= view.nearPlane) { behind = true; break; }
    //            if (behind) continue;

    //            Vec3f r[4];
    //            for (uint8_t k = 0; k < vpf; ++k)
    //            {
    //                const float inv = 1.0f / c[k].w;
    //                r[k] = view.viewport.ToRaster({ c[k].x * inv, c[k].y * inv, c[k].z * inv });
    //            }

    //            if (EdgeFunction(r[0], r[1], r[2]) <= 0.0f) continue;   // backface

    //            const Color col = FaceColor(int(f));

    //            // Triangulation en éventail, puis SOUMISSION. Rien d'autre.
    //            // A savoir que Triangle2D => Agrégat POD, impact négligeable
				//// Passer tout en paramètres signifierait que les 4 premiers flottants dans XMM0-XMM3. Les cinq suivants partent sur la pile, plus this et color.
    //            // Une struct passée par référence est la forme la moins chère, pas la plus chère
    //            for (uint8_t t = 0; t + 2 < vpf; ++t)
    //                renderer.DrawTriangle(
    //                    Triangle2D{ { r[0].x,     r[0].y     },
    //                                { r[t + 1].x,   r[t + 1].y   },     
    //                                { r[t + 2].x,   r[t + 2].y   },
    //                                  r[0].z, r[t + 1].z, r[t + 2].z },
    //                                 col);
    //        }
    //    }
    //}

    // ── Le seul endroit du moteur où la division par w a lieu ──
    static void EmitClipTriangle(Renderer& renderer, const ViewData& view,
        const ClipVertex& a, const ClipVertex& b,
        const ClipVertex& c, Color col)
    {
        const ClipVertex* v[3] = { &a, &b, &c };

        Vec3f r[3];
        float invW[3];

        for (int k = 0; k < 3; ++k)
        {
            // Après clipping, w > 0 est GARANTI. Pas de garde nécessaire.
            invW[k] = 1.0f / v[k]->clip.w;
            r[k] = view.viewport.ToRaster({ v[k]->clip.x * invW[k],
                                            v[k]->clip.y * invW[k],
                                            v[k]->clip.z * invW[k] });
        }

        // Backface : signe déjà calibré et verrouillé par la TNR (front-face = aire > 0)
        if (IsBackFacing(EdgeFunction(r[0], r[1], r[2]))) return;

        renderer.DrawTriangle(
            RasterTriangle{ { r[0].x, r[0].y }, { r[1].x, r[1].y }, { r[2].x, r[2].y },
                              r[0].z,  r[1].z,  r[2].z,
                              invW[0], invW[1], invW[2] },
            col);
    }

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

                // ── Local → CLIP ──
                ClipVertex cv[4];
                float      d[4];
                for (uint8_t k = 0; k < vpf; ++k)
                {
                    cv[k].clip = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);
                    d[k] = NearDistance(cv[k]);
                }

                // ── Classification de la FACE, une seule fois pour ses 1-2 triangles ──
                bool allOut = true, allIn = true;
                for (uint8_t k = 0; k < vpf; ++k)
                {
                    if (d[k] >= 0.0f) allOut = false;
                    else              allIn = false;
                }
                if (allOut) continue;                      // rejet, coût nul

                const Color col = FaceColor(int(f));

                // ── Éventail en CLIP space ──
                for (uint8_t t = 0; t + 2 < vpf; ++t)
                {
                    const ClipVertex tri[3] = { cv[0], cv[t + 1], cv[t + 2] };

                    if (allIn)
                    {
                        EmitClipTriangle(renderer, view, tri[0], tri[1], tri[2], col);
                    }
                    else
                    {
                        ClipVertex poly[kMaxClipVertices];
                        const int32_t n = ClipTriangleNear(tri, poly);

                        // éventail du polygone clippé — winding PRÉSERVÉ par l'ordre d'émission
                        for (int32_t q = 1; q + 1 < n; ++q)
                            EmitClipTriangle(renderer, view, poly[0], poly[q], poly[q + 1], col);
                    }
                }
            }

        }
    }

}