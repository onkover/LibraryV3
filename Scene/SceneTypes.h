#pragma once
#include <cstdint>

// ============================================================
//  Scene/SceneTypes.h — Enums et opérateurs du système scène
// ============================================================

namespace LV3
{

    enum class ELayerMask : uint32_t
    {
        None = 0,
        Default = 1 << 0,   // 0x01
        UI = 1 << 1,   // 0x02
        Physics = 1 << 2,   // 0x04
        Raycast = 1 << 3,   // 0x08
        FX = 1 << 4,   // 0x10
        Editor = 1 << 5,   // 0x20
        All = ~0u        // 0xFFFFFFFF — toutes les couches
    };

    /* L'opérateur | allume des bits — il ne peut jamais en éteindre.

    On veut Default ET Physics dans le masque
    ELayerMask mask = ELayerMask::Default | ELayerMask::Physics;
        Default  = 0000 0001  (bit 0)
        Physics  = 0000 0100  (bit 2)
        résultat = 0000 0101  = 5
    */
    inline ELayerMask operator|(ELayerMask a, ELayerMask b)
    {
        return static_cast<ELayerMask>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

	/* L'opérateur & tester un bit commun entre deux masques — retourne un masque avec les bits communs
    
    if (mask & ELayerMask::Physics)  // true  — bit 2 allumé
    if (mask & ELayerMask::UI)       // false — bit 1 éteint

    mask     = 0000 0101
    Physics  = 0000 0100
    &        = 0000 0100  ≠ 0 → true

    mask     = 0000 0101
    UI       = 0000 0010
    &        = 0000 0000  = 0 → false

    */ 
    inline ELayerMask operator&(ELayerMask a, ELayerMask b)
    {
        return static_cast<ELayerMask>(
            static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

	/* L'opérateur ~ inverse tous les bits du masque — utile pour créer un masque "inverse" ou "complémentaire"
    
    Éteindre Physics du masque
    mask = mask & ~ELayerMask::Physics;

    mask      = 0000 0101
    ~Physics  = 1111 1011  (tous les bits sauf le 2)
    &         = 0000 0001  — Physics retiré, Default intact
    */

    inline ELayerMask operator~(ELayerMask a)
    {
        return static_cast<ELayerMask>(~static_cast<uint32_t>(a));
    }

    // Raccourci — retourne true si au moins un bit commun
    inline bool HasLayer(ELayerMask mask, ELayerMask layer)
    {
        return (mask & layer) != ELayerMask::None;
    }

} // namespace LV3


/*
Exemple d'utilisation

1. Un rayon de raycast ignore l'UI et les FX
ELayerMask rayMask = ELayerMask::Default | ELayerMask::Physics;

2. Une lumière n'éclaire que Default et FX
ELayerMask lightMask = ELayerMask::Default | ELayerMask::FX;

3. Test dans le renderer — cet objet est-il visible par la caméra ?
if (HasLayer(camera.cullingMask, object.layer))
    Render(object);

4. Retirer une couche dynamiquement
camera.cullingMask = camera.cullingMask & ~ELayerMask::Editor; // cache l'éditeur en runtime

Question : Pourquoi uint32_t et pas uint8_t
uint8_t = 8 bits = 8 couches maximum. 
Unity en a 32. On prend uint32_t dès le départ — changer le type sous-jacent d'un bitmask en cours de projet casse la sérialisation des scènes.

*/
