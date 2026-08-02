#include "pch.h"
#include "Projection.h"

namespace LV3::Projection
{
    namespace {
        // Une matrice de projection perspective n'est PAS l'identité modifiée :
        // il faut partir de zéro, sinon m[3][3] reste à 1 et la division par w
        // ne se fait jamais.
        Matrix44f Zeroed() noexcept
        {
            Matrix44f m;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) m[i][j] = 0.0f;
            return m;
        }
    }

    // ------------------------------------------------------------
    //  Perspective décentrée — main droite, reverse-Z [0,1]
    //
    //  clip.z = z·m22 + m32        clip.w = -z
    //  z = -near -> clip.z/clip.w = 1
    //  z = -far  -> clip.z/clip.w = 0
    // ------------------------------------------------------------
    Matrix44f PerspectiveOffCenter(float l, float r, float b, float t,
        float n, float f) noexcept
    {
        Matrix44f m = Zeroed();

        m[0][0] = (2.0f * n) / (r - l);
        m[1][1] = (2.0f * n) / (t - b);

        m[2][0] = (r + l) / (r - l);        // décentrement horizontal
        m[2][1] = (t + b) / (t - b);        // décentrement vertical
        m[2][2] = n / (f - n);              // REVERSE-Z
        m[2][3] = -1.0f;                     // recopie -z dans w (main droite)

        m[3][2] = (n * f) / (f - n);        // REVERSE-Z
        m[3][3] = 0.0f;                     // matrice projective
        return m;
    }

    Matrix44f Perspective(float fovYRad, float aspect, float n, float f) noexcept
    {
        const float t = std::tan(fovYRad * 0.5f) * n;   // demi-hauteur au near
        const float r = t * aspect;                     // demi-largeur au near
        return PerspectiveOffCenter(-r, r, -t, t, n, f);
    }

    Matrix44f PerspectiveInfinite(float fovYRad, float aspect, float n) noexcept
    {
        const float th = std::tan(fovYRad * 0.5f);

        Matrix44f m = Zeroed();
        m[0][0] = 1.0f / (aspect * th);
        m[1][1] = 1.0f / th;
        m[2][2] = 0.0f;                     // limite de n/(f-n) quand f -> +inf
        m[2][3] = -1.0f;
        m[3][2] = n;                        // z_ndc = n / distance
        m[3][3] = 0.0f;
        return m;
    }

    // ------------------------------------------------------------
    //  Modèle sténopé : tangente = (taille pellicule / 2) / focale.
    //  Le gate fit corrige la différence entre le ratio de la
    //  pellicule et celui de la fenêtre.
    // ------------------------------------------------------------
    Matrix44f PerspectiveFilmback(float focalMm,
        float filmWidthMm, float filmHeightMm,
        float deviceAspect,
        float n, float f,
        EGateFit fit) noexcept
    {
        const float filmAspect = filmWidthMm / filmHeightMm;

        float xscale = 1.0f, yscale = 1.0f;
        switch (fit)
        {
        case EGateFit::Fill:
            if (filmAspect > deviceAspect) xscale = deviceAspect / filmAspect;
            else                           yscale = filmAspect / deviceAspect;
            break;
        case EGateFit::Overscan:
            if (filmAspect > deviceAspect) yscale = filmAspect / deviceAspect;
            else                           xscale = deviceAspect / filmAspect;
            break;
        }

        const float r = ((filmWidthMm * 0.5f) / focalMm) * xscale * n;
        const float t = ((filmHeightMm * 0.5f) / focalMm) * yscale * n;
        return PerspectiveOffCenter(-r, r, -t, t, n, f);
    }

    // ------------------------------------------------------------
    //  Orthographique — main droite, reverse-Z [0,1]
    //  Pas de division perspective : m23 = 0, m33 = 1.
    // ------------------------------------------------------------
    Matrix44f Orthographic(float l, float r, float b, float t,
        float n, float f) noexcept
    {
        Matrix44f m;                          // identité

        m[0][0] = 2.0f / (r - l);
        m[1][1] = 2.0f / (t - b);
        m[2][2] = 1.0f / (f - n);            // REVERSE-Z

        m[3][0] = -(r + l) / (r - l);
        m[3][1] = -(t + b) / (t - b);
        m[3][2] = f / (f - n);               // REVERSE-Z
        return m;
    }

    Matrix44f OrthographicCentered(float height, float aspect, float n, float f) noexcept
    {
        const float halfH = height * 0.5f;
        const float halfW = halfH * aspect;
        return Orthographic(-halfW, halfW, -halfH, halfH, n, f);
    }
}