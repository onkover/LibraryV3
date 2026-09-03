#include "pch.h"
#include "Hierarchy.hpp"
//#include <iostream>
//#include "Registry.hpp"
#include "../Core/Logger.h"
#include "SerializerHelpers.hpp"


namespace LV3
{
	/// <summary>
	/// Fonction pour lier un enfant à un parent (et vice versa) dans le Registry
	/// Pour une entité donnée dans HierarchyComponent, nous allons créer une nouvelle nouvelle hiérarchie pour référencer :
	/// * son parent
	/// * ses enfants
	/// 
	/// On recherche ensuite son parent pour référencer son enfant
	/// </summary>
	/// <param name="registry">Registre des composants</param>
	/// <param name="child">entité enfant</param>
	/// <param name="parent">entité parent</param>
	//void linkChildToParentOLD(Registry& registry, Entity child, Entity parent)
	//{
	//	std::cout << "LIAISON : Entité " << registry.getComponent<NameComponent>(child).m_id << " est maintenant enfant de " << registry.getComponent<NameComponent>(parent).m_id << std::endl;

	//	// 1. Attache un composant Hierarchie à l'enfant pour référencer son futur parent
	//	registry.addComponent(child, HierarchyComponent{ parent, {},false }); // pas besoin de std::move(composant) ici car le composant est construit directement en argument, donc c'est une prvalue

	//	// 2. Ajoute l'enfant à la liste du parent
	//	if (!registry.hasComponent<HierarchyComponent>(parent))
	//	{
	//		// Crée un composant parent s'il n'existe pas
	//		registry.addComponent(parent, HierarchyComponent{ NULL_ENTITY, {}, true }); // parent vide, marqué comme racine
	//	}
	//	// Retrouve le parent (nouvellement créé ou pas) pour référencer son enfant
	//	registry.getComponent<HierarchyComponent>(parent).m_children.push_back(child);
	//}

	void linkChildToParent(Registry& registry, Entity child, Entity parent)
	{
		LV3_ASSERT(child != parent && "Une entité ne peut pas être son propre parent");
		LV3_ASSERT(child != NULL_ENTITY && parent != NULL_ENTITY);

		// ── L'ENFANT : mise à jour, JAMAIS écrasement ─────────────────────
		if (HierarchyComponent* hc = registry.TryGet<HierarchyComponent>(child))
		{
			// L'enfant possède déjà un HierarchyComponent (parce qu'un de SES enfants,
			// déclaré plus tôt dans le JSON, le lui a créé), on ne touche qu'aux
			// champs qui changent.
			LV3_ASSERT(hc->m_parent == NULL_ENTITY && "Ré-parentage interdit ici : linkChildToParent construit, SetParent (P1) mutera");
			hc->m_parent = parent;
			//hc->m_isRoot = false;      // il vient de recevoir un parent
			// (champ condamné en P1 — R27)
		}
		else
		{
			// Attache un composant Hierarchie à l'enfant. Référence son parent
			registry.addComponent(child, HierarchyComponent{ parent, {} });
		}

		// ── LE PARENT : créé vierge si absent, jamais écrasé ──────────────
		if (!registry.hasComponent<HierarchyComponent>(parent))
			registry.addComponent(parent, HierarchyComponent{ NULL_ENTITY, {} });
		
		// Référence l'enfant à son parent
		registry.getComponent<HierarchyComponent>(parent).m_children.push_back(child);
	}

	// Chaque entité hiérarchisée doit atteindre une racine en moins de N pas.
// Un cycle (A parent de B, B parent de A) n'a PAS de racine : sans ce
// contrôle, le sous-graphe cyclique n'est jamais propagé — silencieusement.
	bool ValidateHierarchy(Registry& registry)
	{
		const std::uint32_t maxSteps = registry.GetAliveCount();

		for (auto&& [entity, hc] : registry.ViewGroup<HierarchyComponent>())
		{
			Entity cursor = entity;
			std::uint32_t steps = 0;

			while (true)
			{
				const HierarchyComponent* h = registry.TryGet<HierarchyComponent>(cursor);
				if (!h || h->m_parent == NULL_ENTITY) break;      // racine atteinte
				cursor = h->m_parent;

				if (++steps > maxSteps)
				{
					Logger::error("ValidateHierarchy — cycle détecté depuis '" + EntityLabel(registry, entity) + "'");
					return false;
				}
			}
		}
		return true;
	}


}