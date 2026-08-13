#include "pch.h"
#include "Fragment.h"
#include "../Maths/MathUtils.h"

namespace LV3
{

    //void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights& /*bary*/, void* userData)
    //{
    //    auto* ctx = static_cast<UnlitContext*>(userData);
    //    ctx->fb->SetPixel(x, y, ctx->color);
    //    //ctx->fb->SetPixel(x, y, Color{ 255, 255, 255, 255 });
    //}

    void ShadeFragment_Barycentric(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
    {
        // w0 est le poids de v0, pas de l'arête v0v1

        auto* ctx = static_cast<FragmentContext*>(userData);   // ← alignement
        const float z = bary.w0 * ctx->z0 + bary.w1 * ctx->z1 + bary.w2 * ctx->z2;
        if (!ctx->db->TestAndSet(x, y, z)) return;

        ctx->fb->SetPixelUnchecked(x, y, Color{
            static_cast<uint8_t>(std::clamp(bary.w2, 0.f, 1.f) * 255.0f),   // b
            static_cast<uint8_t>(std::clamp(bary.w1, 0.f, 1.f) * 255.0f),   // g
            static_cast<uint8_t>(std::clamp(bary.w0, 0.f, 1.f) * 255.0f),   // r
            255 });
    }

    void ShadeFragment_Solid(int32_t x, int32_t y,
        const BarycentricWeights& b, void* userData)
    {
        auto* ctx = AsFragmentContext(userData);

        // ── 1. PROFONDEUR : affine, aucune correction (cf. §1.1) ──
        const float z = b.w0 * ctx->z0 + b.w1 * ctx->z1 + b.w2 * ctx->z2;

        // ── 2. EARLY-Z : rejeter AVANT tout autre calcul ──
        if (!ctx->db->TestAndSet(x, y, z)) return;      // reverse-Z, GREATER

        // ── 3. Le dénominateur n'est calculé QUE si le fragment survit ──
        // (couleur plate ici : rien à corriger. Structure en place pour la suite.)
        ctx->fb->SetPixel(x, y, ctx->color);
    }

    /*
    Fragment de DEBUG : 
   
	Permet de diagnostiquer les erreurs de perspective dans le shader de profondeur linéaire.
    * Bandes droites, continues, resserrées au loin         : 	✅ Chantier B correct
    * Bandes qui plient en V sur la diagonale des quads 	:   ❌ interpolation affine — le dénominateur n'est pas appliqué
    * Bandes régulièrement espacées jusqu'à l'horizon	    :   ❌ tu interpoles dist affinement au lieu de 1/dist
    * Bandes qui sautent d'un triangle à l'autre	        :   ❌ invW mal rempli sur certains sommets
    
    */
    void ShadeFragment_LinearDepth(int32_t x, int32_t y,
        const BarycentricWeights& b, void* userData)
    {
        auto* ctx = AsFragmentContext(userData);

        const float z = b.w0 * ctx->z0 + b.w1 * ctx->z1 + b.w2 * ctx->z2;
        if (!ctx->db->TestAndSet(x, y, z)) return;

        // ── LA division perspective par pixel ──
        const float den = b.w0 * ctx->invW0 + b.w1 * ctx->invW1 + b.w2 * ctx->invW2;
        const float dist = 1.0f / den;                 // distance VRAIE à l'œil

        // bandes de 1 unité — le motif plie visiblement si la correction manque
        const uint8_t g = uint8_t((std::fmod(dist, 1.0f)) * 255.0f);
        ctx->fb->SetPixel(x, y, MakeColor(g, g, g));
    }

    /*
	Fragment de DEBUG : Affiche la profondeur linéaire dans une plage de debug, avec un mapping gamma pour l'affichage.
    */
    void ShadeFragment_Depth(int32_t x, int32_t y,
        const BarycentricWeights& b, void* userData)
    {
        auto* ctx = AsFragmentContext(userData);

        // 1. profondeur NDC — affine, pour le test de profondeur uniquement
        const float z = b.w0 * ctx->z0 + b.w1 * ctx->z1 + b.w2 * ctx->z2;
        if (!ctx->db->TestAndSet(x, y, z)) return;

        // 2. distance à l'œil — DIRECTE. w = d, et 1/w est linéaire en écran.
        const float den = b.w0 * ctx->invW0 + b.w1 * ctx->invW1 + b.w2 * ctx->invW2;
        const float dist = 1.0f / den;

        // 3. normalisation d'AFFICHAGE — plage de debug, pas la lentille
        const float t = Saturate(dist / ctx->depthDisplayRange);

        // 4. gamma — inchangé
        const uint8_t g = uint8_t(std::pow(1.0f - t, 1.0f / 2.2f) * 255.0f);
        ctx->fb->SetPixel(x, y, MakeColor(g, g, g));
    }

    /*
	Fragment de DEBUG : permet de compter combien de fragments sont générés par pixel, pour diagnostiquer la TNR top-left.
    */
    void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights&, void* userData)
    {
        auto* ctx = static_cast<CountContext*>(userData);
        (*ctx->counts)[y * ctx->width + x]++;   // ici width suffit : c'est TON buffer, pas SDL
    }

} // namespace LV3