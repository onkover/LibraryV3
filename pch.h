#pragma once
// ============================================================
//  pch.h — En-tête précompilé (C++23)
//  Tous les .cpp du projet LibraryV2 doivent commencer par :
//    #include "pch.h"
// ============================================================

// --- STL stable ---
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <map>
#include <functional>
#include <algorithm>
#include <numeric>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cassert>
#include <limits>
#include <type_traits>
#include <string_view>
#include <iostream>
#include <limits>
#include <version>

// --- C++20 ---
#include <span>
#include <ranges>
#include <format>

// --- C++23 ---
//#if __cplusplus >= 202302L
//	#include <expected>		// std::expected est utile pour retourner soit une valeur valide, soit une erreur, sans lever d'exception.
//#endif

#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
	#error "LibraryV3 exige std::expected. Verifie LanguageStandard=stdcpp23 dans CETTE configuration."
#endif
#include <expected>

// --- Core moteur (toujours présent) ---
#include "core/Compiler.h"			// LV3_FORCEINLINE
#include "core/config.h"				// configuration du moteur qui ne change jamais sinon, recompilation totale du projet
#include "Core/CoreTypes.h"		
#include "Maths/MathTypes.h"
#include "Maths/Transform.h"
#include "Core/InputState.h"

//#include <Resources/MathTypes.h>
//#include <Resources/ResourceHandle.h>
//#include <Resources/Material.h>
//#include <Resources/SubMesh.h>
//#include <Geometry/MeshClass.h>