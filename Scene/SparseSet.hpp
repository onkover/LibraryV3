#pragma once

#include <vector>
#include <array>
#include <memory>   // pour std::unique_ptr, std::make_unique
#include "Components/Component.hpp"

namespace LV3
{

	// ============================================================
	//  PagedSparseArray — remplace un std::vector<uint32_t> plat par des pages allouées paresseusement. 
	//  Coût mémoire proportionnel aux entités RÉELLEMENT touchées, pas au plus grand index rencontré (B7).
	//  API : Get(idx) / Set(idx, value) — se substitue à un accès m_Sparse[idx] brut.
	// ============================================================
	class PagedSparseArray
	{
	public:
		static constexpr std::uint32_t PAGE_SIZE = 4096u;		// 16,7 M d'index / 4096 = 4096 pages max, soit 32 Ko de simples pointeurs si toutes les pages étaient utilisées
		static constexpr std::uint32_t INVALID_INDEX = (std::numeric_limits<std::uint32_t>::max)();

		[[nodiscard]] std::uint32_t Get(std::uint32_t idx) const noexcept
		{
			const std::uint32_t page = idx / PAGE_SIZE;
			if (page >= m_pages.size() || !m_pages[page])
				return INVALID_INDEX;			// page jamais allouée = aucune entité de cette plage n'a le composant
			return (*m_pages[page])[idx % PAGE_SIZE];
		}

		void Set(std::uint32_t idx, std::uint32_t value)
		{
			const std::uint32_t page = idx / PAGE_SIZE;
			EnsurePage(page);
			(*m_pages[page])[idx % PAGE_SIZE] = value;
		}

	private:
		using Page = std::array<std::uint32_t, PAGE_SIZE>;

		void EnsurePage(std::uint32_t page)
		{
			if (page >= m_pages.size())
				m_pages.resize(page + 1);		// agrandit le tableau de POINTEURS, pas les pages elles-mêmes

			if (!m_pages[page])
			{
				m_pages[page] = std::make_unique<Page>();
				m_pages[page]->fill(INVALID_INDEX);	// une page neuve est vierge par défaut
			}
		}

		std::vector<std::unique_ptr<Page>> m_pages;	// chaque page : 4096 * 4 = 16 Ko, allouée à la demande
	};

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
		SparseSet() = default;

		bool Contains(Entity entity) const override
		{

			const std::uint32_t idx = EntityIndex(entity);
			const std::uint32_t dense = m_Sparse.Get(idx);		// était : m_Sparse[idx]
			return dense != INVALID_INDEX
				&& dense < m_Dense.size()
				&& m_Entities[dense] == entity;
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
												// Assert = gratuit en Release, fatal et bruyant en Debug.

			return m_Dense[m_Sparse.Get(EntityIndex(entity))];		// était : m_Sparse[EntityIndex(entity)]

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
												// Assert = gratuit en Release, fatal et bruyant en Debug.

			assert(Contains(entity));
			return m_Dense[m_Sparse.Get(EntityIndex(entity))];
		}


		/// <summary>
		/// Ajoute un composant pour une entité : réserve l'espace si nécessaire, met à jour le composant existant si l'entité en possède déjà, sinon insère le composant et l'entité dans les structures de stockage sparse/dense.
		/// </summary>
		/// <param name="entity">Identifiant de l'entité pour laquelle le composant doit être ajouté ou mis à jour.</param>
		/// <param name="component">Composant à associer à l'entité. Reçu par valeur et déplacé (move) dans le stockage interne.</param>
		void Add(Entity entity, ComponentType component)
		{					
			const std::uint32_t idx = EntityIndex(entity);
			
			if (Contains(entity))
			{
				Get(entity) = std::move(component);
				return;
			}

			// Contrat : si Contains() est faux, le slot sparse DOIT être vierge. S'il pointe encore quelque part,
			// c'est qu'un composant d'une génération précédente n'a pas été nettoyé → DestroyEntity a fauté.
			assert(m_Sparse.Get(idx) == INVALID_INDEX && "Composant fantôme d'une génération antérieure détecté");

			const uint32_t dense_index = static_cast<uint32_t>(m_Dense.size());
			m_Sparse.Set(idx, dense_index);		// était : m_Sparse[idx] = dense_index
			m_Dense.push_back(std::move(component));
			m_Entities.push_back(entity);
		}

