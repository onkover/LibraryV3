#pragma once
#include "Entity.hpp"
#include "Registry.hpp"
#include <iostream>


namespace LV3
{
    class Registry;

    void linkChildToParent(Registry& registry, Entity child, Entity parent);
	///// <summary>
	//	/// Fonction pour lier un enfant à un parent (et vice versa) dans le Registry
	//	/// Pour une entité donnée dans HierarchyComponent, nous allons créer une nouvelle nouvelle hiérarchie pour référencer :
	//	/// * son parent
	//	/// * ses enfants
	//	/// 
	//	/// On recherche ensuite son parent pour référencer son enfant
	//	/// </summary>
	//	/// <param name="registry">Registre des composants</param>
	//	/// <param name="child">entité enfant</param>
	//	/// <param name="parent">entité parent</param>
	//static void linkChildToParent(Registry& registry, Entity child, Entity parent)
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
}