#pragma once

#include <vector>
#include <optional>	// c++ v17
#include <array>
#include <utility>
#include "Components/Component.hpp"
#include "SparseSet.hpp"

/*********************************************************************

m_Storages (std::vector<std::unique_ptr<IComponentStorage>>)
	|
	+-- Index 0 [ID du composante Position]		--> unique_ptr --> SparseSet<Position> { ... }
	|
	+-- Index 1 [ID du composante Velocity]		--> unique_ptr --> SparseSet<Velocity> { ... }
	|
	+-- Index 2 [ID du composante Transform]	--> unique_ptr --> SparseSet<Transform> { ... }
	|
	+-- ...
	|
	+-- Index X [ID du composante YYY]			--> unique_ptr --> SparseSet<YYY> { ... }

Performance d'accès (GetComponent) : O(1).
* GetTypeID<T>() : O(1) (statique)
* m_Storages[typeID] : O(1) (accès vector)
* storage->Get(entity) : O(1) (accès sparse set)

Performance d'itération (Systèmes) : O(N),
	où N est le nombre de composants (pas d'entités !).
	C'est la performance maximale possible grâce à l'accès direct au std::vector dense via la fonction View().

Flux d'une Opération (Ex: registry.addComponent(E_1, TransformComponent{...})) :
	1. Votre code appelle registry.addComponent(E_1, monTransform).
	2. Le Registry demande au ComponentTypeManager l'ID de TransformComponent (disons que c'est 0).
	3. Le Registry regarde dans m_Storages[0].
	4. Il trouve (ou crée si c'est la première fois) le SparseSet<TransformComponent>.
	5. Le Registry demande à ce SparseSet d'ajouter monTransform pour l'entité E_1.
	6. Le SparseSet<TransformComponent> :
		* Met à jour m_sparse[E_1] pour qu'il pointe vers le nouvel emplacement dans m_dense (par exemple, l'index 2).
		* Ajoute monTransform à la fin de m_dense.
		* Ajoute E_1 à la fin de m_entities.

Flux d'une Opération (Ex: for (auto& transform : registry.View<TransformComponent>())) :
	1. Votre système appelle registry.View<TransformComponent>().
	2. Le Registry (via GetStorage) récupère le SparseSet<TransformComponent>.
	3. Le Registry appelle sparseSetTransform->GetDenseData().
	4. Cette fonction retourne directement une référence au m_dense (std::vector<TransformComponent>).
	5. Votre boucle for parcourt ce std::vector de manière ultra-rapide et contiguë en mémoire.

**********************************************************
Exemples d'utilisation

1. Utilisation des données du composants uniquement -> on parcourt le pool de composants Dense
	2 façons pour obtenir un pool de composants
	* soit : auto& mesh = registry.View<MeshComponent>();
	* soit : SparseSet<PlayerControlComponent>* playerControlStorage = registry.getStorage<PlayerControlComponent>();

		for (auto& mesh : registry.View<MeshComponent>())
		{
				mesh.m_currentOrbitAngle += mesh.m_orbitalSpeed * deltaTime;
				mesh.m_currentRotationAngle += mesh.m_rotationSpeed * deltaTime;
		}

2. Utilisation de plusieurs componsants -> On choisit d'itérer sur le pool le moins nombreux pour minimiser les itérations 
	* Recupération du pool de composants : SparseSet<PlayerControlComponent>* playerControlStorage = registry.getStorage<PlayerControlComponent>();
	* Recupération du son Dense : auto& playerControlComponents = playerControlStorage->GetDenseData();
	* Recupération de son Entites : auto& playerControlEntitites = playerControlStorage->GetDenseEntities();	// Permet de retrouver l'EntityID

		for (size_t i = 0; i < playerControlComponents.size(); i++)
		{
			Entity entity = playerControlEntitites[i];
			if (registry.hasComponent<TransformComponent>(entity))
			{
				auto& transform= registry.getComponent<TransformComponent>(entity);
				auto& control= playerControlComponents[i];

				transform.m_initialLocalPosition.x += control.m_speed * deltaTime;
			}
		}

*********************************************************************/


// Simple gestionnaire d'ID de type
// Permettra d'obtenir un ID unique pour chaque type de composant
class ComponentTypeManager
{
public:
	template  <typename T>
	static uint32_t GetTypeID()
	{
		// 'static' ici signifie que 'id' n'est initialisé qu'UNE SEULE FOIS pour chaque type 'T'.
		// A chaque T différent, s_NextNextID est incrémenté, sinon, la valeur demeurera la même pour un T donné
		static uint32_t id = s_NextNextID++;
		return id;
	}

private:
	static inline uint32_t s_NextNextID = 0;
};
// --- Exemple d'utilisation ---
// uint32_t posID = ComponentTypeManager::GetTypeID<Position>(); // Retournera 0
// uint32_t velID = ComponentTypeManager::GetTypeID<Velocity>(); // Retournera 1
// uint32_t posID_encore = ComponentTypeManager::GetTypeID<Position>(); // Retournera 0


