    #pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <bit>
#include "../Maths/Vectorlib.h"
#include "../Maths/MatrixLib.h"
#include "FrameBuffer.h"
#include "Viewport.h"

namespace LV3
{
    struct BarycentricWeights { float w0, w1, w2; };   // normalisés, somme == 1

    using FragmentCallback = void(*)(int32_t x, int32_t y,
                                    const BarycentricWeights& bary,
                                    void* userData);

    // ════════════════════════════════════════════════════════════
    //  DÉFINIES ICI : appelées par pixel ou par sommet.
    //  Une fonction inline doit avoir son corps dans le header.
    // ════════════════════════════════════════════════════════════

   // ── Fonction d'arête de Pineda ────────────────────────────
    //  Elle ne travaille QUE sur x et y : le z n'intervient jamais.
    //  Trois services pour le prix d'un :
    //    * sur trois sommets  -> deux fois l'aire signée (backface, normalisation)
    //    * signe sur un pixel -> le pixel est-il du bon côté de l'arête
    //    * valeur / aire      -> coordonnée barycentrique

    // Le NOYAU. Tout le reste y délègue.
    // Forme ANTISYMÉTRIQUE : le point testé p sert de référence, donc
    //     E(b,a,p) == -E(a,b,p)  EXACTEMENT en IEEE 754
    // (les deux produits sont identiques, seul l'ordre de la soustraction change).
    //
    // Bénéfice secondaire : en soustrayant p d'abord, les magnitudes restent
    // locales au pixel au lieu d'être absolues. Supprime l'annulation
    // catastrophique sur les sommets très éloignés du viewport, ce qui arrive
    // systématiquement sur les intersections du plan near.
    //
    // NÉCESSAIRE, mesuré : sans cette forme, 124 trous sur la couture du quad
    // de Test_ClipCoverage. Avec, il en reste 4 — que ClipLess élimine.
    [[nodiscard]] LV3_FORCEINLINE constexpr float EdgeFunction(
        float ax, float ay, float bx, float by, float px, float py) noexcept
    {
        const float ux = ax - px, uy = ay - py;
        const float vx = bx - px, vy = by - py;
        return ux * vy - uy * vx;
    }

    // Arête (a,b) contre un troisième sommet. Idem, 2D ou 3D.
    // backface culling (EmitClipTriangle, EmitForTest) + area dans RasterizeTriangle
    template<typename V>
    [[nodiscard]] LV3_FORCEINLINE float EdgeFunction(const V& a, const V& b, const V& c) noexcept
    {
       return EdgeFunction(a.x, a.y, b.x, b.y, c.x, c.y);
    }

