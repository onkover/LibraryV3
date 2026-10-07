#pragma once
// ============================================================
//  Core/Profiler.h — instrument de mesure par frame (phase G)
//
//  HORS pch.h : a inclure a la main dans les .cpp instrumentes.
//  Commutateur : LV3_PROFILE (Core/config.h), pose par
//  LV3.Common.props pour la LIB *et* l'EXE (ODR).
//
//  Principes, dans l'ordre d'importance :
//   1. l'instrument ne doit pas perturber la mesure -> un enum dense
//      indexe un tableau ; jamais de chaine, jamais de table de hachage,
//      jamais d'allocation dans la boucle.
//   2. on stocke des ENTIERS de nanosecondes ; la conversion en ms se
//      fait une seule fois, a l'ecriture du CSV.
//   3. l'instrument se mesure lui-meme (OverheadNs) : un chiffre dont
//      on ignore la marge d'erreur n'est pas un chiffre.
// ============================================================

#include "Core/config.h"     // LV3_PROFILE : l'en-tete ne depend d'aucun ordre d'inclusion

#include <chrono>
#include <cstdint>
#include <iterator>          // std::size
#include <string>

namespace LV3
{

	// ── Les zones : un enum DENSE. L'ordre suit S13, donc le CSV se lit
	//    de gauche a droite comme la frame se deroule.
	enum class EProfZone : uint8_t
	{
		Input = 0,      // BuildInputState + PlayerInputSystem
		Animation,      // AnimationSystem (simDt)
		LocalXform1,    // cuisson n°1
		WorldXform1,
		Cameras,        // FPS / Follow / Zoom / bindings / gizmos
		LocalXform2,    // cuisson n°2, ciblee
		WorldXform2,
		Trigger,        // TriggerSystem
		BuildViews,     // BuildViewData x N
		Clear,          // Clean_Render + BeginFrame
		Render,         // RenderView x N  <- la rasterisation
		Present,        // SDL : HORS budget moteur, mesure pour etre exclu

						// ── SOUS-ZONES (chantier Clear). Chacune est INCLUSE dans une zone
						//    principale : elles sont EXCLUES de la somme qui calcule RESTE,
						//    sinon le temps serait compte deux fois. Toujours APRES Present :
						//    les colonnes des CSV de reference gardent leur position.
		ClearColor,     // Clean_Render  (inclus dans Clear)
		ClearDepth,     // BeginFrame -> DepthBuffer::Clear (inclus dans Clear)

		Count
	};

	// ── Les compteurs. C'est EUX qui donnent la complexite : un temps
	//    seul ne distingue jamais un O(N) d'un O(N^2), le rapport
	//    temps/compteur si.
	enum class EProfCounter : uint8_t
	{
		Entities = 0,      // entites vivantes
		XformNodes,        // noeuds visites par PropagateWorld
		TriggerPairs,      // paires REELLEMENT testees (apres filtre de role)
		TriggerHits,       // paires en recouvrement
		MeshesTested,      // meshes classifies par le frustum
		MeshesCulled,      // ... rejetes Outside
		FacesSubmitted,    // faces entrant dans le pipeline
		TrisRasterized,    // triangles front-facing (apres backface), AVANT le rejet 3b.
							// Sens garde depuis la Reference F : memes triangles consideres.
		VertsTransformed,  // appels MulRow (chantier 2). Ajoute EN FIN : les colonnes
							// des CSV de reference versionnes gardent leur position.
		// ── Couverture (LV3_RASTER_STATS). Nuls si le commutateur vaut 0.
		//    Invariant : TrisCov0 + TrisCov1 + TrisCov2to4 + TrisCov5plus == TrisRasterized
		TrisCov0,          // triangles ne couvrant AUCUN pixel
		TrisCov1,
		TrisCov2to4,
		TrisCov5plus,
		TrisTightEmpty,    // boite serree vide : rejetable en O(1) (sous-ensemble de TrisCov0)
		PixelsTested,      // tours de la boucle pixel ACTUELLE (boite lache)
		PixelsTight,       // aire de la boite serree ceil(min-1/2)..floor(max-1/2)
		PixelsCovered,     // pixels passant les 3 fonctions d'arete (AVANT le Z)
		TrisEarlyRejected, // chantier 3b : boite serree vide, jamais envoyes au rasterizer.
							// Invariant : Σ TrisCov* + TrisEarlyRejected == TrisRasterized

							// ── Taille apparente (LV3_LOD_STATS). Nuls si le commutateur vaut 0.
							//    Par paire (instance, vue) NON rejetee par le frustum.
							//    Invariants : Σ InstPx*  == MeshesTested - MeshesCulled
							//                 Σ FacesPx* == FacesSubmitted
							//    ORDRE CONTIGU OBLIGATOIRE : RenderSystem indexe par (base + tranche).
		FacesInside,       // chantier 2b : faces des meshes Inside (chemin par sommet).
							// eta = FacesInside / FacesSubmitted : le champ d'action du 2b.