//********************************************************************
/* Le SEUL conteneur de stockage ! C'est un tableau de pointeur (unique_ptr) de sparseSet<composantXXXX>
	On ne peut pas stocker tous nos SparseSet dans un seul conteneur (comme un std::vector), ils doivent partager un type de base commun.
	Deux concepts :
	1. Pour stocker tous nos SparseSet dans un seul conteneur (comme un std::vector), ils doivent partager un type de base commun : IComponentStorage
	L'objectif est de "gommer le type" (type erasure) pour pouvoir stocker tous les pools de composants dans un seul std::vector.
	Pour ce faire, nous avons besoin d'une classe de base non-template (interface) pour que notre Registry puisse contenir des pointeurs vers n'importe quel type de SparseSet.
		Cette interface est "IComponentStorage"
	2. Pour un accès en O(1), nous n'allons pas utiliser de std::map avec type_index (qui est en O(log N)). Nous allons attribuer un ID entier unique (0, 1, 2...) à chaque type de composant.

*/
class Registry
{
public:
	Registry() = default;

	Entity CreateEntity()
	{
		Entity newEntity;

		/*
		recyclage simple des ID
		La prochaine version serait de versionner les ID pour identifier après recyclage d'une entité, qu'un script/composant n'utilise pas les anciennes données de l'ancienne entité (qui auraient été concervées par erreur) mais bien les données de la nouvelle entité
		Dans ce cas, il faudrait travailler non plus avec seulement une variable entité mais un couple entité/version
		*/
		if (!m_FreeEntities.empty())
		{
			// On recycle un ID de la liste des IDs libres
			newEntity = m_FreeEntities.back();	// Prend le dernier ID
			m_FreeEntities.pop_back();			// Le retire de la liste
		}
		else
			newEntity = m_EntityCount++;	// Plus d'IDs libres, on génère un nouvel ID

		return newEntity;
	}

	void DestroyEntity(Entity entity)
	{
		// Doit notifier TOUS les pools de composants que l'entité est détruite
		for (auto& storage : m_Storages)
		{
			if (storage)
				storage->OnEntityDestroyed(entity);
		}
		
		// Ensuite, ajouter l'EntityID à la liste des IDs libres pour recyclage
		m_FreeEntities.push_back(entity);
	}

	template <typename T>
	void addComponent(Entity entity, T component)
	{
		getStorage<T>()->Add(entity, std::move(component));
	}


	template <typename T>
	bool hasComponent(Entity entity) const
	{
		uint32_t typeID = ComponentTypeManager::GetTypeID<T>();

		// Si le pool n'a jamais été créé, l'entité ne peut pas l'avoir
		if (typeID >= m_Storages.size() || !m_Storages[typeID])
			return false;
		// Utilise la version const de SparseSet<T>::Has
		return static_cast<const SparseSet<T>*>(m_Storages[typeID].get())->Has(entity);
	}


	template <typename T>
	void removeComponent(Entity entity)
	{
		// Seule la version non-const de getStorage peut être utilisée pour la modification
		// Assurez-vous que le storage existe avant d'essayer de supprimer
		uint32_t typeID = ComponentTypeManager::GetTypeID<T>();
		if (typeID < m_Storages.size() && m_Storages[typeID]) {
			SparseSet<T>* storage = static_cast<SparseSet<T>*>(m_Storages[typeID].get());
			if (storage->Has(entity)) { // Vérifie si le composant existe avant de tenter de le supprimer
				storage->Remove(entity);
			}
		}
	}

	//************************************************************************

	// Renvoie une référence modifiable au composant.
	template <typename T>
	T& getComponent(Entity entity)
	{
		// La version non - const peut créer le storage si nécessaire(via getStorage<T>())
		// Une assertion ou gestion d'erreur si l'entité n'a pas le composant serait utile
		return getStorage<T>()->Get(entity);
	}

	// Renvoie une référence en lecture seule. Destinée aux contextes où Registry est const.
	template <typename T>
	const T& getComponent(Entity entity) const
	{
		// La version const ne peut PAS créer le storage.
		// Elle doit s'assurer que le storage existe et que le composant est présent.
		const SparseSet<T>* storage = getStorage<T>();		// Utilise la version const de getStorage
		assert(storage && storage->Has(entity));			// Vérification stricte en debug : si absent, c'est une erreur de logique
		return storage->Get(entity);
	}

