#include "pch.h"
#include "Hierarchy.hpp"
//#include <iostream>
//#include "Registry.hpp"
#include "../Core/Logger.h"
#include "SerializerHelpers.hpp"


namespace LV3
{

	/* 
	Exemple d'utilisation de la hiérarchie :

		LV3::SetParent(registry, e1, e2,false);
		LV3::Detach(registry, e2);
		LV3::DestroyHierarchy(registry, e2);

	*/



	// ─────────────────────────────────────────────────────────────
	//  ATTACHE — le geste unique, interne au fichier ('static' :
	//  invisible hors de Hierarchy.cpp, personne d'autre ne peut
	//  attacher sans passer par les fonctions publiques ci-dessous).
	//  Préconditions (garanties par les appelants, pas revérifiées) :
	//  child n'a pas de parent actuellement ; pas de cycle.
	// ─────────────────────────────────────────────────────────────
	/// <summary>
	/// Attache une entité enfant à une entité parente dans le Registry en ajoutant ou mettant à jour les HierarchyComponent appropriés.
	/// </summary>
	/// <param name="registry">Référence au Registry contenant les entités et leurs composants ; sera modifié pour ajouter ou mettre à jour des HierarchyComponent.</param>
	/// <param name="child">Entité à attacher comme enfant.</param>
	/// <param name="parent">Entité qui deviendra le parent.</param>
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
	/// <summary>
	/// Attache l'entité enfant au parent dans le registre après vérification des préconditions (non-null, distincts, enfant sans parent).
	/// </summary>
	/// <param name="registry">Référence au Registry contenant les composants et les entités.</param>
	/// <param name="child">Entité enfant à attacher. Doit être différente de parent, ne pas être NULL_ENTITY et ne pas avoir déjà de parent.</param>
	/// <param name="parent">Entité parent à laquelle attacher l'enfant. Doit être non NULL_ENTITY.</param>
	void linkChildToParent(Registry& registry, Entity child, Entity parent)
	{
		LV3_ASSERT(child != parent && child != NULL_ENTITY && parent != NULL_ENTITY);

		#ifdef _DEBUG
			const HierarchyComponent* hc = registry.TryGet<HierarchyComponent>(child);
			LV3_ASSERT((!hc || hc->m_parent == NULL_ENTITY)
				&& "Construction : cet enfant a déjà un parent — utiliser SetParent");
		#endif
		AttachToParent(registry, child, parent);
	}

	//********************************************************************
	// Chaque entité hiérarchisée doit atteindre une racine en moins de N pas.
	// Un cycle (A parent de B, B parent de A) n'a PAS de racine : sans ce
	// contrôle, le sous-graphe cyclique n'est jamais propagé — silencieusement.
	/// <summary>
	/// Valide la hiérarchie des entités.
	/// </summary>
	/// <param name="registry">Référence au Registry contenant les entités et leurs composants ; sera modifié pour ajouter ou mettre à jour des HierarchyComponent.</param>
	/// <returns>true si la hiérarchie est valide, false sinon.</returns>
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
	/// <summary>
	/// Détache une entité de son parent.
	/// </summary>
	/// <param name="registry">Référence au Registry contenant les entités et leurs composants ; sera modifié pour ajouter ou mettre à jour des HierarchyComponent.</param>
	/// <param name="child">Entité à détacher.</param>
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

	// ─────────────────────────────────────────────────────────────
	//  MUTATION (gameplay, éditeur) : ré-parentage légal, gardé
	//  contre les cycles, refus sans effet de bord.
	// ─────────────────────────────────────────────────────────────
	// Déplace 'child' sous 'newParent' (NULL_ENTITY = équivaut à Detach).
	// Retourne false et ne fait RIEN si la demande est illégale — une mutation
	// refusée doit laisser le graphe exactement comme elle l'a trouvé.
	/// <summary>
	/// Définit le parent d'une entité dans le registre en effectuant les vérifications d'intégrité et en réalisant le détachement/attachement.
	/// </summary>
	/// <param name="registry">Référence vers le registre contenant les entités et leurs relations (modifié par l'opération).</param>
	/// <param name="child">Identifiant de l'entité enfant dont on souhaite changer le parent.</param>
	/// <param name="newParent">Identifiant du nouvel parent, ou NULL_ENTITY pour détacher l'enfant.</param>
	/// <param name="keepWorld">Indique si l'enfant doit conserver sa transformation mondiale ; non implémenté (si true, l'appel échoue et une erreur est journalisée).</param>
	/// <returns>true si l'opération a réussi, false en cas d'échec (keepWorld non implémenté, auto-parentage, cycle détecté, etc.).</returns>
	bool SetParent(Registry& registry, Entity child, Entity newParent, bool keepWorld)
	{
		if (keepWorld)
		{
			// local de l'enfant est exprimé par le world du parent : on doit recalculer le local de l'enfant pour que son world reste inchangé.
			// todo : implémenter keepWorld. Pour l'instant, refuse la demande (cf. Annexe A8)
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

	/// <summary>
	/// Détruit une hiérarchie d'entités en partant de la racine fournie.
	/// </summary>
	/// <param name="registry">Référence au registre qui gère les entités et leurs stockages.</param>
	/// <param name="root">Entité racine du sous-arbre à détruire.</param>
	//void DestroyHierarchy(Registry& registry, Entity root)
	//{
	//	std::vector<Entity> subtree;
	//	CollectSubtree(registry, root, subtree);       // pré-ordre, root inclus

	//	Detach(registry, root);                        // l'ancien parent oublie le mort

	//	for (Entity e : subtree)
	//		registry.DestroyEntity(e);                 // les storages nettoient chacun leur part
	//}
	void DestroyHierarchy(Registry& registry, Entity root)
	{
		std::vector<Entity> subtree;
		CollectSubtree(registry, root, subtree);       // pré-ordre, root inclus

		Detach(registry, root);                        // l'ancien parent oublie le mort

		// Dénoue tout le sous-arbre AVANT toute destruction : à cet instant
		// chaque nœud interne est encore lié à ses enfants (et eux à lui).
		// Une fois cette passe faite, plus personne dans 'subtree' n'a de
		// lien sortant — la précondition de DestroyEntity pourra les voir
		// partir un par un sans jamais se déclencher à tort.
		for (Entity e : subtree)
			if (HierarchyComponent* h = registry.TryGet<HierarchyComponent>(e))
			{
				h->m_parent = NULL_ENTITY;
				h->m_children.clear();
			}

		for (Entity e : subtree)
			registry.DestroyEntity(e);                 // les storages nettoient chacun leur part
	}

	// Une entité a des liens sortants si la hiérarchie la considère encore
	// comme rattachée à quelque chose — parent vivant référencé, ou enfants
	// référencés. C'est la question que DestroyEntity a le droit de poser
	// sans avoir à connaître la forme de HierarchyComponent.
	[[nodiscard]] bool HasOutgoingLinks(const Registry& registry, Entity e)
	{
		const HierarchyComponent* h = registry.TryGet<HierarchyComponent>(e);
		if (!h) return false;
		return h->m_parent != NULL_ENTITY || !h->m_children.empty();
	}

}