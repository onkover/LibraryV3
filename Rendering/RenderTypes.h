#pragma once
#include <cstdint>

// ============================================================
//  Rendering/RenderTypes.h — Enums du pipeline de rendu CPU
//  Inclure manuellement dans les .cpp du système de rendu
// ============================================================

/*
Vue d'ensemble du pipeline:

Mesh soumis au Renderer
        ↓
ECullMode      → élimine les faces dos-caméra
        ↓
EDepthTest     → accepte ou rejette le fragment via Z-buffer
        ↓
EShadingModel  → calcule la couleur (Flat / Gouraud / Phong)
        ↓
EBlendMode     → fusionne avec le framebuffer
        ↓
ERenderMode    → détermine ce qui est finalement affiché
*/

namespace LV3
{

    // Mode d'affichage global — commande ce que le rasterizer produit
    enum class ERenderMode : uint8_t
    {
        Solid,            // Rendu complet — couleur + éclairage
        Wireframe,        // Arêtes uniquement — débogage géométrie
        Depth,            // Visualise le Z-buffer — débogage profondeur
        Normals,          // Couleur = normale — débogage éclairage
        UV,                // Couleur = coordonnées UV — débogage textures
		BarycentricColors,    // Visualise les coordonnées barycentriques — débogage rasterizer
        LinearDepth
    };

    // Face culling — quelles faces le rasterizer élimine avant de rasteriser
    enum class ECullMode : uint8_t
    {
        None,    // Aucun culling — double face (coûteux)
        Front,   // Élimine les faces avant (rare — effets spéciaux)
        Back     // Élimine les faces arrière (défaut — optimisation)
    };

    // Test de profondeur — comment le Z-buffer accepte ou rejette un fragment
    //enum class EDepthTest : uint8_t
    //{
    //    Always,      // Toujours passe — pas de test (skybox, UI)
    //    Less,        // Passe si plus proche — standard 3D
    //    LessEqual,   // Passe si plus proche ou égal — terrain, décals
    //    Never        // Ne passe jamais — masque d'occlusion
    //};

    enum class EDepthTest : uint8_t
    {
        Always,        // skybox, UI
        Greater,       // standard 3D — plus proche = z plus GRAND (reverse-Z)
        GreaterEqual,  // terrain, décals coplanaires
        Never          // masque d'occlusion
    };

    // Mode de fusion — comment un pixel se mélange avec le framebuffer
    enum class EBlendMode : uint8_t
    {
        Opaque,      // Aucune transparence — remplace le pixel
        AlphaTest,   // Découpe binaire — alpha > seuil ou rien (feuillages)
        AlphaBlend,  // Transparence douce — src*a + dst*(1-a)
        Additive     // Accumulation — effets lumineux, feu, particules
    };

    // Modèle d'ombrage — quel algorithme d'éclairage est appliqué
    enum class EShadingModel : uint8_t
    {
        Unlit,      // Aucun calcul lumière — couleur brute (debug, UI)
        Flat,       // Une normale par face — facetté
        Gouraud,    // Interpolation de couleur par sommet
        Phong       // Interpolation de normale par fragment
    };


} // namespace LV3