    // boucle pixel du rasterizer (w0, w1, w2 par pixel) 
    template<typename V>
    [[nodiscard]] LV3_FORCEINLINE float EdgeFunction(const V& a, const V& b, float px, float py) noexcept
    {
        return EdgeFunction(a.x, a.y, b.x, b.y, px, py);   // ← délégation RÉTABLIE
    }
    // Vecteur-ligne SANS division par w : le w sert au rejet du near.
    [[nodiscard]] LV3_FORCEINLINE Vec4f MulRow(const Matrix44f& m, const Vec3f& p) noexcept
    {
        return { p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
                 p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
                 p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2],
                 p.x * m[0][3] + p.y * m[1][3] + p.z * m[2][3] + m[3][3] };
    }

    // CONVENTION DE FACE — mesurée, pas devinée. Verrouillée par TNR §Winding.
    //   Front-face LV3 = CCW en espace VUE (main droite, regard -Z).
    //   Viewport::ToRaster inverse Y  =>  CW en espace RASTER
    //   =>  l'aire signée d'une FACE AVANT est NEGATIVE.
    //
    // Le >= rejette aussi l'aire nulle : triangle degenere, rien a dessiner.
    [[nodiscard]] LV3_FORCEINLINE bool IsBackFacing(float signedArea) noexcept
    {
        return signedArea >= 0.0f;
    }

    // Boite des pixels dont le CENTRE (x+1/2, y+1/2) peut etre dans le triangle.
    // x1 / y1 EXCLUSIFS.
    struct PixelBox
    {
        int32_t x0, y0, x1, y1;
        [[nodiscard]] bool     Empty() const noexcept { return x0 >= x1 || y0 >= y1; }
        [[nodiscard]] uint32_t Area()  const noexcept
        {
            return Empty() ? 0u : uint32_t(x1 - x0) * uint32_t(y1 - y0);
        }
    };

    // SOURCE UNIQUE de la boite serree : le rejet precoce (chantier 3b) ET les
    // statistiques de couverture l'utilisent. Deux copies de la formule
    // finiraient par diverger, et la mesure ne parlerait plus du meme objet
    // que le rejet.
    // Boite vide  =>  aucun centre de pixel  =>  aucun pixel couvert : rejet EXACT.
    template<typename V>
    [[nodiscard]] LV3_FORCEINLINE PixelBox TightPixelBox(const V& a, const V& b, const V& c,
        const Viewport& vp) noexcept
    {
/*        PixelBox bx{
            int32_t(std::ceil(std::min({ a.x, b.x, c.x }) - 0.5f)),
            int32_t(std::ceil(std::min({ a.y, b.y, c.y }) - 0.5f)),
            int32_t(std::floor(std::max({ a.x, b.x, c.x }) - 0.5f)) + 1,
            int32_t(std::floor(std::max({ a.y, b.y, c.y }) - 0.5f)) + 1 };*/
            
            // B2 : PAS de std::min({...}) : la STL MSVC en fait un appel hors ligne
            // a __std_max_element_f_ (tableau en pile + call). Forme binaire = vminss/vmaxss.
            const float minX = std::min(a.x, std::min(b.x, c.x));
            const float minY = std::min(a.y, std::min(b.y, c.y));
            const float maxX = std::max(a.x, std::max(b.x, c.x));
            const float maxY = std::max(a.y, std::max(b.y, c.y));
            PixelBox bx{
                int32_t(std::ceil(minX - 0.5f)),
                int32_t(std::ceil(minY - 0.5f)),
                int32_t(std::floor(maxX - 0.5f)) + 1,
                int32_t(std::floor(maxY - 0.5f)) + 1 };

        vp.ClampBox(bx.x0, bx.y0, bx.x1, bx.y1);
        return bx;
    }



    // ════════════════════════════════════════════════════════════
    //  DÉCLARÉES ICI, définies dans R
    // asterizer.cpp : elles bouclent.
    // ════════════════════════════════════════════════════════════

    [[nodiscard]] bool IsTopLeft(const Vec2f& a, const Vec2f& b) noexcept;

    [[nodiscard]] Color FaceColor(int i) noexcept;

    // ── Compteurs de couverture (chantier 3b) : UN enregistrement par triangle.
    //    C1 : declare ICI (et non plus dans un espace anonyme du .cpp) car le noyau
    //    template de Fragment.h enregistre aussi.
#if LV3_RASTER_STATS
    void RecordRasterStats(uint32_t tested, uint32_t tight, uint32_t covered) noexcept;
#define LV3_RASTER_RECORD(tested, tight, covered) ::LV3::RecordRasterStats((tested), (tight), (covered))
#else
    // Les arguments DISPARAISSENT : rien n'est evalue quand le commutateur vaut 0.
#define LV3_RASTER_RECORD(tested, tight, covered) ((void)0)
#endif

// ════════════════════════════════════════════════════════════
//  C1 : PRÉPARATION ET BOUCLE PIXELS, SOURCE UNIQUE
//  Le chemin générique (pointeur de fonction, TNR) et les noyaux
//  spécialisés par shader (Fragment.h) passent par les MÊMES lignes.
// ════════════════════════════════════════════════════════════

