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
#include "Core/Profiler.h"            // hors pch.h : inclusion explicite (phase G)
#include "Rendering/VertexStage.h"

namespace LV3
{
    // Le profileur et la chaine doivent parler du meme nombre de niveaux.
    // Verifie ICI : seul endroit qui connait les deux (Core ne depend pas de Ressources).
    static_assert(uint8_t(LV3::EProfCounter::LodLevel3) - uint8_t(LV3::EProfCounter::LodLevel0) + 1
        == LV3::LodChain::kMaxLevels, "un compteur LodLevel par niveau possible d'une chaine");


#if LV3_DEBUG

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


    void ReportCullStats()
    {

        const int viewsThisFrame = g_viewsThisFrame;
        g_viewIndex = 0;                     // remise à zéro POUR LA FRAME SUIVANTE
        g_viewsThisFrame = 0;

        if (++g_frames < 60) return;

        Logger::info("[CULL] " + std::to_string(g_frames) + " frames, " + std::to_string(viewsThisFrame) + " vues/frame");

        for (int v = 0; v < kMaxViews; ++v)
        {
            const int total = g_perView[v].Total();
            if (total == 0) continue;
            const float inv = 100.0f / float(total);
            Logger::info("   vue " + std::to_string(v) + " : "
                + std::to_string(total) + " tests | Inside "
                + std::to_string(int(g_perView[v].inside * inv)) + "% | Intersect "
                + std::to_string(int(g_perView[v].intersect * inv)) + "% | Outside "
                + std::to_string(int(g_perView[v].outside * inv)) + "%");
            g_perView[v].Reset();
        }
        g_frames = 0;
    }
#endif



     // ── AVAL COMMUN aux deux chemins : backface, compteur, rejet 3b, soumission.
    //    SOURCE UNIQUE : le chemin Inside (2b) et le chemin clippe y passent tous deux.
    static LV3_FORCEINLINE void EmitRasterTriangle(Renderer& renderer, const ViewData& view, const RasterVertex& a, const RasterVertex& b, const RasterVertex& c, Color col)
    {
        // Backface : signe calibre et verrouille par la TNR (front-face = aire < 0 en raster)
        if (IsBackFacing(EdgeFunction(a, b, c))) return;

        // APRES le backface, AVANT le rejet 3b : sens de la Reference F conserve.
        LV3_PROF_COUNT(EProfCounter::TrisRasterized, 1);

        // CHANTIER 3b : rejet precoce EXACT (sauf Wireframe : Bresenham trace le contour)
        if (view.mode != ERenderMode::Wireframe &&
            TightPixelBox(a, b, c, view.viewport).Empty())
        {
            LV3_PROF_COUNT(EProfCounter::TrisEarlyRejected, 1);
            return;
        }

        // ⚠⚠⚠ ABLATION M7 — CHANTIER 2 — À RETIRER IMMÉDIATEMENT APRÈS LA MESURE ⚠⚠⚠
        #if LV3_ABLATION_RASTER
                (void)renderer; (void)col;
        #else
                renderer.DrawTriangle(
                    RasterTriangle{ { a.x, a.y }, { b.x, b.y }, { c.x, c.y },
                                      a.z,    b.z,    c.z,
                                      a.invW, b.invW, c.invW },
                                    col);
        #endif
    }

    // ── Chemin CLIPPE (meshes Intersect) : projection par COIN, parce que les
    //    sommets crees par le clipping n'existent dans aucun buffer.
    static void EmitClipTriangle(Renderer& renderer, const ViewData& view,
        const ClipVertex& a, const ClipVertex& b, const ClipVertex& c, Color col)
    {
        // Apres clipping, w > 0 est GARANTI. Meme formule que ProjectPositions.
        EmitRasterTriangle(renderer, view,
                            ProjectClip(a.clip, view.viewport),
                            ProjectClip(b.clip, view.viewport),
                            ProjectClip(c.clip, view.viewport), col);
    }