	//************************************************************************
	// getStorage<T>() retourne SparseSet<T>*

	// Récupère le stockage pour un type T. Le crée s'il n'existe pas.
	template <typename ComponentType>
	SparseSet<ComponentType>* getStorage()
	{
		// 1. Obtenir l'ID unique pour ce type de composant
		uint32_t typeID = ComponentTypeManager::GetTypeID<ComponentType>();

		// 2. S'assurer que notre vector est assez grand
		if (typeID >= m_Storages.size())
		{
			m_Storages.resize(typeID + 1);		// Agrandit le vector
		}

		// 3. Créer le stockage s'il n'existe pas
		if (!m_Storages[typeID])
		{
			m_Storages[typeID] = std::make_unique<SparseSet<ComponentType>>();
		}

		// 4. "static_cast" est sûr ici. Nous SAVONS que m_Storages[typeID] est un SparseSet<ComponentType> grâce à notre logique.
		return static_cast<SparseSet<ComponentType>*>(m_Storages[typeID].get());

	}

	// Récupère le stockage pour un type T ou retourne nullptr s'il n'existe pas (version const).
	template <typename ComponentType>
	const SparseSet<ComponentType>* getStorage() const
	{
		uint32_t typeID = ComponentTypeManager::GetTypeID<ComponentType>();

		// NE PAS modifier m_Storages dans la version const. Si le storage n'existe pas, retourner nullptr (lecture seule).
		if (typeID >= m_Storages.size() || !m_Storages[typeID])
			return nullptr;

		return static_cast<const SparseSet<ComponentType>*>(m_Storages[typeID].get());
	}



	//************************************************************************
	// View<T>() : retourne la référence au std::vector<T> dense du SparseSet<T>
	// itération mono - composant, très rapide
	
	// Helper pour retourner une référence à un vecteur dense vide et constant. // Utilisez-le UNIQUEMENT pour la version const de View()
	template <typename T>
	static const std::vector<T>& GetEmptyDenseVector() {
		static const std::vector<T> empty_vec; // Un vecteur vide statique par type T
		return empty_vec;
	}

	// Donne un accès direct au vector dense pour une itération. 100% "cache-friendly".
	template <typename T>
	std::vector<T>& View()	// Cette View reste pour les itérations mono-composant
	{
		return getStorage<T>()->GetDenseData();
	}
	
	template <typename T>
	const std::vector<T>& View() const
	{
		const SparseSet<T>* storage = getStorage<T>();

		// Si le storage n'existe pas, retourne une référence à un vecteur vide constant.
		if (!storage) {
			return GetEmptyDenseVector<T>();
		}
		return storage->GetDenseData();
	}
	
	//************************************************************************
	// ComponentView est un helper(range) pour itérer efficacement sur un groupe de composants : il choisit le SparseSet le plus petit comme base et filtre les entités qui possèdent tous les composants requis.
	/*
	•	Lambda capture : [&] { ... }
		•	[&] capture toutes variables locales par référence(utilisé ici pour idx, m_componentTypeIDs, registry, etc.).
		•	[=] capturerait par valeur.Tu peux aussi écrire[this], [&idx], etc.
		•	Appel immédiat de la lambda : ()
		•	La lambda est définie et immédiatement appelée pour chaque type de composant dans ComponentTypes...

	•	Fold expression / / coma-fold : (expr, ...)
		•	Cette syntaxe est une "fold expression" introduite en C++17.
		•	Elle permet d'appliquer une opération (ici, l'appel de la lambda) à chaque élément d'un parameter pack.
		•	Dans ce cas, pour chaque type de composant dans ComponentTypes..., la lambda est appelée avec le type actuel.
		•	(expr(), ...) appelle expr() pour chaque élément du pack.

	•	f(args...) où args... est une expansion, ou des formes comme (... , expression) pour appliquer une expression à tous les éléments (fold expression).

	Exemple : auto printAll = [](auto... xs){ ( (std::cout << xs << '\n'), ... ); };


	*/
	template<typename... ComponentTypes>
	class ComponentView
	{
		friend class Registry; // Permet au Registry d'accéder au constructeur privé
		Registry* m_registry; // Pointeur vers le Registry parent

		// Tuple de pointeurs vers les SparseSet<ComponentTypes> spécifiques. Permet un accès typé et rapide aux stockages individuels.
		std::tuple<SparseSet<ComponentTypes>*...> m_storages_ptr_tuple;

