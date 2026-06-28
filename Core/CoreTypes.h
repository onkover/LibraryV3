//#include <cstdint>

#pragma once
// ============================================================
//  Core/CoreTypes.h — Vocabulaire fondamental du moteur
//  Inclus via pch.h
//  Règle : multi-système + jamais modifié
// ============================================================

namespace LV3
{

	// Axe cartésien — Transform, Camera, Renderer, Physics
	enum class EAxis : uint8_t { X, Y, Z };

	// Espace de référence — Transform, Renderer
	enum class ESpace : uint8_t { Local, World };

	// Face géométrique — Rasterizer, Material
	enum class EFace : uint8_t { Front, Back, Both };

	// Ordre d'enroulement des triangles — OBJLoader, Rasterizer
	enum class EWindingOrder : uint8_t { CW, CCW };

	// État d'activation générique — tous les composants
	enum class EEnabled : uint8_t { No = 0, Yes = 1 };

} // namespace LV3