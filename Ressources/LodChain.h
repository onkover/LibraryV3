#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>

#include "ResourceHandle.h"               // MeshHandle
#include "../Maths/geometry/AABB3d.h"

namespace LV3
{
    // ════════════════════════════════════════════════════════════
    //  CHAINE DE LOD — annexe A13 bis, disposition A.
    //  Propriete de l'ASSET : construite au chargement, immuable ensuite.
    //  Un mesh sans LOD est une chaine de longueur 1 : aucun cas particulier.
    //
    //  Ordre des membres = ordre des lectures dans RenderView :
    //  * culling (bounds), 
    //  * puis selection (invEps), 
    //  * puis dessin (levels).
    // 
    //  Le tout tient dans UNE ligne de cache : pas de separation chaud/froid.
    // ════════════════════════════════════════════════════════════
    #if defined(_MSC_VER)   // evite le warning si alignas(64) est deja actif sur le compilateur et si la taille < 60
							// La compiltateur ajoutera  tout de même un padding pour tenir sur 64 octets, mais on ne veut pas de warning inutile. On ne peut pas utiliser static_assert(sizeof(LodChain) == 64) car la taille peut etre > 64 si le compilateur ajoute du padding.
        #   pragma warning(push)                    // Le push sauvegarde l'état des avertissements
        #   pragma warning(disable : 4324)          // le disable coupe C4324
    #endif
    struct alignas(64) LodChain
    {
        static constexpr uint32_t kMaxLevels = 4;

        // ── CULLING : union des AABB LOCALES de tous les niveaux.
        //    Un niveau grossier peut deborder de la boite de L0.
        AABB3d     bounds;

        // ── SELECTION : invEps[k] = 1 / epsilon_k, en unites locales.
        //    invEps[0] = +inf (L0 exact).  Strictement DECROISSANT sur 1..levelCount-1.
        //    Cases inutilisees = -inf : aucune valeur de q' ne les admet.
        float      invEps[kMaxLevels] = { std::numeric_limits<float>::infinity(),
                                          -std::numeric_limits<float>::infinity(),
                                          -std::numeric_limits<float>::infinity(),
                                          -std::numeric_limits<float>::infinity() };

        // ── DESSIN : un mesh complet par niveau (disposition A).
        MeshHandle levels[kMaxLevels] = {};
        uint32_t   levelCount = 0;
    };
    #if defined(_MSC_VER) // réative le warning 4324 pour le reste du code
        #pragma warning(pop) // le pop restaure l'état sauvegardé aupravant le push
    #endif

    // Stocke dans un conteneur et copie librement : aucun destructeur, aucun pointeur possede.
    static_assert(std::is_trivially_copyable_v<LodChain>, "LodChain doit rester une donnee pure");
    
    // Si cette assertion echoue, AABB3d n'est pas en float : on en discute AVANT d'aller plus loin.
    static_assert(sizeof(LodChain) == 64, "LodChain doit tenir dans une ligne de cache");

    // ────────────────────────────────────────────────────────────
    //  Niveau le plus grossier admis : epsilon_k * q <= tau  <=>  q' <= invEps[k],
    //  avec q' = q / tau, calcule par l'appelant.
    //  invEps decroissant => les niveaux admis forment un PREFIXE 1..m :
    //  leur NOMBRE est l'indice du niveau choisi.
    //  Aucune branche, aucune division, 3 tours fixes (deroules par le compilateur).
    //  q' = +inf (touche le near) ou NaN => aucune comparaison vraie => L0.
    // ────────────────────────────────────────────────────────────
    [[nodiscard]] LV3_FORCEINLINE uint32_t SelectLodLevel(const LodChain& chain, float qOverTau) noexcept
    {
        uint32_t k = 0;

        for (uint32_t i = 1; i < LodChain::kMaxLevels; ++i)
            k += uint32_t(qOverTau <= chain.invEps[i]);

        return k;
    }
}