		// Pointeur vers le SparseSet choisi comme base d'itération (le plus petit). Il est de type IComponentStorage* pour le polymorphisme.
		const IComponentStorage* m_mainStorage = nullptr;

		// Contient les IDs IDs des composants (obtenus via ComponentTypeManager::GetTypeID<T>()) correspondant à chaque type de composant du ComponentView.
		// Utile pour diagnostics ou comparaisons directes (vérifications d'ID directes).
		std::array<uint32_t, sizeof...(ComponentTypes)> m_componentTypeIDs;

		// Constructeur privé : seule la méthode View du Registry peut créer un ComponentView
		ComponentView(Registry* registry) // <-- Le paramètre est Registry* non-const
			: m_registry(registry),
			m_storages_ptr_tuple(registry->template getStorage<ComponentTypes>()...)			// Ici registry->getStorage<ComponentTypes>()... est une expansion de pack : pour chaque ComponentType on appelle getStorage<X>(); le résultat(pointeurs possiblement nuls) est stocké dans un std::tuple<SparseSet<ComponentTypes>*...>.
		{

			// 2. Initialiser m_componentTypeIDs
			// fold - expression + appel immédiat d'une lambda pour chaque type du pack. Pour chaque ComponentType on stocke son ID dans le tableau en incrémentant idx.
			// cette forme est un pattern courant pour exécuter un bloc pour chaque élément d'un parameter pack.
			size_t idx = 0;
			([&]
			{
				m_componentTypeIDs[idx++] = ComponentTypeManager::GetTypeID<ComponentTypes>();
			}(), ...);

			// 3. Trouver le stockage le plus petit pour m_mainStorage (base d'itération)
			size_t min_size = (std::numeric_limits<size_t>::max)();
			bool any_storage_null = false; // Pour détecter si un composant requis n'a pas de storage

			// std::apply permet d'appeler une lambda avec les éléments du tuple
			// Déplie un std::tuple en arguments d'une lambda/fonction : std::apply(lambda, tuple).
			std::apply([&](auto... storages) 
			{
				// Cette fold expression va itérer sur chaque 'storage' (qui est un SparseSet<T>*)
				([&](auto* storage_ptr) 
				{
					if (storage_ptr)
					{ // Si le SparseSet existe
						if (storage_ptr->Size() < min_size)
						{
							min_size = storage_ptr->Size();
							m_mainStorage = storage_ptr; // Stocke le pointeur polymorphique
						}
					}
					else
					{
						any_storage_null = true;	// Si un des stockages n'existe pas, la vue est vide. On marque qu'un storage est null et on peut court-circuiter.
					}

				}(storages), ...); // Exécute la lambda pour chaque élément du pack 'storages'

			}, m_storages_ptr_tuple);	// std::apply([&](auto... storages){ (...) }, m_storages_ptr_tuple); déroule la tuple en arguments storages.

			// Si un des stockages était nul, la vue est vide.
			if (any_storage_null) 
			{
				m_mainStorage = nullptr;
			}
		}

	public:
		// --- CLASSE ITERATOR (L'itérateur pour ComponentView) ---
		/* 
			Itère sur le vecteur d'entités denses du m_mainStorage : m_currentDenseIndex 
			L'itération parcourt le pool le plus petit et vérifie la présence des autres composants sur chaque entité candidate (donc sensiblement : O(M * K) où M = taille du plus petit pool, K = nombre de composants à vérifier ; chaque vérif est O(1)).
		*/ 
		class Iterator 
		{
			const ComponentView* m_view; // Pointeur vers la ComponentView parente
			size_t m_currentDenseIndex;  // Index actuel dans le tableau dense d'entités du m_mainStorage : m_mainStorage->GetDenseEntities() 

			// Méthode clé : avance l'itérateur jusqu'à la prochaine entité valide (jusqu'à trouver une entité qui possède effectivement tous les composants requis)
			void SkipInvalidEntities()
			{
				// Si la vue est invalide (pas de mainStorage ou mainStorage est vide), l'itérateur est à la fin
				if (!m_view->m_mainStorage || m_currentDenseIndex >= m_view->m_mainStorage->GetDenseEntities().size()) {
					m_currentDenseIndex = m_view->m_mainStorage ? m_view->m_mainStorage->GetDenseEntities().size() : 0; // Positionne à la fin
					return;
				}

				const auto& mainEntities = m_view->m_mainStorage->GetDenseEntities();
				const size_t mainSize = mainEntities.size(); // Utilise la taille réelle du vecteur d'entités

				while (m_currentDenseIndex < mainSize)
				{
					Entity currentEntity = mainEntities[m_currentDenseIndex];

					bool allComponentsPresent = true;
					// Utilise une fold expression pour vérifier la présence de TOUS les ComponentTypes
					// Important : utilise la version const de hasComponent
					// •	Pour chaque entité candidate, on effectue : ([&] { if (!m_view->m_registry->template hasComponent<ComponentTypes>(currentEntity)) allComponentsPresent = false; }(), ...);
					// •	Ce test utilise la version const de hasComponent<T>() (O(1) par composant).
					([&] 
					{
						if (!m_view->m_registry->template hasComponent<ComponentTypes>(currentEntity))
						{
							allComponentsPresent = false;
						}
					}(), ...);

					if (allComponentsPresent)
					{
						break; // Trouvé une entité qui a TOUS les composants requis
					}
					m_currentDenseIndex++; // Entité non valide, passer à la suivante
				}
			}

