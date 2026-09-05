#include "pch.h"
#include "Hierarchy.hpp"
//#include <iostream>
//#include "Registry.hpp"
#include "../Core/Logger.h"
#include "SerializerHelpers.hpp"


namespace LV3
{

	//********************************************************************

	//void linkChildToParent(Registry& registry, Entity child, Entity parent)
	//{
	//	LV3_ASSERT(child != parent && "Une entité ne peut pas être son propre parent");
	//	LV3_ASSERT(child != NULL_ENTITY && parent != NULL_ENTITY);

	//	// ── L'ENFANT : mise à jour, JAMAIS écrasement ─────────────────────
	//	if (HierarchyComponent* hc = registry.TryGet<HierarchyComponent>(child))
	//	{
	//		// L'enfant possède déjà un HierarchyComponent (parce qu'un de SES enfants,
	//		// déclaré plus tôt dans le JSON, le lui a créé), on ne touche qu'aux
	//		// champs qui changent.
	//		LV3_ASSERT(hc->m_parent == NULL_ENTITY && "Ré-parentage interdit ici : linkChildToParent construit, SetParent (P1) mutera");
	//		hc->m_parent = parent;
	//		//hc->m_isRoot = false;      // il vient de recevoir un parent
	//		// (champ condamné en P1 — R27)
	//	}
	//	else
	//	{
	//		// Attache un composant Hierarchie à l'enfant. Référence son parent
	//		registry.addComponent(child, HierarchyComponent{ parent, {} });
	//	}

	//	// ── LE PARENT : créé vierge si absent, jamais écrasé ──────────────
	//	if (!registry.hasComponent<HierarchyComponent>(parent))
	//		registry.addComponent(parent, HierarchyComponent{ NULL_ENTITY, {} });
	//	
	//	// Référence l'enfant à son parent
	//	registry.getComponent<HierarchyComponent>(parent).m_children.push_back(child);
	//}

	 // ─────────────────────────────────────────────────────────────
	//  ATTACHE — le geste unique, interne au fichier ('static' :
	//  invisible hors de Hierarchy.cpp, personne d'autre ne peut
	//  attacher sans passer par les fonctions publiques ci-dessous).
	//  Préconditions (garanties par les appelants, pas revérifiées) :
	//  child n'a pas de parent actuellement ; pas de cycle.
	// ─────────────────────────────────────────────────────────────
	static void AttachToParent(Registry& registry, Entity child, Entity parent)
	{
		if (HierarchyComponent* hc = registry.TryGet<HierarchyComponent>(child))
			hc->m_parent = parent;
		else
			registry.addComponent(child, HierarchyComponent{ parent, {} });

		if (!registry.hasComponent<HierarchyComponent>(parent))
			registry.addComponent(parent, HierarchyComponent{ NULL_ENTITY, {} });

		registry.getComponent<HierarchyComponent>(parent).m_children.push_back(child);
	}

	// ─────────────────────────────────────────────────────────────
	//  CONSTRUCTION (Serializer, SpawnCameraGizmos) : l'enfant ne
	//  doit pas déjà avoir de parent — ré-attacher pendant la
	//  construction est un bug de données, l'assert le dénonce.
	// ─────────────────────────────────────────────────────────────
	void linkChildToParent(Registry& registry, Entity child, Entity parent)
	{
		LV3_ASSERT(child != parent && child != NULL_ENTITY && parent != NULL_ENTITY);

		const HierarchyComponent* hc = registry.TryGet<HierarchyComponent>(child);
		LV3_ASSERT((!hc || hc->m_parent == NULL_ENTITY)
			&& "Construction : cet enfant a déjà un parent — utiliser SetParent");

		AttachToParent(registry, child, parent);
	}

	//********************************************************************
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
	
