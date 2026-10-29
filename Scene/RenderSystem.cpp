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
#include "Rendering/FaceStage.h"     // B4 : ClassifyFaces8
#include <bit>                       // std::popcount, std::countr_zero
#include <cstring>                   // B3 : std::memcmp (filet Debug)
#include <algorithm>

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

    // ── SOUMISSION : fin commune des chemins Inside scalaire, Inside par lots (B4) et clippe.
    static LV3_FORCEINLINE void SubmitRasterTriangle(Renderer& renderer,
        const RasterVertex& a, const RasterVertex& b, const RasterVertex& c, Color col)
    {
        // ⚠⚠⚠ ABLATION M7 — CHANTIER 2 — À RETIRER IMMÉDIATEMENT APRÈS LA MESURE ⚠⚠⚠
        #if LV3_ABLATION_RASTER
            (void)renderer; (void)a; (void)b; (void)c; (void)col;
        #else
            renderer.DrawTriangle(
                RasterTriangle{ { a.x, a.y }, { b.x, b.y }, { c.x, c.y },
                              a.z,    b.z,    c.z,
                              a.invW, b.invW, c.invW },
                col);
        #endif
    }

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

        SubmitRasterTriangle(renderer, a, b, c, col);

        //// ⚠⚠⚠ ABLATION M7 — CHANTIER 2 — À RETIRER IMMÉDIATEMENT APRÈS LA MESURE ⚠⚠⚠
        //#if LV3_ABLATION_RASTER
        //        (void)renderer; (void)col;
        //#else
        //        renderer.DrawTriangle(
        //            RasterTriangle{ { a.x, a.y }, { b.x, b.y }, { c.x, c.y },
        //                              a.z,    b.z,    c.z,
        //                              a.invW, b.invW, c.invW },
        //                            col);
        //#endif
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