		public:
			// Typedefs standards pour un itérateur C++
			using value_type = std::tuple<Entity, ComponentTypes&...>; 
			using reference = value_type&; // On retourne le tuple par valeur (RVO optimisera cela)

			// Constructeur de l'itérateur
			Iterator(const ComponentView* view, size_t startIndex)
				: m_view(view), m_currentDenseIndex(startIndex)
				// PAS BESOIN D'INITIALISER m_currentTuple ici, std::optional le gère.
			{
				SkipInvalidEntities(); // Avancer au premier élément valide (ou à la fin)
			}

			/*
			•	Construit et retourne un std::tuple<Entity, ComponentTypes&...> contenant l'Entity et des références aux composants.
			•	Problème de durée de vie : on ne peut pas retourner une référence vers un tuple tempora. 
					=> Solution du code : mutable std::optional<value_type> m_currentTuple; est rempli (emplace) puis on retourne *m_currentTuple par référence.			
			*/
			reference operator*() const
			{
				const auto& mainEntities = m_view->m_mainStorage->GetDenseEntities();
				Entity currentEntity = mainEntities[m_currentDenseIndex]; // currentEntity est une copie par valeur

				m_currentTuple.emplace( // Utilise emplace() si m_currentTuple est optional, sinon =
					currentEntity, // <-- Passera la valeur de currentEntity
					m_view->m_registry->template getComponent<ComponentTypes>(currentEntity)...
				);
				return *m_currentTuple; // ou return m_currentTuple si pas optional
			}

			// Opérateur d'incrémentation (pré-incrément : ++it)
			Iterator& operator++() 
			{
				m_currentDenseIndex++;
				SkipInvalidEntities(); // Avancer au prochain élément valide (ou à la fin)
				return *this;
			}

			// Opérateur d'incrémentation (post-incrément : it++)
			Iterator operator++(int) 
			{
				Iterator temp = *this;
				++(*this);
				return temp;
			}

			// Opérateurs de comparaison (pour savoir quand la boucle for-each doit s'arrêter)
			bool operator==(const Iterator& other) const {
				return m_currentDenseIndex == other.m_currentDenseIndex && m_view == other.m_view;
			}
			bool operator!=(const Iterator& other) const {
				return !(*this == other);
			}

		private:
			// Il stockera le tuple de références que operator*() retournera par référence
			mutable std::optional<value_type> m_currentTuple;	// "mutable" permet à cette donnée d'être modifiée même dans une méthode const operator*().

		}; // Fin de la classe Iterator

		// Méthodes begin() et end() : nécessaires pour que ComponentView soit un range utilisable dans for-each
		Iterator begin() const
		{
			return Iterator(this, 0); // Démarre l'itérateur au début
		}

		Iterator end() const
		{
			// L'itérateur de fin pointe juste après le dernier élément valide
			// Si m_mainStorage est nullptr (vue invalide), la fin est 0 pour que begin() == end().
			return Iterator(this, m_mainStorage ? m_mainStorage->GetDenseEntities().size() : 0);
		}

	}; // Fin de la classe ComponentView

	//************************************************************************
	// ViewGroup<...>() construit et retourne un ComponentView<ComponentTypes...> pour itérations multi - composants.
	template<typename... ComponentTypes>
	ComponentView<ComponentTypes...> ViewGroup() {
		return ComponentView<ComponentTypes...>(this);
	}

	uint32_t getEntityCount() const
	{
		return m_EntityCount;
	}

private:
	Entity m_EntityCount = 0;

	std::vector<std::unique_ptr<IComponentStorage>> m_Storages;		// Le conteneur principal de pointeurs sur un SparseSet<T> pour un type T. O(1) pour l'accès par index.
	std::vector<Entity> m_FreeEntities;	// Liste des EntityIDs disponibles pour recyclage
};

