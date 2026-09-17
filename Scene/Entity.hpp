#pragma once

//#include <cstdint>
/*
Un Entity n'est plus un numéro : c'est un ticket daté. 
* Il identifie un slot (l'index) à une époque donnée (la génération). 
* Quand une entité meurt, son slot est recyclable, mais tous les tickets émis pour l'ancienne époque deviennent instantanément détectables comme périmés — en O(1), sans table de correspondance, sans allocation.

Exemple : 0x00010002
bits 31 … 16                bits 15 … 0
génération de 16 bits       index de 16 bits
	0x0001                       0x0002

=> index 2, génération 1 → Entity = 0x00010002

Découpage retenu : 16 bits d'index / 16 bits de génération
=> 65 536 entités simultanées, 65 536 générations par slot avant bouclage.
Choisi au-dessus de LV3_MAX_ENTITIES (4096, EngineSettings.h) avec une marge
volontaire, sans faire de 4096 une contrainte dure du format du handle.
Un handle antique aurait 1 chance sur 65 536 de « revalider » par accident
après recyclage du slot — contre 1 sur 256 avec le découpage précédent.

Augmenter le nombre de génération permet de réduire ce risque, mais réduit le nombre d'entités simultanées.

/!\ Une Entity non initialisée n'existe pas. Toute déclaration s'écrit 
	Entity e = NULL_ENTITY;
jamais 	* Entity e; ni Entity e{};


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

	inline constexpr std::uint32_t ENTITY_INDEX_BITS = 16u;
	inline constexpr std::uint32_t ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS) - 1u;	// 0x0000FFFF
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
	[[nodiscard]] constexpr std::uint16_t EntityGeneration(Entity e) noexcept
	{
		return static_cast<std::uint16_t>(e >> ENTITY_INDEX_BITS);
	}

	/// <summary>
	/// Crée une entité à partir d'un index et d'une génération.
	/// </summary>
	/// <param name="index"></param>
	/// <param name="generation"></param>
	/// <returns></returns>
	[[nodiscard]] constexpr Entity MakeEntity(std::uint32_t index, std::uint16_t generation) noexcept
	{
		return (static_cast<Entity>(generation) << ENTITY_INDEX_BITS) | (index & ENTITY_INDEX_MASK);
		// Le masque index & ENTITY_INDEX_MASK dans MakeEntity empêche un index débordant de contaminer le champ génération.
	}

	// Garde-fous à la compilation : si quelqu'un touche au layout, ça casse ici, pas en production
	static_assert(EntityIndex(MakeEntity(2u, 1u)) == 2u);		// Vérifie que l'index est bien dans les 16 bits
	static_assert(EntityGeneration(MakeEntity(2u, 1u)) == 1u);	// Vérifie que la génération est bien dans les 16 bits
	static_assert(MakeEntity(2u, 1u) == 0x00010002u);			// Vérifie que la combinaison index/génération est correcte

}