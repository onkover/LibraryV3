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
   

#ifdef _DEBUG
    namespace   // interne au .cpp : aucun symbole exporté
    {
        struct CullStats
        {
            int inside = 0, intersect = 0, outside = 0;
            void Reset() noexcept { inside = intersect = outside = 0; }
            int  Total() const noexcept { return inside + intersect + outside; }
        };

        constexpr int kMaxViews = 4;
        CullStats g_perView[kMaxViews];      // cumul 60 frames, PAR VUE
        int       g_viewIndex = 0;           // vue en cours dans la frame
        int       g_viewsThisFrame = 0;
        int       g_frames = 0;
        CullStats g_cull;
        CullStats g_accum;
    }
#endif

    void ReportCullStats()
    {
#ifdef _DEBUG
        const int viewsThisFrame = g_viewsThisFrame;
        g_viewIndex = 0;                     // remise à zéro POUR LA FRAME SUIVANTE
        g_viewsThisFrame = 0;

        if (++g_frames < 60) return;

        Logger::log("[CULL] " + std::to_string(g_frames) + " frames, "
            + std::to_string(viewsThisFrame) + " vues/frame");

        for (int v = 0; v < kMaxViews; ++v)
        {
            const int total = g_perView[v].Total();
            if (total == 0) continue;
            const float inv = 100.0f / float(total);
            Logger::log("   vue " + std::to_string(v) + " : "
                + std::to_string(total) + " tests | Inside "
                + std::to_string(int(g_perView[v].inside * inv)) + "% | Intersect "
                + std::to_string(int(g_perView[v].intersect * inv)) + "% | Outside "
                + std::to_string(int(g_perView[v].outside * inv)) + "%");
            g_perView[v].Reset();
        }
        g_frames = 0;
#endif
    }




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

#ifdef _DEBUG
        const int vi = g_viewIndex;          // index de CETTE vue
        ++g_viewIndex;
        ++g_viewsThisFrame;
#endif

        for (auto&& [entity, meshComp, transform] : registry.ViewGroup<MeshComponent, TransformComponent>())
        {

            // ── 0. FILTRE DE VUE ─────────────────────────────────────────
             // Un visuel de debug peut se declarer invisible depuis SA PROPRE vue.
             // RenderView ignore ce qu'est une camera : il ne connait qu'une regle
             // generique "cet objet est masque pour la vue qui vient de cette entite
            const DebugVisualComponent* dbg = registry.TryGet<DebugVisualComponent>(entity);
            if (dbg && dbg->m_hideForCamera == view.m_sourceCamera) continue;


            const MeshClass* mesh = rm.GetMesh(meshComp.m_meshHandle);
            if (!mesh || mesh->faceCount() == 0) continue;

            const Matrix44f& modelMatrix = transform.m_worldMatrix;

            //if (view.frustum.Classify(mesh->GetMeshAABB().Transformed(modelMatrix))
            //    == EIntersect::Outside) continue;
            const EIntersect vis = view.frustum.Classify(mesh->GetMeshAABB().Transformed(modelMatrix));

#ifdef _DEBUG
            if (vi < kMaxViews)
            {
                switch (vis)
                {
                case EIntersect::Inside:    ++g_perView[vi].inside;    break;
                case EIntersect::Intersect: ++g_perView[vi].intersect; break;
                case EIntersect::Outside:   ++g_perView[vi].outside;   break;
                }
            }
#endif


            if (vis == EIntersect::Outside) continue;

            // Inside ⇒ l'AABB monde est entièrement dans les 6 plans, donc devant le near.
            // Aucun triangle ne peut le traverser : le clipping est structurellement inutile.
            const bool needsNearClip = (vis == EIntersect::Intersect);


            const Matrix44f mvp = modelMatrix * view.viewProjectionMatrix;
            const uint8_t   vpf = mesh->vertsPerFace;

            // ── Teinte : invariante pour toute l'entite, evaluee UNE fois ──
            const bool  hasTint = (dbg != nullptr);
            const Color tint = hasTint ? dbg->m_color : Color{};

            for (size_t f = 0; f < mesh->faceCount(); ++f)
            {
                const uint32_t base = uint32_t(f) * vpf;

                ClipVertex cv[4];
                for (uint8_t k = 0; k < vpf; ++k)
                    cv[k].clip = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);

                // Chemin rapide garanti par la classification du MESH,
                // pas redecouvert face par face.
                bool allIn = true;

                if (needsNearClip)
                {
                    float d[4];
                    bool allOut = true;
                    for (uint8_t k = 0; k < vpf; ++k)
                    {
   /*                     const float d = NearDistance(cv[k]);
                        if (d >= 0.0f) allOut = false; else allIn = false;
                        d[k] = d;*/
                        d[k] = NearDistance(cv[k]);
                        if (d[k] >= 0.0f) allOut = false;
                        else              allIn = false;
                    }
                    if (allOut) continue;
                }
                // sinon : allIn reste true, aucune distance calculée


                //const Color col = FaceColor(int(f));
                const Color col = hasTint ? tint : FaceColor(int(f));   // <-- remplace la ligne existante
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