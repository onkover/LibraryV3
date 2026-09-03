#pragma once
#include "Entity.hpp"
#include "Registry.hpp"
//#include <iostream>


namespace LV3
{
    class Registry;

    void linkChildToParent(Registry& registry, Entity child, Entity parent);
    bool ValidateHierarchy(Registry& registry);

    // Convention A1 : pas de HierarchyComponent = racine.
    [[nodiscard]] inline bool IsRoot(const Registry& registry, Entity e)
    {
        const HierarchyComponent* h = registry.TryGet<HierarchyComponent>(e);
        return !h || IsRoot(*h);        // délègue à la forme canonique — un seul foyer pour la règle
    }
}