// Tout ce que la boucle pixels lit, calculé UNE fois par triangle.
    struct TriangleSetup
    {
        Vec2f    p0, p1, p2;       // ordre NORMALISÉ : aire positive
        PixelBox bb;               // boîte serrée, rognée au viewport (= le scissor)
        float    invArea;          // 1 / aire (positive)
        bool     flipped;          // p1 <-> p2 échangés : w1 et w2 à remettre dans l'ordre d'origine
        bool     tl0, tl1, tl2;    // règle top-left des arêtes (p1,p2), (p2,p0), (p0,p1)
    };

    // false = rien à dessiner (triangle dégénéré ou boîte vide). Même ordre de
    // calcul que l'ancien RasterizeTriangle : aire, échange, boîte, top-left, 1/aire.
    [[nodiscard]] LV3_FORCEINLINE bool SetupTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        const Viewport& vp, TriangleSetup& s) noexcept
    {
        s.p0 = v0; s.p1 = v1; s.p2 = v2;
        float area = EdgeFunction(s.p0, s.p1, s.p2);
        if (area == 0.0f) return false;                       // triangle dégénéré

        s.flipped = (area < 0.0f);                            // toujours vrai pour une face AVANT
        if (s.flipped) { std::swap(s.p1, s.p2); area = -area; }

        s.bb = TightPixelBox(s.p0, s.p1, s.p2, vp);           // source unique, partagée avec le rejet 3b
        if (s.bb.Empty()) return false;

        s.tl0 = IsTopLeft(s.p1, s.p2);
        s.tl1 = IsTopLeft(s.p2, s.p0);
        s.tl2 = IsTopLeft(s.p0, s.p1);
        s.invArea = 1.0f / area;
        return true;
    }

    // Règle top-left, choix fait HORS de la boucle :
    //     w > 0 || (w == 0 && tl)   ≡   tl ? (w >= 0) : (w > 0)
    // Équivalence EXACTE : -0.0 (== 0 et >= 0 tous deux vrais), NaN (tout faux des deux côtés).
    [[nodiscard]] LV3_FORCEINLINE bool InsideEdge(float w, bool tl) noexcept
    {
        return tl ? (w >= 0.0f) : (w > 0.0f);
    }

    // Parcourt la boîte, appelle sink.BeginRow(y) une fois par ligne et
    // sink.Pixel(x, bary) pour chaque pixel couvert. Rend le nombre de pixels couverts.
    // Sink est résolu À LA COMPILATION : aucun appel indirect ici.
    template<class Sink>
    [[nodiscard]] LV3_FORCEINLINE uint32_t ForEachCoveredPixel(const TriangleSetup& s, Sink& sink)
    {
        uint32_t covered = 0;
        for (int32_t y = s.bb.y0; y < s.bb.y1; ++y)
        {
            const float py = float(y) + 0.5f;
            sink.BeginRow(y);                                 // pointeurs de ligne : 1 fois par ligne
            for (int32_t x = s.bb.x0; x < s.bb.x1; ++x)
            {
                const float px = float(x) + 0.5f;

                const float w0 = EdgeFunction(s.p1, s.p2, px, py);
                const float w1 = EdgeFunction(s.p2, s.p0, px, py);
                const float w2 = EdgeFunction(s.p0, s.p1, px, py);

                // && et non & : sur un grand triangle la branche est prévisible,
                // et la sortie au premier échec évite deux tests (mesuré, voir C1).
                if (!(InsideEdge(w0, s.tl0) && InsideEdge(w1, s.tl1) && InsideEdge(w2, s.tl2))) continue;
                ++covered;

                // Si les sommets ont été échangés, w1 et w2 le sont aussi :
                // on les remet dans l'ordre du triangle d'origine.
                const BarycentricWeights bary = s.flipped
                    ? BarycentricWeights{ w0 * s.invArea, w2 * s.invArea, w1 * s.invArea }
                : BarycentricWeights{ w0 * s.invArea, w1 * s.invArea, w2 * s.invArea };

                sink.Pixel(x, bary);
            }
        }
        return covered;
    }

    // ── C0 : ablation par paliers (LV3_ABLATION_PIXELS), deplacee ICI en C1 :
    //    le moteur passe par les noyaux de Fragment.h, plus par RasterizeTriangle.
    #if LV3_ABLATION_PIXELS
        // PUITS defini dans Rasterizer.cpp. volatile : l'ecriture ne peut pas etre supprimee.
        // Forme "s = s + v" : l'affectation composee sur un volatile est depreciee en C++20.
        extern volatile uint32_t g_ablationSink;

        // Palier S : TOUT ce que la preparation produit est consomme. UNE ecriture par triangle.
        LV3_FORCEINLINE void AblationConsumeSetup(const TriangleSetup& s) noexcept
        {
            g_ablationSink = g_ablationSink
                + uint32_t(s.bb.x0) + uint32_t(s.bb.y0) + uint32_t(s.bb.x1) + uint32_t(s.bb.y1)
                + (uint32_t(s.tl0) | (uint32_t(s.tl1) << 1) | (uint32_t(s.tl2) << 2) | (uint32_t(s.flipped) << 3))
                + std::bit_cast<uint32_t>(s.invArea);
        }

        // Palier E : la boucle d'aretes complete, AUCUN fragment. Les barycentriques,
        // jamais lues, disparaissent a l'optimisation ; le compte couvert va au puits.
        struct AblationEdgeSink
        {
            LV3_FORCEINLINE void BeginRow(int32_t) noexcept {}
            LV3_FORCEINLINE void Pixel(int32_t, const BarycentricWeights&) noexcept {}
        };
    #endif
    
    // Rasterisation en espace écran, avec règle top-left.
    // /!\ prend un VIEWPORT, plus une largeur/hauteur : c'est lui le scissor.
    //     Sans ça, une vue déborde sur l'autre en écran splitté.
    void RasterizeTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        const Viewport& vp,
        FragmentCallback onFragment, void* userData);

    void DrawLineClipped(FrameBuffer& fb, const Viewport& vp,
        const Vec3f& a, const Vec3f& b, Color c) noexcept;
}