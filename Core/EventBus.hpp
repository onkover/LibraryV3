#pragma once
// SYSTÈME D'ÉVÉNEMENTS (EventBus)

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <iostream>

#include "../Scene/Entity.hpp"
namespace LV3
{

    class Registry;

    // Signature de nos callbacks d'événement (l'entité qui a déclenché, et l'entité qui est entrée)
    using EventCallback = std::function<void(Entity, Entity)>;

    class EventBus
    {
    public:

        /// <summary>
        // S'abonner à un événement    
        /// </summary>
        /// <param name="eventType">type d'event</param>
        /// <param name="callBack">nom de la fonction avec ses paramètres</param>

        void subscribe(const std::string& eventType, EventCallback callBack)
        {
            m_subscribers[eventType].push_back(callBack);
            std::cout << "[EventBus] Nouvel abonné pour l'événement : " << eventType << std::endl;
        }

        /// <summary>
        /// Publier (déclencher) un événement : collision entre 2 entitées
        /// </summary>
        /// <param name="eventType">type d'event</param>
        /// <param name="entity1">entité qui a déclenché l'évènement</param>
        /// <param name="entity2">entité qui est entrée</param>
        void publish(const std::string& eventType, Entity entity1, Entity entity2)
        {
            // Il y a quelqu'un pour cet événement ?
            if (m_subscribers.count(eventType))
            {
                // Appelle tous les callbacks abonnés à cet événement
                for (auto& callback : m_subscribers[eventType])
                {
                    callback(entity1, entity2);
                }
            }
        }

    private:
        // Map qui associe un nom d'événement (string) à une liste de fonctions (callbacks)
        std::map<std::string, std::vector<EventCallback>> m_subscribers;

    };
}