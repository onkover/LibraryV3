#pragma once

//#include <string>
#include <vector>
#include "Components/Component.hpp"

namespace LV3
{
	// pour le SparseSet, taille initiale du tableau sparse (m_sparse)
	//const size_t MAX_ENTITIES_INIT = 10;
	inline constexpr size_t MAX_ENTITIES_INIT = 10000;	// inine au lieu de const pour éviter les problèmes de linkage multiple (par unité de compilation à cause du CONST)

	//********************************************************************

	// interface de base / de stockage pour le "type erasure"
	// Permet de stocker différents SparseSet<T> dans un vector de pointeurs de base.
	class IComponentStorage
	{
	public:
		virtual ~IComponentStorage() = default;
		virtual void OnEntityDestroyed(Entity entity) = 0;		// gestion de la suppression d'une entité
		virtual  size_t Size() const = 0;						// pour trouver le plus petit SparseSet
		virtual bool Contains(Entity entity) const = 0;			//pour vérifier de manière polymorphique
		virtual const std::vector<Entity>& GetDenseEntities() const = 0;	// pour l'itération polymorphique
	};

	//********************************************************************
	/*
	Les Stockages de Composants (SparseSets) :
		Il y a un SparseSet par type de composant(un SparseSet<TransformComponent>, un SparseSet<NameComponent>, etc.).

		m_sparse (Le "dictionnaire d'entités") : C'est un grand tableau où l'index est l'EntityID. La valeur à cet index est l'emplacement réel du composant de cette entité dans m_dense. S'il n'y a pas de composant, l'entrée est "invalide".
		m_dense (Le "coffre aux trésors compact") : C'est un std::vector qui contient les composants eux-mêmes, empilés les uns après les autres sans trous. C'est le rêve du cache CPU !
		m_entities (Le "miroir des entités") : C'est un std::vector qui garde la trace de quelle entité possède le composant à l'index correspondant dans m_dense.

		Les Stockages de Composants (SparseSets) :
		Un SparseSet est une structure de données optimisée pour stocker des composants dans un ECS.
		Il y a un SparseSet par type de composant (ex: SparseSet<TransformComponent>).

		m_sparse (Le "dictionnaire d'entités") :
			- C'est un grand tableau où l'index est l'EntityID.
			- La valeur à cet index est l'emplacement réel du composant de cette entité dans m_Dense.
			- Si l'entité n'a pas de composant, l'entrée est INVALID_INDEX.
			- Permet un accès en O(1) pour vérifier l'existence ou obtenir un composant par EntityID.

		m_Dense (Le "coffre aux trésors compact") :
			- C'est un std::vector qui contient les composants eux-mêmes, empilés les uns après les autres sans trous.
			- Offre une excellente localité cache et permet une itération ultra-rapide en O(N) où N est le nombre de composants de ce type.

		m_Entities (Le "miroir des entités") :
			- C'est un std::vector qui garde la trace de quelle EntityID possède le composant à l'index correspondant dans m_Dense.
			- Utilisé lors de la suppression d'un composant pour mettre à jour m_Sparse correctement après un "swap-and-pop".

	*/

	template<typename ComponentType>
	class SparseSet : public IComponentStorage
	{
	public:
		SparseSet()
		{
			m_Sparse.resize(MAX_ENTITIES_INIT, INVALID_INDEX);
		}

		/// <summary>
		/// Détermine si une entité donnée est présente dans la structure (vérification via les tableaux sparse/dense).
		/// </summary>
		/// <param name="entity">Identifiant de l'entité à vérifier.</param>
		/// <returns>true si l'entité est présente (indice dense valide et correspondance dans m_Entities), sinon false.</returns>
		//bool Has(Entity entity) const
		//{
		//	if (entity >= m_Sparse.size())
		//		return false;

		//	const uint32_t dense_index = m_Sparse[entity];
		//	return (dense_index != INVALID_INDEX) && (dense_index < m_Dense.size()) && (m_Entities[dense_index] == entity);
		//}

		/// <summary>
		/// Retourne vrai si l'entité spécifiée est présente.
		/// </summary>
		/// <param name="entity">L'entité à vérifier.</param>
		/// <returns>true si l'entité est présente (Has(entity) est vraie), sinon false.</returns>
		
		/*
		ContainsEntity n'est qu'un vernis virtuel par-dessus Has, 
		nécessaire parce que le code générique (comme Registry::DestroyEntity) manipule des IComponentStorage* sans connaître le type concret, donc sans accès direct à Has().		
		*/
		//bool Contains(Entity entity) const override
		//{
		//	return Has(entity);
		//}
		bool Contains(Entity entity) const override
		{
			//const std::uint32_t idx = EntityIndex(entity);	==> pour le versionning plus tard
			//
			//if (idx >= m_Sparse.size()) 
			//	return false;
			
			if (entity >= m_Sparse.size())
				return false;

			//const std::uint32_t dense = m_Sparse[idx];==> pour le versionning plus tard
			const std::uint32_t dense = m_Sparse[entity];
			return dense != INVALID_INDEX && dense < m_Dense.size() && m_Entities[dense] == entity;
		}

