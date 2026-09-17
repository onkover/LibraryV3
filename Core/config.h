#pragma once
/*
Fichier de configuration

*/
#include <cstdio>     // fprintf, fflush
#include <cstdlib>    // abort

namespace LV3
{
	// Déclaration de constats mathématiques pour éviter d'inclure <corecrt_math_defines.h>
	constexpr auto M_E = 2.71828182845904523536;   // e
	constexpr auto M_LOG2E = 1.44269504088896340736;   // log2(e)
	constexpr auto M_LOG10E = 0.434294481903251827651;  // log10(e)
	constexpr auto M_LN2 = 0.693147180559945309417;  // ln(2)
	constexpr auto M_LN10 = 2.30258509299404568402;   // ln(10)
	constexpr auto M_PI = 3.14159265358979323846;   // pi
	constexpr auto M_PI_2 = 1.57079632679489661923;   // pi/2
	constexpr auto M_PI_4 = 0.785398163397448309616;  // pi/4
	constexpr auto M_1_PI = 0.318309886183790671538;  // 1/pi
	constexpr auto M_2_PI = 0.636619772367581343076;  // 2/pi
	constexpr auto M_2_SQRTPI = 1.12837916709551257390;   // 2/sqrt(pi)
	constexpr auto M_SQRT2 = 1.41421356237309504880;   // sqrt(2)
	constexpr auto M_SQRT1_2 = 0.707106781186547524401;  // 1/sqrt(2)

	// constexpr double INV_PI = 0.31830988618379067153777;
	// constexpr double INV_2PI = 0.15915494309189533576888;
	constexpr float INV_PI = 0.31830988618379067153777f;
	constexpr float INV_2PI = 0.15915494309189533576888f;

	constexpr float EPSILON = 0.01f;    // tolérance géométrique grossière
	constexpr float EPSILON_FLOAT = 0.001f;   // tolérance géométrique fine
	constexpr double EPSILON_DOUBLE = 0.001;

	// Garde ANTI-DIVISION. Ce n'est PAS une tolérance : ne l'utilise
	// jamais pour comparer deux grandeurs, uniquement pour vérifier
	// qu'un dénominateur n'est pas nul.
	constexpr float NEAR_ZERO_SQ = 1e-30f;     // sur une longueur au carré
	constexpr float NEAR_ZERO = 1e-15f;     // sur une longueur

	constexpr float TO_RADIAN = (float) M_PI / 180.0f; //float
	constexpr float TO_DEGRE = 180.0f / (float)M_PI;	//float

	constexpr float inchToMm = 25.4f;

	//  ************************************************************************************************

	[[noreturn]] inline void AssertFailed(const char* expr, const char* file, int line)
	{
		std::fprintf(stderr, "\n[LV3_ASSERT] %s\n  %s:%d\n", expr, file, line);
		std::fflush(stderr);
		#ifdef _MSC_VER
			__debugbreak();     // s'arrete DANS le debogueur, au bon endroit
		#endif
		std::abort();
	}

}

// ── config.h ────────────────────────────────────────────────────
// QUATRE commutateurs INDEPENDANTS. Chacun sous #ifndef : une
// configuration du projet peut le forcer sans toucher a ce fichier.
// Les valeurs par defaut derivent de _DEBUG, donc Debug et Release
// se comportent exactement comme avant.

// 1. Invariants permanents (CheckSceneInvariants, ValidateHierarchy...).
//    Le SEUL qu'on voudra activer en Release, pour mesurer sans filet coupe.
#ifndef LV3_ASSERTS_ENABLED
	#ifdef _DEBUG
		#define LV3_ASSERTS_ENABLED 1
	#else
		#define LV3_ASSERTS_ENABLED 0
	#endif
#endif

// 2. Code de developpement (RenderSystem console, dumps...).
#ifndef LV3_DEBUG
	#ifdef _DEBUG
		#define LV3_DEBUG 1
	#else
		#define LV3_DEBUG 0
	#endif
#endif

// 3. Journaux de diagnostic.
#ifndef LV3_DEBUG_LOG
	#ifdef _DEBUG
		#define LV3_DEBUG_LOG 1
	#else
		#define LV3_DEBUG_LOG 0
	#endif
#endif

// 4. Journaux verbeux (le maximum d'info a l'ecran).
#ifndef LV3_VERBOSE_LOG
	#ifdef _DEBUG
		#define LV3_VERBOSE_LOG 0
	#else
		#define LV3_VERBOSE_LOG 0
	#endif
#endif

// 5. Dump console de la hierarchie, PAR FRAME (DrawHierarchySystem).
//    Independant de LV3_DEBUG : sinon CHAQUE build Debug l'allume,
//    meme quand ce n'est pas ca qu'on debug. A activer ponctuellement.
#ifndef LV3_DUMP_HIERARCHY
	#define LV3_DUMP_HIERARCHY 1
#endif

#if LV3_DEBUG_LOG
	#define LV3_LOG_DEBUG(msg)   LV3::Logger::info(msg)
#else
	#define LV3_LOG_DEBUG(msg)   ((void)0)
#endif

#if LV3_VERBOSE_LOG
	#define LV3_LOG_VERBOSE(msg) LV3::Logger::info(msg)
#else
	#define LV3_LOG_VERBOSE(msg) ((void)0)
#endif



#if LV3_ASSERTS_ENABLED
	// __VA_ARGS__ et non (x) : une expression contenant une virgule
	// -- LV3_ASSERT(std::min(a,b) > 0) -- serait vue comme DEUX arguments.
	#define LV3_ASSERT(...) \
			do { if (!(__VA_ARGS__)) LV3::AssertFailed(#__VA_ARGS__, __FILE__, __LINE__); } while (0)
#else
	// sizeof : l'expression est COMPILEE (donc verifiee par le compilateur
	// et comptee comme un usage des variables) mais jamais EVALUEE.
	#define LV3_ASSERT(...) ((void)sizeof(!(__VA_ARGS__)))
#endif
