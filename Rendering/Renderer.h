#pragma once
//#include "Rasterizer.h"   // Vec2f
#include "../Maths/Vectorlib.h"
#include "FrameBuffer.h"  // FrameBuffer
#include "RenderTypes.h"  // ERenderMode (Leçon 1)
#include "Viewport.h"     // Viewport

namespace LV3
{

    // Triangle déjà transformé en espace écran (post-viewport).
    // Les z sont conservés pour l'interpolation de profondeur — ils seront
    // exploités pleinement au Z-buffer en Leçon 4 Partie 2.
    struct Triangle2D
    {
        Vec2f v0, v1, v2;
        float z0, z1, z2;
    };

    class Renderer
    {
    public:
        void DrawTriangle(const Triangle2D& tri, FrameBuffer& fb, const Viewport& vp, ERenderMode mode, Color c);
//        void DrawTriangle(const Triangle2D& tri, FrameBuffer& fb, ERenderMode mode, Color c);
    };

} // namespace LV3