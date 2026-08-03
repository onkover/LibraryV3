#pragma once
// ============================================================
//  Core/EventNames.h — Noms canoniques des evenements moteur
//
//  Une faute de frappe dans une chaine litterale ne produit
//  AUCUNE erreur : le systeme s'abonne a un evenement que
//  personne ne publie, et se tait pour toujours. C'est le bug
//  le plus penible a diagnostiquer de cette categorie.
//  Ici, une faute de frappe devient une erreur de compilation.
// ============================================================

namespace LV3::Events
{
    inline constexpr const char* TakingDamage = "TAKING_DAMAGE";
    inline constexpr const char* StartedTakingDamage = "STARTED_TAKING_DAMAGE";
    inline constexpr const char* StoppedTakingDamage = "STOPPED_TAKING_DAMAGE";
    inline constexpr const char* EntityDied = "ENTITY_DIED";
}