		/// <summary>
		/// Construit le composant DIRECTEMENT à sa place finale dans m_Dense, à partir des arguments
		/// de son constructeur. 
		/// Contrairement à Add(), aucun objet temporaire n'est créé : les arguments
		/// sont transmis (forwarding) jusqu'à emplace_back, qui appelle le constructeur de ComponentType
		/// une seule fois, dans le buffer du vector. Réservé aux entités qui n'ont pas encore le composant.
		/// </summary>
		/// <param name="entity">Entité à laquelle attacher le nouveau composant.</param>
		/// <param name="args">Arguments transmis tels quels au constructeur de ComponentType.</param>
		/// <returns>Référence vers le composant nouvellement construit.</returns>
		template<typename... Args>
		ComponentType& Emplace(Entity entity, Args&&... args)
		{
		
			// Contrat : Emplace sert à CRÉER, pas à mettre à jour. Si le composant existe déjà,
			// c'est un appel incorrect côté système — on le signale plutôt que de l'accepter en silence.			
			// emplace_back construit ComponentType(std::forward<Args>(args)...) directement dans le
			// buffer du vector : zéro copie, zéro déplacement intermédiaire.
			
			const std::uint32_t idx = EntityIndex(entity);
			assert(!Contains(entity) && "Emplace : le composant existe déjà");

			const uint32_t dense_index = static_cast<uint32_t>(m_Dense.size());
			m_Sparse.Set(idx, dense_index);		// était : m_Sparse[idx] = dense_index
			m_Entities.push_back(entity);
			return m_Dense.emplace_back(std::forward<Args>(args)...);
		}

		/// <summary>
		/// Supprime une entité du conteneur sparse/dense. Si l'entité n'existe pas, la fonction ne fait rien. Pour maintenir la compacité, l'élément à supprimer est remplacé par le dernier élément et les index sont mis à jour.
		/// </summary>
		/// <param name="entity">L'entité à supprimer. Si elle n'est pas présente dans le conteneur, l'appel est sans effet.</param>
		void Remove(Entity entity)
		{
			if (!Contains(entity)) return;

			const std::uint32_t idx = EntityIndex(entity);
			const std::uint32_t toRemove = m_Sparse.Get(idx);		// était : m_Sparse[idx]
			const std::uint32_t lastIdx = static_cast<std::uint32_t>(m_Dense.size() - 1);

			if (toRemove != lastIdx)							// évite le self-move quand on retire le dernier
			{
				const Entity lastEntity = m_Entities[lastIdx];
				m_Dense[toRemove] = std::move(m_Dense[lastIdx]);
				m_Entities[toRemove] = lastEntity;
				m_Sparse.Set(EntityIndex(lastEntity), toRemove);	// était : m_Sparse[EntityIndex(lastEntity)] = toRemove
			}

			m_Dense.pop_back();
			m_Entities.pop_back();
			m_Sparse.Set(idx, INVALID_INDEX);		// était : m_Sparse[idx] = INVALID_INDEX


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
	//	std::vector<uint32_t> m_Sparse;				// Indexé par Entity. Donne l'indice dans 'm_dense'.
		PagedSparseArray m_Sparse;					// Indexé par Entity. Donne l'indice dans 'm_dense
													// remplace std::vector<uint32_t> m_Sparse
		std::vector<Entity> m_Entities;				// Indexé par Entity. Miroir de m_dense, stocke l'ID de l'entité.
		std::vector<ComponentType> m_Dense;			// Stockage compact des composants. Son index provient de m_Sparse

		static constexpr uint32_t INVALID_INDEX = UINT32_MAX;	// const uint32_t INVALID_INDEX = UINT32_MAX;
																// évite de résever 4 octects pour rien

	};

}