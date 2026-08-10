#include "pch.h"
#include "Renderer.h"
#include "Rasterizer.h"
#include "Fragment.h"

namespace LV3
{

    void Renderer::DrawTriangle(const Triangle2D& tri, FrameBuffer& fb, const Viewport& vp, ERenderMode mode, Color c)
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
                vp,
                ShadeFragment_Unlit,   // ← adresse de fonction n°1
                &ctx);
            break;
        }

        case ERenderMode::Depth:
        {
            DepthContext ctx{ &fb, tri.z0, tri.z1, tri.z2 };

            RasterizeTriangle(tri.v0, tri.v1, tri.v2,
                vp,
                ShadeFragment_Depth,   // ← adresse de fonction n°2
                &ctx);
            break;
        }
        case ERenderMode::BarycentricColors:
        {
            DepthContext ctx{ &fb, tri.z0, tri.z1, tri.z2 };

            RasterizeTriangle(tri.v0, tri.v1, tri.v2,
                vp,
                ShadeFragment_Barycentric,   // ← adresse de fonction n°2
                &ctx);
            break;
        }
		//case ERenderMode::Wireframe:
  //      {
  //          const float area = EdgeFunction(r[0], r[1], r[2]);
  //          if (area <= 0.0f) break;                      // backface

  //          const Color w = MakeColor(255, 255, 255);
  //          for (uint8_t k = 0; k < vpf; ++k)
  //              DrawLineClipped(fb, vp, r[k], r[(k + 1) % vpf], w);
  //      }
        default:
            break; // Wireframe/Normals/UV — pas encore implémentés, on ne silence pas ça longtemps
        }
    }

} // namespace LV3