		InstPx0to1, InstPx1to4, InstPx4to16, InstPx16plus,
		FacesPx0to1, FacesPx1to4, FacesPx4to16, FacesPx16plus,
		VertsInside,       // sommets transformes pour les meshes Inside : champ du 2b
		TrisEmittedInside, // triangles emis par les meshes Inside : 3 divisions chacun

		// ── LOD (A13 bis) : niveau CHOISI par paire (instance, vue) non cullee.
		//    Actif des que LV3_PROFILE vaut 1 : c'est le resultat de la fonction, pas un diagnostic.
		//    Invariant : Σ LodLevel* == MeshesTested - MeshesCulled
		//    ORDRE CONTIGU OBLIGATOIRE : RenderSystem indexe par (LodLevel0 + niveau).
		LodLevel0, LodLevel1, LodLevel2, LodLevel3,
		Count
	};

	static_assert(uint8_t(EProfCounter::InstPx16plus) == uint8_t(EProfCounter::InstPx0to1) + 3
		&& uint8_t(EProfCounter::FacesPx16plus) == uint8_t(EProfCounter::FacesPx0to1) + 3,
		"RenderSystem indexe les tranches par base + b : ordre contigu obligatoire");

	inline constexpr size_t kProfZoneCount = static_cast<size_t>(EProfZone::Count);
	// Zones PRINCIPALES : [0, kProfMainZoneCount). Seules elles entrent dans RESTE.
	inline constexpr size_t kProfMainZoneCount = static_cast<size_t>(EProfZone::ClearColor);
	static_assert(kProfMainZoneCount == static_cast<size_t>(EProfZone::Present) + 1, "Les sous-zones doivent suivre Present : une zone principale ajoutee apres elles sortirait de RESTE");	inline constexpr size_t kProfCounterCount = static_cast<size_t>(EProfCounter::Count);

	// Tableaux PARALLELES aux enums. Un ajout dans l'enum sans ajout ici
	// est une erreur de compilation, pas un decalage silencieux des colonnes
	// du CSV -- le genre de bug qu'on ne voit qu'en relisant les chiffres
	// trois semaines plus tard, quand il est trop tard.
	inline constexpr const char* kProfZoneNames[] = {
		"Input", "Animation", "LocalXform1", "WorldXform1", "Cameras",
		"LocalXform2", "WorldXform2", "Trigger", "BuildViews",
		"Clear", "Render", "Present",
		"ClearColor", "ClearDepth"
	};

	inline constexpr const char* kProfCounterNames[] = {
		"Entities", "XformNodes", "TriggerPairs", "TriggerHits",
		"MeshesTested", "MeshesCulled", "FacesSubmitted", "TrisRasterized",
		"VertsTransformed",
		"TrisCov0", "TrisCov1", "TrisCov2to4", "TrisCov5plus", "TrisTightEmpty",
		"PixelsTested", "PixelsTight", "PixelsCovered",
		"TrisEarlyRejected",
		"FacesInside",
		"InstPx0to1", "InstPx1to4", "InstPx4to16", "InstPx16plus",
		"FacesPx0to1", "FacesPx1to4", "FacesPx4to16", "FacesPx16plus",
		"VertsInside", "TrisEmittedInside",
		"LodLevel0", "LodLevel1", "LodLevel2", "LodLevel3"
	};

	static_assert(std::size(kProfZoneNames) == kProfZoneCount, "kProfZoneNames doit suivre EProfZone, une entree par zone");
	static_assert(std::size(kProfCounterNames) == kProfCounterCount, "kProfCounterNames doit suivre EProfCounter, une entree par compteur");

	// ── Une frame = un enregistrement plat, trivialement copiable.
	//    Pas de std::string, pas de pointeur : un memset le remet a zero.
	struct ProfFrame
	{
		uint64_t ns[kProfZoneCount]{};
		uint64_t counters[kProfCounterCount]{};
		uint64_t frameNs = 0;
		uint64_t simTimeMilliDays = 0;   // entier : voir la note "locale" du .cpp
		uint32_t index = 0;
		uint32_t warmup = 0;             // 1 = frame de chauffe, exclue du resume
	};

	// Metadonnees de la campagne. Un chiffre sans son contexte ne vaut rien
	// dans six mois : la scene, la configuration et la resolution sont
	// ecrites DANS le fichier, pas dans le nom du fichier.
	struct ProfRunInfo
	{
		std::string scene;
		std::string config;              // "Release", "RelWithAsserts"
		int width = 0, height = 0;
		int views = 0;
		std::string renderDriver = "inconnu"; // pilote SDL : il decide de la memoire du FrameBuffer
		//std::string ablation = "none";   // M2 : un run d'ablation se declare LUI-MEME
	};

