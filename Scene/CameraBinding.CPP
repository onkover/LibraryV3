#include "pch.h"          // ? première ligne, toujours
#include "CameraBinding.hpp"


namespace LV3
{
    // Decoupe un rectangle. Aucune notion de camera.
    [[nodiscard]] size_t BuildLayout(ELayout layout, int w, int h, Viewport* out, size_t capacity)
    {
        LV3_ASSERT(out && w > 1 && h > 1);
        const int hw = w / 2, hh = h / 2;

        switch (layout)
        {
        case ELayout::Single:
            LV3_ASSERT(capacity >= 1);
            out[0] = { 0, 0, w, h };
            return 1;

        case ELayout::SplitH:                       // cote a cote
            LV3_ASSERT(capacity >= 2);
            out[0] = { 0,  0, hw,     h };
            out[1] = { hw, 0, w - hw, h };
            return 2;

        case ELayout::SplitV:                       // l'un au-dessus de l'autre
            LV3_ASSERT(capacity >= 2);
            out[0] = { 0, 0,  w, hh };
            out[1] = { 0, hh, w, h - hh };
            return 2;
        
        case ELayout::MainSide:                     // 75% principal, 25% lateral
            LV3_ASSERT(capacity >= 2);
            {
                const int mainW = (w * 3) / 4;
                out[0] = { 0,     0, mainW,     h };
                out[1] = { mainW, 0, w - mainW, h };
            }
            return 2;

        case ELayout::Quad:
            LV3_ASSERT(capacity >= 4);
            out[0] = { 0,  0,  hw,     hh };
            out[1] = { hw, 0,  w - hw, hh };
            out[2] = { 0,  hh, hw,     h - hh };
            out[3] = { hw, hh, w - hw, h - hh };
            return 4;
        }
        return 0;   // enum hors domaine
    }


    //[[nodiscard]] size_t BuildCameraBindings(Entity activeCamera, Entity observerCamera,
    //    int fbWidth, int fbHeight,
    //    CameraBinding* out, size_t capacity)
    //{
    //    LV3_ASSERT(out && capacity >= 2);
    //    LV3_ASSERT(activeCamera != NULL_ENTITY);
    //    LV3_ASSERT(observerCamera != NULL_ENTITY);
    //    LV3_ASSERT(activeCamera != observerCamera);   // sinon un gizmo masque des DEUX cotes

    //    const int half = fbWidth / 2;

    //    out[0] = { activeCamera,   Viewport{ 0,    0, half,           fbHeight }, ERenderMode::Solid };
    //    out[1] = { observerCamera, Viewport{ half, 0, fbWidth - half, fbHeight }, ERenderMode::Wireframe };

    //    return 2;
    //}

    //size_t BuildCameraBindings(ELayout layout, const ViewSlot* slots, size_t slotCount,
    //    int w, int h, CameraBinding* out, size_t capacity)
    //{
    //    Viewport vps[8];
    //    const size_t n = BuildLayout(layout, w, h, vps, std::size(vps));

    //    // Le layout et la liste de cameras doivent s'accorder. Sinon on l'apprend ICI,
    //    // pas trois systemes plus loin sur un NULL_ENTITY.
    //    LV3_ASSERT(n == slotCount && n <= capacity);

    //    for (size_t i = 0; i < n; ++i)
    //    {
    //        LV3_ASSERT(slots[i].m_camera != NULL_ENTITY);
    //        out[i] = { slots[i].m_camera, vps[i], slots[i].m_mode };
    //    }
    //    return n;
    //}

    size_t BuildCameraBindings(ELayout layout, const ViewSlot* slots, size_t slotCount,
        int w, int h, CameraBinding* out, size_t capacity)
    {
        Viewport vps[8];
        const size_t n = BuildLayout(layout, w, h, vps, std::size(vps));

        LV3_ASSERT(n == slotCount && n <= capacity);

        for (size_t i = 0; i < n; ++i)
        {
            LV3_ASSERT(slots[i].m_camera != NULL_ENTITY);
            out[i] = { slots[i].m_camera, vps[i], slots[i].m_mode };
        }

        // Aucune camera deux fois : son gizmo porte m_hideForCamera et serait
        // masque dans TOUTES les vues ou elle apparait, donc invisible partout.
        #ifdef _DEBUG
            for (size_t i = 0; i < n; ++i)
                for (size_t j = i + 1; j < n; ++j)
                    LV3_ASSERT(out[i].m_camera != out[j].m_camera);
        #endif

        return n;
    }
}