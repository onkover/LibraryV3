    #pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <bit>
#include <climits>     // D-a1 : INT32_MIN / INT32_MAX
#include <immintrin.h> // D-a1 : ToFixed (_mm_cvtss_si32), _mm_getcsr
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
    // Giesen method : repli hors garde, LV3_RASTER_FIXED = 0
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

  // ════════════════════════════════════════════════════════════
  //  D-a1 : ARÊTES EN VIRGULE FIXE, AVANCÉES PAR ADDITIONS (Giesen method)
  //
  //  Unités : un sommet X = x * 256 (entier, 1/256 px). Un centre de pixel
  //  (x + 1/2) vaut 256 x + 128. La fonction d'arête de C2,
  //      E(a, b, P) = (Xa - Px)(Yb - Py) - (Ya - Py)(Xb - Px)        (unité 2^-16 px²)
  //  se développe en
  //      E = A' P.x + B' P.y + (Xa Yb - Ya Xb),   A' = Ya - Yb,  B' = Xb - Xa
  //  Aux centres : E(x, y) = 256 (A' x + B' y) + K,  K = 128 (A' + B') + Xa Yb - Ya Xb.
  //
  //  Règle top-left en entiers : couvert  <=>  E + bias >= 0,  bias = tl ? 0 : -1.
  //  On retire les 8 bits sous-pixel de la partie qui varie (cours Lot D, B3) :
  //      K + bias = 256 q + r,  0 <= r < 256  (q = (K + bias) >> 8, décalage ARITHMÉTIQUE)
  //      E'(x, y) = A' x + B' y + q          et       couvert  <=>  E' >= 0   (EXACT)
  //  Un pixel à droite : E' += A'. Une ligne plus bas : E' += B'. Rien d'autre.
  // ════════════════════════════════════════════════════════════

  // Ce que les boucles en virgule fixe lisent, en plus de TriangleSetup.
  // Arêtes dans l'ordre de C1 : 0 = (p1, p2), 1 = (p2, p0), 2 = (p0, p1).
    struct EdgeSetupFx
    {
        int64_t e[3];      // E' au premier pixel de la boîte (bb.x0, bb.y0)
        int64_t a[3];      // pas d'UN pixel vers la droite (A' = Ya - Yb)
        int64_t b[3];      // pas d'UNE ligne vers le bas   (B' = Xb - Xa)

        // D-a1b : ce que le décalage >> 8 a retiré, pour retrouver E EXACT depuis E' :
        //     E = 256 E' + c,   c = r - bias  dans [0, 256]   ->   E / 256 = E' + ofs,  ofs = c / 256
        float   ofs[3];    // c / 256 : EXACT (c entier <= 256, 2^-8 puissance de 2)
        float   invQ;      // 256 / aire = s.invArea * 2^-8 : EXACT (division par une puissance de 2)
        bool    wide;      // true : E' sort de int32 sur la boîte étendue -> voies AVX2 de 64 bits
        // (rempli par PrepareLanesFx, juste avant les voies AVX2)
    };

    // D-a1b : barycentrique d'UNE arête tirée de son E' ENTIER.
    //     b = E / aire = (E' + c/256) * (256 / aire)
    //  float(e) : conversion entier -> float ARRONDIE AU PLUS PROCHE (cvtsi2ss), exacte si |e| < 2^24.
    //  + ofs     : exacte si |E| < 2^24 (E / 256 a 8 bits de fraction), sinon UN arrondi.
    //  * invQ    : UN arrondi.
    //  Pendant AVX2 : BaryFromFixed8 (PixelStage8.h), mêmes opérations, même ordre : bits identiques.
    [[nodiscard]] LV3_FORCEINLINE float BaryFromFixed(int64_t e, float ofs, float invQ) noexcept
    {
        return (float(e) + ofs) * invQ;
    }

    // Les deux largeurs de voie AVX2. Auto = choix de la préparation (fx.wide).
    // Force64 n'existe que pour la TNR : int32 et int64 doivent donner les MÊMES pixels.
    enum class EFixedLanes : uint8_t { Auto, Force64 };

    // Les 3 sommets sont-ils dans la garde ? NaN -> false (comparaison ordonnée).
    [[nodiscard]] LV3_FORCEINLINE bool InFixedGuard(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2) noexcept
    {
        return std::fabs(v0.x) < kFixedGuardPx && std::fabs(v0.y) < kFixedGuardPx
            && std::fabs(v1.x) < kFixedGuardPx && std::fabs(v1.y) < kFixedGuardPx
            && std::fabs(v2.x) < kFixedGuardPx && std::fabs(v2.y) < kFixedGuardPx;
    }
    
    // float pixel -> entier en 1/256 de pixel : UNE instruction, vcvtss2si, qui arrondit selon
    // MXCSR (au plus proche, au pair : le défaut, que LV3 ne change jamais). C'est l'arrondi
    // de SnapToSubpixel. Sur un sommet déjà accroché (étage sommets), x * 256 est un entier :
    // la conversion est EXACTE, quel que soit le mode.
    // Précondition : |v| < kFixedGuardPx  =>  |résultat| < 2^28, tient dans int32.
    [[nodiscard]] LV3_FORCEINLINE int32_t ToFixed(float v) noexcept
    {
        return _mm_cvtss_si32(_mm_set_ss(v * kSubpixelScale));
    }

    // ── Préparation en virgule fixe. Remplit s (comme SetupTriangle) ET fx.
    //    false = rien à dessiner : aire ENTIÈRE nulle, ou boîte vide.
    //    Précondition : InFixedGuard(v0, v1, v2).
    //    Bornes : |X| < 2^28  ->  différences < 2^29  ->  produits < 2^58 : int64 sans débordement.
    //    fx.wide n'est PAS calculé ici : seul le chemin AVX2 en a besoin (PrepareLanesFx).
    [[nodiscard]] LV3_FORCEINLINE bool SetupTriangleFx(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
        const Viewport& vp, TriangleSetup& s, EdgeSetupFx& fx) noexcept
    {
        // 1. ACCROCHAGE : sommets en 1/256 de pixel.
        LV3_ASSERT((_mm_getcsr() & 0x6000u) == 0u && "MXCSR : arrondi au plus proche exige (ToFixed)");
        const int64_t X0 = ToFixed(v0.x), Y0 = ToFixed(v0.y);
        int64_t       X1 = ToFixed(v1.x), Y1 = ToFixed(v1.y);
        int64_t       X2 = ToFixed(v2.x), Y2 = ToFixed(v2.y);

        // 2. AIRE EXACTE (deux fois l'aire, unité 2^-16 px²). Forme de EdgeFunction(a, b, c).
        //    Le SIGNE décide de l'échange : plus aucun arrondi ne peut le retourner.
        int64_t area = (X0 - X2) * (Y1 - Y2) - (Y0 - Y2) * (X1 - X2);
        if (area == 0) return false;                          // dégénéré EN ENTIERS
        
        s.flipped = (area < 0);                               // toujours vrai pour une face AVANT
        // Sommets float pour les barycentriques : ceux REÇUS. Dans le moteur ils sont déjà
        // accrochés (ProjectClip, ProjectPositions8), donc v == X / 256 au bit près.
        s.p0 = v0; s.p1 = v1; s.p2 = v2;
        if (s.flipped)
        { 
            std::swap(X1, X2); 
            std::swap(Y1, Y2); 
            std::swap(s.p1, s.p2);
            area = -area; 
        }

        // 3. BOÎTE DES CENTRES, en entiers. Centre du pixel x = 256 x + 128.
        //    premier x : 256 x + 128 >= minX  <=>  x0 = (minX + 127) >> 8
        //    dernier x : 256 x + 128 <= maxX  <=>  x1 = ((maxX - 128) >> 8) + 1   (exclusif)
        //    Sur des sommets accrochés, c'est EXACTEMENT TightPixelBox (rejet 3b, étage faces).
        //    Min / Max PAR VALEUR : std::min rend une RÉFÉRENCE, et le compilateur en fait des
        //    sauts sur adresses (mesuré : 6 branches imprévisibles). Par valeur : cmov.
        const auto Min = [](int64_t a, int64_t b) noexcept { return b < a ? b : a; };
        const auto Max = [](int64_t a, int64_t b) noexcept { return a < b ? b : a; };
        const int64_t minX = Min(X0, Min(X1, X2)), maxX = Max(X0, Max(X1, X2));
        const int64_t minY = Min(Y0, Min(Y1, Y2)), maxY = Max(Y0, Max(Y1, Y2));
        PixelBox bb{ int32_t((minX + 127) >> kSubpixelBits), 
                     int32_t((minY + 127) >> kSubpixelBits),
                     int32_t((maxX - 128) >> kSubpixelBits) + 1,
                     int32_t((maxY - 128) >> kSubpixelBits) + 1 };

        vp.ClampBox(bb.x0, bb.y0, bb.x1, bb.y1);
        if (bb.Empty()) return false;
        s.bb = bb;

        // 4. 1 / aire, en px² : float(area) arrondi UNE fois, * 2^-16 exact, puis une vraie division.
        //    D-a1b : invQ = 256 / aire, l'échelle des E' : * 2^-8, exact (puissance de 2).
        s.invArea = 1.0f / (float(area) * kSubpixelArea);
        fx.invQ = s.invArea * kSubpixelInv;

        // 5. LES TROIS ARÊTES, ordre de C1 : (p1, p2), (p2, p0), (p0, p1).
        //    Centre du premier pixel de la boîte : P = (256 x0 + 128, 256 y0 + 128).
        const int64_t px = (int64_t(bb.x0) << kSubpixelBits) + 128;
        const int64_t py = (int64_t(bb.y0) << kSubpixelBits) + 128;
        const auto Edge = [&](int64_t xa, int64_t ya, int64_t xb, int64_t yb, int k) noexcept -> bool
            {
                const int64_t A = ya - yb;                        // pas d'un pixel en x
                const int64_t B = xb - xa;                        // pas d'une ligne en y
                // Top-left, en entiers : arête b - a = (B, -A). Même règle que IsTopLeft :
                //   haute = (dy == 0 && dx < 0) ; gauche = (dy > 0)
                const bool tl = (A == 0 && B < 0) || (A < 0);
                // E au centre P : la forme ANTISYMÉTRIQUE de C2, en entiers (exacte).
                const int64_t E = (xa - px) * (yb - py) - (ya - py) * (xb - px);
                // E' = plancher((E + bias) / 256) : >> ARITHMÉTIQUE (C++20), JAMAIS / 256 (contre-exemple).
                const int64_t bias = tl ? 0 : -1;
                const int64_t eb = E + bias;                      // = 256 E' + r,  0 <= r < 256
                fx.e[k] = eb >> kSubpixelBits;
                // D-a1b : le reste r (8 bits bas, & 255 : exact en complément à deux, négatifs compris).
                //   E = 256 E' + r - bias  ->  c = r - bias = r + (tl ? 0 : 1), dans [0, 256].
                fx.ofs[k] = float(int32_t(eb & 255) - int32_t(bias)) * kSubpixelInv;
                fx.a[k] = A;
                fx.b[k] = B;
                return tl;
            };

        s.tl0 = Edge(X1, Y1, X2, Y2, 0);
        s.tl1 = Edge(X2, Y2, X0, Y0, 1);
        s.tl2 = Edge(X0, Y0, X1, Y1, 2);
        fx.wide = false;

        return true;
    }

    // Garantir qu’aucune voie int32 ne déborde. 
    // ── Avant les voies AVX2 seulement : acceptation triviale et choix de la largeur.
    //    Boîte ÉTENDUE : les voies lisent jusqu'à x1 + 6, le dernier pas de bloc mène à
    //    x1 + 14, le dernier pas de ligne à y1. 
    //    Si E' tient dans int32 aux 4 coins de [x0, x1 + 15] x [y0, y1], il y tient partout (E' est affine).
    //    On n'évalue que deux coins par arête : celui de REJET (minimum) et celui d'ACCEPTATION (maximum),
    //    choisis par les signes de A' et B' (cours Lot D, A5 / A6).
    LV3_FORCEINLINE void PrepareLanesFx(const TriangleSetup& s, EdgeSetupFx& fx) noexcept
    {
        const int64_t dxe = int64_t(s.bb.x1) + 15 - s.bb.x0;
        const int64_t dye = int64_t(s.bb.y1) - s.bb.y0;
        bool wide = false;

        for (int k = 0; k < 3; ++k)
        {
            const int64_t ax = fx.a[k] * dxe, by = fx.b[k] * dye;
            const int64_t lo = fx.e[k] + (ax < 0 ? ax : 0) + (by < 0 ? by : 0);    // coin de rejet
            const int64_t hi = fx.e[k] + (ax > 0 ? ax : 0) + (by > 0 ? by : 0);    // coin d'acceptation

            // D-a1b : PLUS d'acceptation triviale (arête remplacée par 0). Les voies portent
            // désormais les barycentriques : une arête mise à 0 donnerait b = ofs * invQ, faux
            // (contre-exemple 1). Une arête acceptée a lo >= 0 : ses vraies valeurs passent le
            // test de couverture sans le changer. Seul reste le choix de la largeur, sur les
            // VRAIES valeurs (scène V3 : 7 triangles en 64 bits sur 36 262 en AVX2).
            wide |= (lo < INT32_MIN) | (hi > INT32_MAX);
        }

        fx.wide = wide;
    }


   // ── Pendant SCALAIRE (LV3_SIMD = 0, boîtes étroites, TNR). Toujours en int64 :
   //    une addition int64 coûte une addition int32 en x64, et rien ne peut déborder.
   //    Couverture : (e0 | e1 | e2) >= 0  <=>  les trois >= 0 (le OU porte les bits de signe).
   //    Barycentriques : la formule FLOTTANTE DIRECTE de C1, et seulement sur les pixels couverts.
    template<class Sink>
    [[nodiscard]] LV3_FORCEINLINE uint32_t ForEachCoveredPixelFx(const TriangleSetup& s, const EdgeSetupFx& fx, Sink& sink)
    {
        uint32_t covered = 0;
        const int64_t a0 = fx.a[0], a1 = fx.a[1], a2 = fx.a[2];
        const int64_t b0 = fx.b[0], b1 = fx.b[1], b2 = fx.b[2];
        int64_t r0 = fx.e[0], r1 = fx.e[1], r2 = fx.e[2];     // E' au début de la ligne
        const float o0 = fx.ofs[0], o1 = fx.ofs[1], o2 = fx.ofs[2], inv = fx.invQ;
        const bool  flipped = s.flipped;

        for (int32_t y = s.bb.y0; y < s.bb.y1; ++y)
        {
            sink.BeginRow(y);
            int64_t e0 = r0, e1 = r1, e2 = r2;

            for (int32_t x = s.bb.x0; x < s.bb.x1; ++x)
            {
                if ((e0 | e1 | e2) >= 0)                      // couvert : décision ENTIÈRE, exacte
                {
                    ++covered;
                    // D-a1b : barycentriques tirées des MÊMES entiers que la décision.
                    // Avant (D-a1) : 3 EdgeFunction flottantes (21 opérations) refaites ici.
                    const float w0 = BaryFromFixed(e0, o0, inv);
                    const float w1 = BaryFromFixed(e1, o1, inv);
                    const float w2 = BaryFromFixed(e2, o2, inv);
                    // Échange défait : w1 et w2 dans l'ordre du triangle d'origine.
                    const BarycentricWeights bary = flipped
                        ? BarycentricWeights{ w0, w2, w1 }
                        : BarycentricWeights{ w0, w1, w2 };
                    sink.Pixel(x, bary);
                }
                e0 += a0; e1 += a1; e2 += a2;                 // un pixel à droite : 3 additions
            }
            r0 += b0; r1 += b1; r2 += b2;                     // une ligne plus bas : 3 additions
        }
        return covered;
    }

	// ════════════════════════════════════════════════════════════
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

        // D-a1, palier S : la preparation en virgule fixe est consommee aussi (6 entiers par arete).
        LV3_FORCEINLINE void AblationConsumeSetupFx(const TriangleSetup& s, const EdgeSetupFx& fx) noexcept
        {
            AblationConsumeSetup(s);
            uint64_t h = std::bit_cast<uint32_t>(fx.invQ);
            for (int k = 0; k < 3; ++k)
                h += (uint64_t(fx.e[k]) ^ uint64_t(fx.a[k]) ^ uint64_t(fx.b[k])) + std::bit_cast<uint32_t>(fx.ofs[k]);
            g_ablationSink = g_ablationSink + uint32_t(h ^ (h >> 32));
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