	//********************************************************************
	// Détache 'child' de son parent : il devient racine, ses propres enfants le suivent.
	// Sans parent -> no-op silencieux (détacher une racine est légal et vide de sens).
	void Detach(Registry& registry, Entity child)
	{
		HierarchyComponent* hc = registry.TryGet<HierarchyComponent>(child);
		if (!hc || hc->m_parent == NULL_ENTITY) return;

		// Symétrie : l'ancien parent oublie l'enfant. swap-erase, l'ordre des
		// frères n'est pas une donnée (personne ne doit s'appuyer dessus).
		if (HierarchyComponent* ph = registry.TryGet<HierarchyComponent>(hc->m_parent))
		{
			auto& kids = ph->m_children;
			const auto it = std::find(kids.begin(), kids.end(), child);
			LV3_ASSERT(it != kids.end() && "Invariant parent<->children brisé : écriture directe quelque part");
			if (it != kids.end()) { *it = kids.back(); kids.pop_back(); }
		}

		hc->m_parent = NULL_ENTITY;    // racine, par définition (IsRoot le lira)
	}

	//********************************************************************
	// Un descendant de 'child' ne peut pas devenir son parent : on remonte depuis
	// 'node' vers les racines ; si on croise 'ancestor', c'est un descendant.
	// O(profondeur), au moment de la mutation — jamais dans la boucle chaude.
	[[nodiscard]] bool IsDescendantOf(const Registry& registry, Entity node, Entity ancestor)
	{
		Entity cursor = node;
		while (const HierarchyComponent* h = registry.TryGet<HierarchyComponent>(cursor))
		{
			if (h->m_parent == NULL_ENTITY) return false;
			if (h->m_parent == ancestor)    return true;
			cursor = h->m_parent;
		}
		return false;
	}

	// Déplace 'child' sous 'newParent' (NULL_ENTITY = équivaut à Detach).
	// Retourne false et ne fait RIEN si la demande est illégale — une mutation
	// refusée doit laisser le graphe exactement comme elle l'a trouvé.
	bool SetParent(Registry& registry, Entity child, Entity newParent, bool keepWorld)
	{
		if (keepWorld)
		{
			Logger::error("SetParent — keepWorld : Annexe A8, pas encore implémenté"); return false;
		}

		if (child == newParent)
		{
			Logger::error("SetParent — une entité ne peut être son propre parent"); return false;
		}

		if (newParent != NULL_ENTITY && IsDescendantOf(registry, newParent, child))
		{
			Logger::error("SetParent — cycle refusé : le nouveau parent est un descendant de l'enfant"); return false;
		}

		Detach(registry, child);
		if (newParent != NULL_ENTITY)
			AttachToParent(registry, child, newParent);
		
		return true;

		//Detach(registry, child);
		//if (newParent == NULL_ENTITY) return true;

		//// Attache — même chair que linkChildToParent, qui devient un alias de
		//// construction : le Serializer continue de l'appeler, il aboutit ICI.
		//if (HierarchyComponent* hc = registry.TryGet<HierarchyComponent>(child))
		//	hc->m_parent = newParent;
		//else
		//	registry.addComponent(child, HierarchyComponent{ newParent, {} });

		//if (!registry.hasComponent<HierarchyComponent>(newParent))
		//	registry.addComponent(newParent, HierarchyComponent{ NULL_ENTITY, {} });
		//registry.getComponent<HierarchyComponent>(newParent).m_children.push_back(child);
		//return true;
	}

	//********************************************************************
	// Détruit 'root' ET tout son sous-arbre. Collecte d'abord, détruit ensuite :
	// DestroyEntity retire des composants via swap-and-pop — détruire en itérant
	// les listes qu'on parcourt est la recette du sous-arbre à moitié mort.
	void CollectSubtree(Registry& registry, Entity e, std::vector<Entity>& out)
	{
		out.push_back(e);
		if (const HierarchyComponent* h = registry.TryGet<HierarchyComponent>(e))
			for (Entity child : h->m_children)
				CollectSubtree(registry, child, out);
	}

	void DestroyHierarchy(Registry& registry, Entity root)
	{
		std::vector<Entity> subtree;
		CollectSubtree(registry, root, subtree);       // pré-ordre, root inclus

		Detach(registry, root);                        // l'ancien parent oublie le mort

		for (Entity e : subtree)
			registry.DestroyEntity(e);                 // les storages nettoient chacun leur part
	}


}