#if LV3_SIMD   // B4 + B5 + C3 : retire en mode scalaire (aucune fonction morte, aucun avertissement)

    // ════════════════════════════════════════════════════════════
    //  CHANTIER B4 — etage FACES par lots de 8
    //  Domaine : triangles (vpf == 3), chemin Inside, hors Wireframe.
    //  Tests en AVX2 (ClassifyFaces8), dessin SCALAIRE des seules survivantes.
    //  Meme ordre d'emission et memes compteurs que la boucle scalaire.
    // ════════════════════════════════════════════════════════════
    static void EmitTriangleList8(Renderer& renderer, const RasterVertex* rv,
        const uint32_t* indices, size_t nFaces, size_t nVerts, const Viewport& vp,
        bool hasTint, Color tint,
        const FacePlanesSoA& planes, const Vec4f& eye, float tau)      // B5
    {
        (void)nVerts;   // lu par le seul filet Debug ci-dessous
        LV3_ASSERT((reinterpret_cast<uintptr_t>(rv) & 15u) == 0);   // exige par _mm_load_ps

        // B5 : la SoA des plans couvre TOUS les lots, bourrage compris.
        LV3_ASSERT(planes.count == nFaces && planes.PaddedCount() >= (nFaces + 7) / 8 * 8);
        LV3_ASSERT(((uintptr_t(planes.nx.data()) | uintptr_t(planes.ny.data())
            | uintptr_t(planes.nz.data()) | uintptr_t(planes.d.data())) & 31u) == 0);

        // B5 : oeil et seuil diffuses UNE fois par (instance, vue), HORS de la boucle des lots.
        const __m256 ex = _mm256_set1_ps(eye.x), ey = _mm256_set1_ps(eye.y);
        const __m256 ez = _mm256_set1_ps(eye.z), ew = _mm256_set1_ps(eye.w);
        const __m256 negTau = _mm256_set1_ps(-tau);

        // Indices du DERNIER lot quand nFaces n'est pas multiple de 8 (voir plus bas).
        uint32_t padIdx[24];

        for (size_t base = 0; base < nFaces; base += 8)     // base = 1re face du lot
        {
            // ── 1. Taille du lot et masque des couloirs VALIDES ─────────────
            // n = 8 sauf pour le dernier lot. valid a n bits a 1 : n = 8 -> 0xFF, n = 3 -> 0x07.
            const size_t    n = std::min<size_t>(8, nFaces - base);
            const uint32_t  valid = (1u << n) - 1u;
            const uint32_t* idx = indices + base * 3;      // 24 indices du lot

            // ── 1b. B5 : face arriere en espace OBJET, AVANT tout gather ─────
            const uint32_t objBack = valid & ObjBackMask8(
                planes.nx.data() + base, planes.ny.data() + base,
                planes.nz.data() + base, planes.d.data() + base, ex, ey, ez, ew, negTau);

            #if LV3_DEBUG
                // Filet B5 (1) : SIMD == scalaire au bit pres. Couloirs VALIDES seulement.
                for (uint32_t k = 0; k < n; ++k)
                {
                    const size_t f = base + k;
                    const float s = ((planes.nx[f] * eye.x + planes.ny[f] * eye.y)
                        + planes.nz[f] * eye.z) + planes.d[f] * eye.w;
                    LV3_ASSERT(bool((objBack >> k) & 1u) == (s < -tau));
                }
            #endif

            LV3_PROF_COUNT(EProfCounter::TrisObjRejected, uint64_t(std::popcount(objBack)));

            // Lot ENTIEREMENT arriere : ni gather, ni test ecran, ni compteur ecran.
            if (objBack == valid) continue;

            // ── 2. Dernier lot incomplet : PAS de boucle scalaire de queue ──
            // On complete les couloirs n..7 en REPETANT la derniere vraie face :
            // ClassifyFaces8 lit toujours 24 indices valides (aucun debordement),
            // et 'valid' efface ensuite les couloirs de bourrage. Un seul chemin de calcul.
            if (n < 8)
            {
                for (size_t j = 0; j < 8; ++j)
                {
                    const size_t src = std::min(j, n - 1) * 3;   // j >= n -> face n-1
                    padIdx[j * 3 + 0] = idx[src + 0];
                    padIdx[j * 3 + 1] = idx[src + 1];
                    padIdx[j * 3 + 2] = idx[src + 2];
                }
                idx = padIdx;
            }

            // ── 3. Les deux tests, 8 faces d'un coup ────────────────────────
            //    C3 : et, si kPix1Path, le test du centre unique des boites 1 x 1.
            Pixel1Lanes8 p1;
            const FaceMasks8 m = ClassifyFacesT8<kPix1Path>(rv, idx, vp, &p1);

            #if LV3_DEBUG
            // Filet Debug : CHAQUE lot est refait en scalaire et compare bit a bit.
            // Toute divergence vectoriel / scalaire leve une assertion sur la frame fautive.
            for (uint32_t k = 0; k < 8; ++k)
            {
                const uint32_t* t = idx + k * 3;
                LV3_ASSERT(t[0] < nVerts && t[1] < nVerts && t[2] < nVerts);
                const bool fr = !IsBackFacing(EdgeFunction(rv[t[0]], rv[t[1]], rv[t[2]]));
                const bool sv = fr && !TightPixelBox(rv[t[0]], rv[t[1]], rv[t[2]], vp).Empty();
                LV3_ASSERT(fr == bool((m.front >> k) & 1u) && sv == bool((m.survive >> k) & 1u));

                // Filet C3 : chaque couloir survivant est refait par le CODE DU NOYAU
                // (Pixel1Reference = SetupTriangle + ForEachCoveredPixel) et compare au bit.
                if constexpr (kPix1Path)
                {
                    if (!sv) continue;
                    const Pixel1Ref ref = Pixel1Reference(rv[t[0]], rv[t[1]], rv[t[2]], vp);
                    LV3_ASSERT(ref.pix1 == bool((p1.pix1 >> k) & 1u));
                    if (!ref.pix1) continue;
                    LV3_ASSERT(ref.x == p1.x[k] && ref.y == p1.y[k]);
                    LV3_ASSERT(ref.covered == bool((p1.covered >> k) & 1u));
                    if (ref.covered)
                        LV3_ASSERT(std::bit_cast<uint32_t>(ref.b.w0) == std::bit_cast<uint32_t>(p1.b0[k])
                            && std::bit_cast<uint32_t>(ref.b.w1) == std::bit_cast<uint32_t>(p1.b1[k])
                            && std::bit_cast<uint32_t>(ref.b.w2) == std::bit_cast<uint32_t>(p1.b2[k]));
                }
            }
            #endif

            // ── 4. Compteurs : memes definitions que EmitRasterTriangle ─────
            //   TrisRasterized    = faces AVANT                  (apres backface)
            //   TrisEarlyRejected = faces AVANT a boite VIDE     (rejet 3b)
            // popcount = nombre de bits a 1 (instruction popcnt sous /arch:AVX2).
            // B5 : une face rejetee en objet est ecartee QUEL QUE SOIT son lot
            // (sinon son sort dependrait de ses 7 voisines).
            const uint32_t keep = valid & ~objBack;
            const uint32_t front = m.front & keep;     // ni bourrage, ni face surement arriere
            uint32_t       surv = m.survive & keep;
            LV3_PROF_COUNT(EProfCounter::TrisRasterized, uint64_t(std::popcount(front)));
            LV3_PROF_COUNT(EProfCounter::TrisEarlyRejected, uint64_t(std::popcount(front & ~surv)));

            // ── 5. Dessin des survivantes : UN appel par lot (C3) ────────────
            // Ordre CROISSANT des couloirs, comme l'ancienne boucle : depth identique.
            // Boite 1 x 1 : fragment direct ou rien ; sinon DrawTriangle (Renderer.cpp).
            
            // Sans kPix1Path, p1.pix1 reste a 0 (initialiseur) : toutes vont a DrawTriangle.
            // Lot sans survivante (V1 : la majorite) : AUCUN appel. Comme l'ancienne boucle
            // while (surv), qui ne coutait rien a vide. Asm C3 : sans ce test, chaque lot vide
            // payait vzeroupper + appel + IsReady + 5 rechargements ymm au retour.
            if (surv == 0) continue;
            renderer.DrawBatch8(rv, idx, surv, p1, hasTint, tint, int32_t(base));
        }
    }