	class Profiler
	{
	public:
		// Arme le harnais. Reserve expectedFrames enregistrements : c'est
		// LA seule allocation du harnais, faite AVANT la premiere mesure.
		static void Begin(size_t expectedFrames, uint32_t warmupFrames = 60);

		static void BeginFrame(uint64_t simTimeMilliDays);
		static void EndFrame();

		static void AddNs(EProfZone z, uint64_t ns) noexcept;
		static void Add(EProfCounter c, uint64_t n = 1) noexcept;

		// Ecrit <path> (une ligne par frame) et <path sans extension>_summary.csv
		// (mediane / p95 / max par zone). La serie brute est la preuve, le
		// resume est la reponse : on versionne les deux.
		static bool DumpCsv(const std::string& path, const ProfRunInfo& info);

		static uint64_t OverheadNs() noexcept;   // cout mesure d'une paire de bornes
		static size_t   FrameCount() noexcept;
		static bool     IsFull() noexcept;       // budget de frames epuise

		// Valeur de LV3_PROFILE telle que vue par la LIB a SA compilation.
		// L'EXE compare avec la sienne : si les deux different, le commutateur
		// n'est pas pose dans le foyer unique (LV3.Common.props) et une partie
		// du programme mesure pendant que l'autre croit ne pas mesurer.
		// Meme nature que le bug 0.3 (LV3_ASSERTS_ENABLED).
		[[nodiscard]] static int CompiledProfileFlag() noexcept;
	};

	// RAII. Non copiable, non deplacable : une borne se ferme exactement
	// une fois, a la fermeture de son bloc.
	class ScopeTimer
	{
	public:
		explicit ScopeTimer(EProfZone z) noexcept
			: m_t0(std::chrono::steady_clock::now()), m_zone(z) {
		}

		~ScopeTimer() noexcept
		{
			const auto t1 = std::chrono::steady_clock::now();
			Profiler::AddNs(m_zone, static_cast<uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - m_t0).count()));
		}

		ScopeTimer(const ScopeTimer&) = delete;
		ScopeTimer& operator=(const ScopeTimer&) = delete;

	private:
		std::chrono::steady_clock::time_point m_t0;
		EProfZone m_zone;
	};

} // namespace LV3

// ── Les macros. Hors namespace, comme LV3_ASSERT.
//    ATTENTION : ne JAMAIS mettre d'expression a effet de bord dans
//    LV3_PROF_COUNT -- elle disparait quand LV3_PROFILE vaut 0.
#if LV3_PROFILE
	#define LV3_PROF_CAT2(a, b) a##b
	#define LV3_PROF_CAT(a, b)  LV3_PROF_CAT2(a, b)

	#define LV3_PROF_SCOPE(z)        ::LV3::ScopeTimer LV3_PROF_CAT(lv3Prof_, __LINE__){ (z) }
	#define LV3_PROF_COUNT(c, n)     ::LV3::Profiler::Add((c), (n))
	#define LV3_PROF_BEGIN_FRAME(t)  ::LV3::Profiler::BeginFrame(t)
	#define LV3_PROF_END_FRAME()     ::LV3::Profiler::EndFrame()

	// Bornes MANUELLES, pour une section qui DECLARE des variables utilisees
	// plus bas : un bloc RAII les enfermerait dans sa portee. Forme d'exception,
	// jamais le defaut -- une borne ouverte et jamais fermee ne mesure rien.
	// Filet : sans le LV3_PROF_END correspondant, lv3ProfT_<tag> n'est jamais
	// reference et /W4 le signale (C4189). L'oubli ne passe pas inapercu.
	#define LV3_PROF_BEGIN(tag)      const auto lv3ProfT_##tag = std::chrono::steady_clock::now()
	#define LV3_PROF_END(tag, z)     ::LV3::Profiler::AddNs((z), static_cast<uint64_t>( \
			std::chrono::duration_cast<std::chrono::nanoseconds>(                            \
				std::chrono::steady_clock::now() - lv3ProfT_##tag).count()))
#else
	// Meme idiome que LV3_ASSERT desactive : l'expression est COMPILEE
	// (donc verifiee, et les variables comptent comme utilisees -- pas de
	// C4189 sous Level4) mais jamais EVALUEE.
	#define LV3_PROF_SCOPE(z)        ((void)sizeof(z))
	#define LV3_PROF_COUNT(c, n)     ((void)sizeof(c), (void)sizeof(n))
	#define LV3_PROF_BEGIN_FRAME(t)  ((void)sizeof(t))
	#define LV3_PROF_END_FRAME()     ((void)0)
	#define LV3_PROF_BEGIN(tag)      ((void)0)
	#define LV3_PROF_END(tag, z)     ((void)sizeof(z))
#endif