    void RenderView(Registry& registry, ResourceManager& rm, Renderer& renderer, const ViewData& view, ClipSpaceBuffer& clipBuf, RasterSpaceBuffer& rasterBuf) 
    {
        renderer.SetViewport(view.viewport);        // l'état de la VUE
        renderer.SetMode(view.mode);
        renderer.SetDepthDisplayRange(view.depthDisplayRange);  // profondeur de rendu si on le rendu est en mode DEPPH
 
        #if LV3_LOD_STATS
                const ScreenSizeParams ssp = MakeScreenSizeParams(view);   // une fois PAR VUE
        #endif

        #if LV3_DEBUG          
            const int vi = g_viewIndex;          // index de CETTE vue
            ++g_viewIndex;
            ++g_viewsThisFrame;
        #endif

        for (auto&& [entity, meshComp, transform] : registry.ViewGroup<MeshComponent, TransformComponent>())
        {

            // ── 0. FILTRE DE VUE ─────────────────────────────────────────
             // Un visuel de debug peut se declarer invisible depuis SA PROPRE vue.
             // RenderView ignore ce qu'est une camera : il ne connait qu'une regle
             // generique "cet objet est masqué pour la vue qui vient de cette entite
            const DebugVisualComponent* dbg = registry.TryGet<DebugVisualComponent>(entity);

            // Bug 56 (Discussion E) : le filtre sur dbg->m_visible a été retiré d'ici — voir
            // DebugGizmos.cpp::CameraGizmoSystem. Politique actuelle (A5 §4.5, validée) : tous
            // les gizmos restent visibles, seule la teinte distingue la caméra active. La
            // politique « une seule caméra active visible » (A9 §11.2, comportement Unity)
            // reste une dette ouverte et délibérément NON activée — si elle est un jour
            // tranchée, DebugVisualComponent::m_visible existe déjà pour la porter.
            if (dbg && dbg->m_hideForCamera == view.m_sourceCamera) continue;       // par exemple, le gizmo de FPS_Camera est inconditionnellement rejeté de la vue de FPS_Camera
                                                                                    // Une caméra ne voit jamais son propre gizmo, sinon elle rendrait l'intérieur de son propre frustum, tranché par le near plane, plein écran.

            //const MeshClass* mesh = rm.GetMesh(meshComp.m_meshHandle);
            //if (!mesh || mesh->faceCount() == 0) continue;
           

            // ── 1. LA CHAINE : bornes de culling (union des niveaux) et handles des niveaux.
            //    Une ligne de cache, indexee directement par l'id (pas de hachage).
            const LodChain* chain = rm.GetLodChain(meshComp.m_MeshlodChain);
            if (!chain) continue;

            const Matrix44f& modelMatrix = transform.m_worldMatrix;

            // ── 2. CULLING : boite de la CHAINE, un niveau grossier peut deborder de L0.
            const EIntersect vis = view.frustum.Classify(chain->bounds.Transformed(modelMatrix));

            // Un mesh CLASSIFIE, pour toutes les vues cumulees. Le rapport
            // MeshesCulled / MeshesTested donne l'efficacite du frustum culling.
            LV3_PROF_COUNT(EProfCounter::MeshesTested, 1);

            #if LV3_DEBUG
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

                if (vis == EIntersect::Outside)
                {
                    LV3_PROF_COUNT(EProfCounter::MeshesCulled, 1);
                    continue;
                }

            // ── 3. SELECTION DU NIVEAU (A13 bis) : par (instance, vue), JAMAIS stockee.
            //    mvp remonte ici : la selection a besoin du w du centre.
            const Matrix44f mvp = modelMatrix * view.viewProjectionMatrix;

            const AABB3d& box = chain->bounds;
            const float   rW = BoundingRadiusWorld(box, modelMatrix);   // rayon monde de la sphere englobante
            const Vec4f   cClip = MulRow(mvp, box.Center());            // centre en espace clip
            const float   wC = cClip.w;                                 // w_clip de son centre

            // Rayon apparent / tau (lodParams.kPx est deja divise par tau). 
            // Calcule UNE fois : il sert au rendu en point ET a la selection du niveau.
            const float   rOverTau = ProjectedRadiusPx(view.lodParams, wC, rW);

            // ── 3a. RENDU EN POINT (etape 4) : instance sous-pixel -> UN pixel, aucun sommet.
            //    Inside seulement : w > 0 garanti pour le centre. Une instance Intersect
            //    (au bord de l'ecran) garde la geometrie complete : cas rare.
            //    +inf (touche le near) et NaN : comparaison fausse -> geometrie complete.
            if (vis == EIntersect::Inside && rOverTau < view.pointRadiusOverTau)
            {
                LV3_PROF_COUNT(EProfCounter::PointSplats, 1);
                renderer.DrawPoint(ProjectClip(cClip, view.viewport), dbg ? dbg->m_color : FaceColor(0));
                continue;
            }

            // q' = q / tau, en pixels par unite LOCALE : rayon apparent (deja divise par tau)
            // / rayon local. +inf (touche le near) ou NaN (boite ponctuelle)
            // -> aucune comparaison vraie dans SelectLodLevel -> L0.
            const float    qOverTau = rOverTau / box.Extent().length();

            const uint32_t level = SelectLodLevel(*chain, qOverTau);

            // Compte AVANT tout 'continue' : l'invariant  LodLevel* + PointSplats == MeshesTested - MeshesCulled
            // ne tient que si chaque paire non cullee est comptee exactement une fois.
            LV3_PROF_COUNT(EProfCounter(uint8_t(EProfCounter::LodLevel0) + level), 1);

            // ── 4. LE MESH du niveau choisi, et seulement maintenant ses verifications.
            const MeshClass* mesh = rm.GetMesh(chain->levels[level]);
            if (!mesh || mesh->faceCount() == 0) continue;

            // Garanti par RegisterMesh. Le if est un filet memoire actif en Release
            // (meme patron que DestroyEntity, bug 53) : un seul test par mesh, gratuit.
            const size_t nVerts = mesh->vertexCount();
            LV3_ASSERT(nVerts <= ClipSpaceBuffer::Capacity());
            if (nVerts > ClipSpaceBuffer::Capacity()) continue;

            // Faces qui ENTRENT dans le pipeline : celles du niveau CHOISI.
            LV3_PROF_COUNT(EProfCounter::FacesSubmitted, mesh->faceCount());

            // Inside ⇒ l'AABB monde est entièrement dans les 6 plans, donc devant le near :
            // w > 0 pour CHAQUE sommet. C'est cette garantie qui autorise le chantier 2b.
            const bool needsNearClip = (vis == EIntersect::Intersect);

            const uint8_t   vpf = mesh->vertsPerFace;
            const size_t    nFaces = mesh->faceCount();

            #if LV3_LOD_STATS
            {
                // MEMES rW et wC que la selection : l'instrument mesure ce que la selection voit.
                const float rPx = ProjectedRadiusPx(ssp, wC, rW);
                LV3_ASSERT(rPx >= 0.0f);   // echoue aussi sur NaN ; +inf admis (touche le near)

                // Tranche sans branche : 0 (<1), 1 (<4), 2 (<16), 3 (>=16 ou inf).
                const uint8_t b = uint8_t(rPx >= 1.0f) + uint8_t(rPx >= 4.0f) + uint8_t(rPx >= 16.0f);
                LV3_PROF_COUNT(EProfCounter(uint8_t(EProfCounter::InstPx0to1) + b), 1);
                // Faces du niveau CHOISI : la somme des tranches reste egale a FacesSubmitted.
                LV3_PROF_COUNT(EProfCounter(uint8_t(EProfCounter::FacesPx0to1) + b), nFaces);
            }
            #endif

        //    // La chaine : bornes de culling (union des niveaux) et handles des niveaux.
        //    // Une ligne de cache, indexee directement par l'id (pas de hachage).
        //    const LodChain* chain = rm.GetLodChain(meshComp.m_MeshlodChain);
        //    if (!chain) continue;

        //    // 4d-2 : toujours L0. L'etape 4d-3 remplacera ce 0 par SelectLodLevel.
        //    const MeshClass* mesh = rm.GetMesh(chain->levels[0]);
        //    if (!mesh || mesh->faceCount() == 0) continue;


        //    // Garanti par RegisterMesh. Le if est un filet memoire actif en Release
        //    // (meme patron que DestroyEntity, bug 53) : un seul test par mesh, gratuit.
        //    const size_t nVerts = mesh->vertexCount();
        //    LV3_ASSERT(nVerts <= ClipSpaceBuffer::Capacity());
        //    if (nVerts > ClipSpaceBuffer::Capacity()) continue;

        //    const Matrix44f& modelMatrix = transform.m_worldMatrix;

        //    //const EIntersect vis = view.frustum.Classify(mesh->GetMeshAABB().Transformed(modelMatrix));
        //    //            
        //    // Boite de la CHAINE, pas du mesh : un niveau grossier peut deborder de L0.
        //    // (Chaine implicite : bounds == boite du mesh, resultat identique a avant.)
        //    const EIntersect vis = view.frustum.Classify(chain->bounds.Transformed(modelMatrix));

        //    // Un mesh CLASSIFIE, pour toutes les vues cumulees. Le rapport
        //    // MeshesCulled / MeshesTested donne l'efficacite du frustum culling.
        //    LV3_PROF_COUNT(EProfCounter::MeshesTested, 1);

        //#if LV3_DEBUG
        //    if (vi < kMaxViews)
        //    {
        //        switch (vis)
        //        {
        //        case EIntersect::Inside:    ++g_perView[vi].inside;    break;
        //        case EIntersect::Intersect: ++g_perView[vi].intersect; break;
        //        case EIntersect::Outside:   ++g_perView[vi].outside;   break;
        //        }
        //    }
        //#endif

        //    if (vis == EIntersect::Outside)
        //    {
        //        LV3_PROF_COUNT(EProfCounter::MeshesCulled, 1);
        //        continue;
        //    }

        //    // Faces qui ENTRENT dans le pipeline. faceCount() est un simple
        //    // accesseur : rappel de la regle, jamais d'expression a effet de bord
        //    // dans LV3_PROF_COUNT, elle disparaitrait quand LV3_PROFILE vaut 0.
        //    LV3_PROF_COUNT(EProfCounter::FacesSubmitted, mesh->faceCount());
        //   
        //    // Inside ⇒ l'AABB monde est entièrement dans les 6 plans, donc devant le near :
        //    // w > 0 pour CHAQUE sommet. C'est cette garantie qui autorise le chantier 2b.
        //    const bool needsNearClip = (vis == EIntersect::Intersect);

        //    const Matrix44f mvp = modelMatrix * view.viewProjectionMatrix;
        //    const uint8_t   vpf = mesh->vertsPerFace;
        //    const size_t    nFaces = mesh->faceCount();

        //    #if LV3_LOD_STATS
        //    {
        //        // Taille apparente de CETTE instance dans CETTE vue.
        //        // Point ① LOD (A12a) : la selection du niveau viendra ICI,
        //        // avec la meme formule. mvp est deja la : w du centre = une MulRow.
        //        const AABB3d& box = mesh->GetMeshAABB();
        //        const float   rW = BoundingRadiusWorld(box, modelMatrix);
        //        const float   wC = MulRow(mvp, box.Center()).w;
        //        const float   rPx = ProjectedRadiusPx(ssp, wC, rW);
        //        LV3_ASSERT(rPx >= 0.0f);   // echoue aussi sur NaN ; +inf admis (touche le near)

        //        // Tranche sans branche : 0 (<1), 1 (<4), 2 (<16), 3 (>=16 ou inf).
        //        const uint8_t b = uint8_t(rPx >= 1.0f) + uint8_t(rPx >= 4.0f) + uint8_t(rPx >= 16.0f);
        //        LV3_PROF_COUNT(EProfCounter(uint8_t(EProfCounter::InstPx0to1) + b), 1);
        //        LV3_PROF_COUNT(EProfCounter(uint8_t(EProfCounter::FacesPx0to1) + b), nFaces);
        //    }
        //    #endif


            // ── Teinte : invariante pour toute l'entite, evaluee UNE fois ──
            const bool  hasTint = (dbg != nullptr);
            const Color tint = hasTint ? dbg->m_color : Color{};


            // Meme sens pour les deux chemins : un MulRow par sommet.
            LV3_PROF_COUNT(EProfCounter::VertsTransformed, nVerts);

            // ── CHEMIN INSIDE (chantier 2b) : MulRow + /w + ToRaster, UNE fois par sommet.
            //    Plus aucune division dans la boucle des faces : elles ne font que LIRE.
            if (!needsNearClip)
            {
                LV3_PROF_COUNT(EProfCounter::FacesInside, nFaces);

                ProjectPositions(mvp, view.viewport, mesh->vertexPositions.data(), nVerts, rasterBuf.Data());
                const RasterVertex* const rv = rasterBuf.Data();

                for (size_t f = 0; f < nFaces; ++f)
                {
                    const uint32_t* const idx = &mesh->indices[f * vpf];
                    LV3_ASSERT(idx[0] < nVerts);   // un indice hors bornes lirait un sommet d'une AUTRE instance
                    const Color col = hasTint ? tint : FaceColor(int(f));

                    // Eventail : meme ordre d'emission que le chemin clippe, winding PRESERVE.
                    for (uint8_t t = 0; t + 2 < vpf; ++t)
                    {
                        LV3_ASSERT(idx[t + 1] < nVerts && idx[t + 2] < nVerts);
                        EmitRasterTriangle(renderer, view, rv[idx[0]], rv[idx[t + 1]], rv[idx[t + 2]], col);
                    }
                }
                continue;   // entite suivante : le chemin Intersect ne la concerne pas
            }

            // ── CHEMIN INTERSECT : inchange. Espace clip, clipping near, projection par coin.
            TransformPositions(mvp, mesh->vertexPositions.data(), nVerts, clipBuf.Data());
            const Vec4f* const clip = clipBuf.Data();

            for (size_t f = 0; f < nFaces; ++f)
            {
                const uint32_t base = uint32_t(f) * vpf;

                ClipVertex cv[4];
                for (uint8_t k = 0; k < vpf; ++k)
                {
                    const uint32_t vi_loc = mesh->indices[base + k];
                    LV3_ASSERT(vi_loc < nVerts);
                    cv[k].clip = clip[vi_loc];
                }

                // Distances au near : ce chemin est TOUJOURS un mesh Intersect.
                bool allIn = true, allOut = true;
                for (uint8_t k = 0; k < vpf; ++k)
                {
                    if (NearDistance(cv[k]) >= 0.0f) allOut = false;
                    else                              allIn = false;
                }
                if (allOut) continue;

                const Color col = hasTint ? tint : FaceColor(int(f));

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

                        // eventail du polygone clippe — winding PRESERVE par l'ordre d'emission
                        for (int32_t q = 1; q + 1 < n; ++q)
                            EmitClipTriangle(renderer, view, poly[0], poly[q], poly[q + 1], col);
                    }
                }
            }

        }
    }
}