#endif // LV3_SIMD : EmitTriangleList8


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

                //{   // B0b : sous-zone de Render, l'etage SOMMETS seul
                //    LV3_PROF_SCOPE(EProfZone::RenderVerts);
                //    ProjectPositions(mvp, view.viewport, mesh->vertexPositions.data(), nVerts, rasterBuf.Data());
                //}
                
                {   // B0b : sous-zone de Render, l'etage SOMMETS seul
                    LV3_PROF_SCOPE(EProfZone::RenderVerts);
                    #if LV3_SIMD
                        // B3 : 8 sommets par lot depuis la SoA ; ecrit PaddedCount() >= nVerts sommets
                        // (le bourrage n'est jamais indexe : les indices restent < nVerts).
                        const PositionsSoA& soa = mesh->positionsSoA;
                        LV3_ASSERT(soa.count == nVerts);   // cuite par RegisterMesh
                        ProjectPositions8(mvp, view.viewport, soa.x.data(), soa.y.data(), soa.z.data(), soa.PaddedCount(), rasterBuf.Data());
                   #else
                        // Pendant scalaire de B3 : un sommet a la fois depuis l'AoS (la SOURCE).
                        ProjectPositions(mvp, view.viewport, mesh->vertexPositions.data(), nVerts, rasterBuf.Data());
                    #endif
                }

#if LV3_DEBUG && LV3_SIMD
                    // Filet Debug B3 (HORS de la zone mesuree) : chaque sommet est recalcule en scalaire
                    // depuis l'AoS (la SOURCE) et compare octet par octet. Attrape aussi une SoA perimee
                    // (positions modifiees apres RegisterMesh sans nouvelle cuisson).
                    for (size_t i = 0; i < nVerts; ++i)
                    {
                        const RasterVertex ref = ProjectClip(MulRow(mvp, mesh->vertexPositions[i]), view.viewport);
                        LV3_ASSERT(std::memcmp(&ref, &rasterBuf.Data()[i], sizeof(RasterVertex)) == 0);
                    }
                #endif

                const RasterVertex* const rv = rasterBuf.Data();

                // B4 : triangles hors Wireframe -> tests par lots de 8.
                // Wireframe (pas de rejet 3b) et quads (eventail) : boucle scalaire inchangee.
                // LV3_SIMD = 0 : bloc retire, TOUTES les faces prennent la boucle scalaire
                // ci-dessous (pendant scalaire de B4, B5 et C3).
                 #if LV3_SIMD

                        if (vpf == 3 && view.mode != ERenderMode::Wireframe)
                        {
                            //EmitTriangleList8(renderer, rv, mesh->indices.data(), nFaces, nVerts,
                            //    view.viewport, hasTint, tint);
                            //continue;
                            // B5 : l'oeil de la VUE dans le repere de l'OBJET, une fois par (instance, vue).
                            const FacePlanesSoA& planes = mesh->facePlanesSoA;
                            LV3_ASSERT(planes.count == nFaces);   // cuite par RegisterMesh
                            const Vec4f eye = HomogeneousEye(mvp);
                            const float tau = ObjBackThreshold(eye, planes.maxAbsD);

                            EmitTriangleList8(renderer, rv, mesh->indices.data(), nFaces, nVerts,
                                view.viewport, hasTint, tint, planes, eye, tau);

                            #if LV3_DEBUG
                                // Filet B5 (2) : toute face rejetee en objet est VRAIMENT arriere,
                                // jugee par l'aire exacte en double (ni l'ecran float, ni l'objet float).
                                for (size_t f = 0; f < nFaces; ++f)
                                {
                                    const float s = ((planes.nx[f] * eye.x + planes.ny[f] * eye.y)
                                        + planes.nz[f] * eye.z) + planes.d[f] * eye.w;
                                    if (!(s < -tau)) continue;
                                    const uint32_t* t = &mesh->indices[f * 3];
                                    LV3_ASSERT(ExactRasterArea(mvp, view.viewport, mesh->vertexPositions[t[0]],
                                        mesh->vertexPositions[t[1]], mesh->vertexPositions[t[2]]) > -1e-9);
                                }
                            #endif
                            continue;
                        }
                #endif

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
            {   // B0b : meme sous-zone, chemin clippe
                LV3_PROF_SCOPE(EProfZone::RenderVerts);
                TransformPositions(mvp, mesh->vertexPositions.data(), nVerts, clipBuf.Data());
            }

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