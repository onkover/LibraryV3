#pragma once
// ============================================================
//  Rendering/ClearKernels.h — effacement des buffers plein ecran
//
//  Chantier Clear, phase G (lots M14 a M16, i7-1265U, 1 coeur) :
//   - std::fill / memset (rep stos) et stores ordinaires plafonnent a
//     ~9 Go/s : chaque ligne est d'abord LUE (RFO) et occupe un tampon de
//     remplissage pendant tout l'aller-retour DRAM (loi de Little :
//     ~16 lignes en vol x 64 o / ~110 ns).
//   - stores NON TEMPORELS : lignes completes envoyees sans attendre de
//     reponse, ~25 Go/s. A 1536x900 : Clear 1,09 -> 0,45 ms, budget
//     moteur x0,60, et Render -13 % (plus de lignes modifiees a evincer).
//   - a 768x450 (buffers dans le L3) : egalite (budget moteur x0,99-1,00).
//     Aucun seuil n'est donc justifie : UN seul chemin.
//
//  Contrat de PERFORMANCE (pas de correction) : le buffer n'est pas relu
//  en entier juste apres l'effacement. Vrai pour couleur et profondeur :
//  le rasterizer n'en touche qu'une petite partie.
//  Appeler Fence() UNE fois apres le dernier StreamFill d'un buffer.
// ============================================================
#include <cstdint>
#include <cstddef>
#include <bit>
#include <type_traits>
#include <emmintrin.h>      // SSE2 : _mm_set1_epi32, _mm_stream_si128, _mm_sfence
#include "../Core/config.h"

namespace LV3::ClearKernel
{
    // Remplit [dst, dst + count) avec value, en stores non temporels.
    // T : mot de 32 bits (uint32_t pour la couleur, float pour la profondeur).
    template <class T>
    inline void StreamFill(T* dst, size_t count, T value) noexcept
    {
        static_assert(sizeof(T) == 4 && std::is_trivially_copyable_v<T>,
            "ClearKernel::StreamFill : ecrit pour des mots de 32 bits");

        T* p = dst;
        T* const end = dst + count;

        // 1. Tete scalaire jusqu'a l'alignement 16 octets (exige par _mm_stream_si128).
        //    Vide en pratique : lignes SDL et std::vector alignes (verifie au debogueur, 2b).
        while (p < end && (reinterpret_cast<uintptr_t>(p) & 15u) != 0) *p++ = value;

        // 2. Corps : 4 x 16 octets = une ligne de cache complete par tour.
        //    Une ligne ENTIERE ecrite d'un coup : le tampon de combinaison
        //    part vers la memoire sans lecture prealable.
        const __m128i x = _mm_set1_epi32(std::bit_cast<int32_t>(value));
        __m128i* q = reinterpret_cast<__m128i*>(p);
        const size_t lines = static_cast<size_t>(end - p) / 16u;    // 16 mots = 64 octets
        for (size_t i = 0; i < lines; ++i, q += 4)
        {
            _mm_stream_si128(q + 0, x); _mm_stream_si128(q + 1, x);
            _mm_stream_si128(q + 2, x); _mm_stream_si128(q + 3, x);
        }

        // 3. Queue scalaire : vide pour toute largeur multiple de 16 pixels.
        p = reinterpret_cast<T*>(q);
        while (p < end) *p++ = value;

        // Filet RelWithAsserts : premier et dernier mot, compares BIT A BIT.
        LV3_ASSERT(count == 0 ||
            (std::bit_cast<uint32_t>(dst[0]) == std::bit_cast<uint32_t>(value) &&
                std::bit_cast<uint32_t>(dst[count - 1]) == std::bit_cast<uint32_t>(value)));
    }

    // Les stores non temporels sont FAIBLEMENT ordonnes : sfence les rend
    // visibles avant qu'un autre agent lise le buffer (pilote D3D au
    // SDL_UnlockTexture pour la couleur). Une fois par buffer, pas par ligne.
    inline void Fence() noexcept { _mm_sfence(); }
}