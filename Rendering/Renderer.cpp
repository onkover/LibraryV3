#include "pch.h"
#include "Renderer.h"
#include "Rasterizer.h"
#include "Fragment.h"

namespace LV3
{

    void Renderer::DrawTriangle(const Triangle2D& tri, FrameBuffer& fb, ERenderMode mode, Color c)
    {
        switch (mode)
        {
        case ERenderMode::Solid:
        {
//            UnlitContext ctx{ &fb, Color{60, 60, 255, 255} };
            UnlitContext ctx{ &fb, c };

            //RasterizeTriangle(tri.v0, tri.v2, tri.v2,
            //    fb.Width(), fb.Height(),
            //    ShadeFragment_Unlit,   // ← adresse de fonction n°1
            //    &ctx);

            RasterizeTriangle(tri.v0, tri.v1, tri.v2,
                fb.Width(), fb.Height(),
                ShadeFragment_Unlit,   // ← adresse de fonction n°1
                &ctx);
            break;
        }

        case ERenderMode::Depth:
        {
            DepthContext ctx{ &fb, tri.z0, tri.z1, tri.z2 };

            RasterizeTriangle(tri.v0, tri.v1, tri.v2,
                fb.Width(), fb.Height(),
                ShadeFragment_Depth,   // ← adresse de fonction n°2
                &ctx);
            break;
        }
        case ERenderMode::TestBarycentric:
        {
            DepthContext ctx{ &fb, tri.z0, tri.z1, tri.z2 };

            RasterizeTriangle(tri.v0, tri.v1, tri.v2,
                fb.Width(), fb.Height(),
                ShadeFragment_Barycentric,   // ← adresse de fonction n°2
                &ctx);
            break;
        }
        default:
            break; // Wireframe/Normals/UV — pas encore implémentés, on ne silence pas ça longtemps
        }
    }

} // namespace LV3