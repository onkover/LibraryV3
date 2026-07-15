#pragma once
/*
Fichier de configuration

Contient que les données "gravées dans le marbre" car il un changement de valeur recompilera le projet

*/

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

	constexpr float EPSILON = 0.01f;
	constexpr float EPSILON_FLOAT = 0.001f;
	constexpr double EPSILON_DOUBLE = 0.001;

	constexpr float TO_RADIAN = (float) M_PI / 180.0f; //float
	constexpr float TO_DEGRE = 180.0f / (float)M_PI;	//float

	constexpr float inchToMm = 25.4f;

	//  ************************************************************************************************
	// _DEBUG définie automatiquement lorsque vous configurez votre projet sur "Debug"
	// Cela permet d'activer des vérifications et des journaux supplémentaires pour le débogage.

	#ifdef _DEBUG
		#define LV3_DEBUG           1
		#define LV3_ASSERT(x)       assert(x)
		#define LV3_DEBUG_LOG       1
	#else												// mode  "Release"
		#define LV3_DEBUG           0
		#define LV3_ASSERT(x)       ((void)0)
		#define LV3_DEBUG_LOG       0
	#endif
	/*
	Utilisation avec un #if (le code ne sera même pas compilé en Release)
    #if LV3_DEBUG_LOG
        std::cout << "[DEBUG] Début du traitement des données.\n";
    #endif
	    
    // Utilisation avec un if classique (le compilateur optimisera et retirera le bloc en Release)
    if (LV3_DEBUG) 
	{
        // Faire une vérification coûteuse qui n'est utile qu'en développement
    }
	
	
	// On s'assure que b n'est pas égal à 0. 
    // Si b == 0 en mode Debug, le programme s'arrête net ici.
	#include <cassert> // Nécessaire pour que assert(x) fonctionne en Debug
    LV3_ASSERT(b != 0);
	*/
	//  ************************************************************************************************
}