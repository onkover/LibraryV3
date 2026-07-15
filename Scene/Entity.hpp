#pragma once

#include <cstdint>
namespace LV3
{
	using Entity = uint32_t;

}


/*
Un Entity n'est plus un numéro : c'est un ticket daté. 
* Il identifie un slot (l'index) à une époque donnée (la génération). 
* Quand une entité meurt, son slot est recyclable, mais tous les tickets émis pour l'ancienne époque deviennent instantanément détectables comme périmés — en O(1), sans table de correspondance, sans allocation.

Exemple : 0x01000002
bits 31 … 24                bits 23 … 0
génération de 8 bits        index de  24 bits                     
    0x01                        0x000002

=> index 2, génération 1 → Entity = 0x01000002





Solution EnTT : le bit-packing dans un seul uint32_t. 
EnTT découpe en 20/12 ; ici, nous allons implémenter un découpage 
* 24 bits d'index 
* 8 bits de génération 
=> Soit 16,7 millions d'entités simultanées, 256 générations par slot.
A savoir que la génération sur 8 bits boucle après 256 réutilisations du même slot (255 → 0 par débordement volontaire de l'uint8_t). Un handle antique aurait alors 1 chance sur 256 de « revalider » par accident. 
Augmenter le nombre de génération permet de réduire ce risque, mais réduit le nombre d'entités simultanées.

*/

namespace LV3
{
	// ============================================================
	//  Entity — ticket daté : [ génération : 8 bits ][ index : 24 bits ]
	//  - index      : slot dans le Registry (16 777 215 entités max)
	//  - génération : époque du slot ; incrémentée à chaque destruction
	//  RÈGLE : une Entity ne s'utilise JAMAIS comme indice brut.
	//          Tout accès tableau passe par EntityIndex(e).
	// ============================================================

	using Entity = std::uint32_t;

	inline constexpr std::uint32_t ENTITY_INDEX_BITS = 24u;
	inline constexpr std::uint32_t ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS) - 1u;	// 0x00FFFFFF
	inline constexpr Entity        NULL_ENTITY = 0xFFFFFFFFu;						// index ET génération à leur max : jamais émis

	/// <summary>
	/// Obtient l'index d'une entité.
	/// </summary>
	/// <param name="e"></param>
	/// <returns></returns>
	[[nodiscard]] constexpr std::uint32_t EntityIndex(Entity e) noexcept
	{
		return e & ENTITY_INDEX_MASK;
	}

	/// <summary>
	/// Obtient la génération d'une entité.
	/// </summary>
	/// <param name="e"></param>
	/// <returns></returns>
	[[nodiscard]] constexpr std::uint8_t EntityGeneration(Entity e) noexcept
	{
		return static_cast<std::uint8_t>(e >> ENTITY_INDEX_BITS);
	}

	/// <summary>
	/// Crée une entité à partir d'un index et d'une génération.
	/// </summary>
	/// <param name="index"></param>
	/// <param name="generation"></param>
	/// <returns></returns>
	[[nodiscard]] constexpr Entity MakeEntity(std::uint32_t index, std::uint8_t generation) noexcept
	{
		return (static_cast<Entity>(generation) << ENTITY_INDEX_BITS) | (index & ENTITY_INDEX_MASK);
		// Le masque index & ENTITY_INDEX_MASK dans MakeEntity empêche un index débordant de contaminer le champ génération.
	}

	// Garde-fous à la compilation : si quelqu'un touche au layout, ça casse ici, pas en production
	static_assert(EntityIndex(MakeEntity(2u, 1u)) == 2u);		// Vérifie que l'index est bien dans les 24 bits
	static_assert(EntityGeneration(MakeEntity(2u, 1u)) == 1u);	// Vérifie que la génération est bien dans les 8 bits
	static_assert(MakeEntity(2u, 1u) == 0x01000002u);			// Vérifie que la combinaison index/génération est correcte

}