		/// <summary>
		/// Retourne une référence mutable au composant associé à l'entité fournie.
		/// </summary>
		/// <param name="entity">Identifiant de l'entité dont on souhaite obtenir le composant.</param>
		/// <returns>Référence au ComponentType attaché à l'entité, permettant de lire ou modifier le composant. Comportement indéfini si l'entité n'a pas ce composant.</returns>
		ComponentType& Get(Entity entity)
		{
			assert(Contains(entity));			// On s'assure quele composant est là.
											// m_Sparse[entity] peut valoir INVALID_INDEX
											//  Asserty = gratuit en Release, fatal et bruyant en Debug.
			return m_Dense[m_Sparse[entity]];
		}

		/// <summary>
		/// Retourne une référence constante au composant associé à l'entité fournie.
		/// </summary>
		/// <param name="entity">Identifiant de l'entité dont on souhaite obtenir le composant.</param>
		/// <returns>Référence constante vers le ComponentType associé à l'entité. Comportement indéfini si l'entité n'est pas présente dans le conteneur (l'appel suppose que l'entité a un composant).</returns>
		const ComponentType& Get(Entity entity) const
		{
			assert(Contains(entity));			// On s'assure quele composant est là.
											// m_Sparse[entity] peut valoir INVALID_INDEX
											//  Asserty = gratuit en Release, fatal et bruyant en Debug.
			return m_Dense[m_Sparse[entity]];
		}


		/// <summary>
		/// Ajoute un composant pour une entité : réserve l'espace si nécessaire, met à jour le composant existant si l'entité en possède déjà, sinon insère le composant et l'entité dans les structures de stockage sparse/dense.
		/// </summary>
		/// <param name="entity">Identifiant de l'entité pour laquelle le composant doit être ajouté ou mis à jour.</param>
		/// <param name="component">Composant à associer à l'entité. Reçu par valeur et déplacé (move) dans le stockage interne.</param>
		void Add(Entity entity, ComponentType component)
		{
			EnsureSparseFits(entity);

			if (Contains(entity))
			{
				// L'entité a déjà ce composant, on le met à jour
				Get(entity) = std::move(component);
				return;
			}

			uint32_t dense_index = static_cast<uint32_t>(m_Dense.size());

			m_Sparse[entity] = dense_index;
			m_Dense.push_back(std::move(component));
			m_Entities.push_back(entity);
		}


		/// <summary>
		/// Supprime une entité du conteneur sparse/dense. Si l'entité n'existe pas, la fonction ne fait rien. Pour maintenir la compacité, l'élément à supprimer est remplacé par le dernier élément et les index sont mis à jour.
		/// </summary>
		/// <param name="entity">L'entité à supprimer. Si elle n'est pas présente dans le conteneur, l'appel est sans effet.</param>
		void Remove(Entity entity)
		{
			if (!Contains(entity))
				return;

			uint32_t index_to_remove = m_Sparse[entity];
			uint32_t last_index = static_cast<uint32_t>(m_Dense.size() - 1);
			Entity last_entity = m_Entities[last_index];

			// Swap
			m_Dense[index_to_remove] = std::move(m_Dense[last_index]);
			m_Entities[index_to_remove] = last_entity;

			// Pop
			m_Dense.pop_back();
			m_Entities.pop_back();

			// Mettre à jour sparse pour l'entité déplacée et invalider l'entité supprimée
			m_Sparse[last_entity] = index_to_remove;
			m_Sparse[entity] = INVALID_INDEX;
		}

		// --- Fonction override pour la gestion de la destruction d'entités ---
		void OnEntityDestroyed(Entity entity) override
		{
			if (Contains(entity))
				Remove(entity);
		}

		// Retourne le nombre de composants stockés
		size_t Size() const override
		{
			return m_Dense.size();
		}

		// --- Fonctions clés pour les Systèmes (accès direct aux données denses) ---
		std::vector<ComponentType>& GetDenseData()
		{
			return m_Dense;
		}

		// Version non-const pour l'accès interne ou direct si nécessaire
		const std::vector<ComponentType>& GetDenseData() const
		{
			return m_Dense;
		}

		std::vector<Entity>& GetDenseEntities()
		{
			return m_Entities;
		}

		// Version const pour l'itération polymorphique dans ComponentView
		const std::vector<Entity>& GetDenseEntities() const override
		{
			return m_Entities;
		}

	private:
		std::vector<uint32_t> m_Sparse;				// Indexé par Entity. Donne l'indice dans 'm_dense'.
		std::vector<Entity> m_Entities;				// Indexé par Entity. Miroir de m_dense, stocke l'ID de l'entité.
		std::vector<ComponentType> m_Dense;			// Stockage compact des composants. Son index provient de m_Sparse

		static constexpr uint32_t INVALID_INDEX = UINT32_MAX;	// const uint32_t INVALID_INDEX = UINT32_MAX;
																// évite de résever 4 octects pour rien


		// Helper pour redimensionner m_sparse si nécessaire
		void EnsureSparseFits(Entity entity)
		{
			if (entity >= m_Sparse.size())
			{
				m_Sparse.resize(entity + 1, INVALID_INDEX);	// Redimensionne et initialise avec la valeur invalide
			}
		}
	};

}