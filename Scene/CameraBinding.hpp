#pragma once
#include "Entity.hpp"
#include "../Rendering/Viewport.h"
#include "../Rendering/RenderTypes.h"

namespace LV3
{

    class Registry;
    
    // viewport / découpage des layouts 
    enum class ELayout : uint8_t { Single, SplitH, SplitV, Quad, MainSide };

    struct ViewSlot
    {
        Entity      m_camera;
        ERenderMode m_mode;
    };

    // L'association (camera, viewport). AUCUNE matrice : construite
    // avant les transforms, donc disponible pour tout consommateur.
    struct CameraBinding
    {
        Entity   m_camera = NULL_ENTITY;
        Viewport m_viewport;
        ERenderMode m_mode = ERenderMode::Solid;
    };

    // LE point unique ou se decide quelle camera rend quel viewport.
    // Split a 4, minimap, picture-in-picture : cette fonction seule change.
    //[[nodiscard]] size_t BuildCameraBindings(Entity activeCamera, Entity observerCamera,
    //    int fbWidth, int fbHeight,
    //    CameraBinding* out, size_t capacity);
    size_t BuildCameraBindings(ELayout layout, const ViewSlot* slots, size_t slotCount, int w, int h, CameraBinding* out, size_t capacity);

    // Decoupe un rectangle. Aucune notion de camera.
    [[nodiscard]] size_t BuildLayout(ELayout layout, int w, int h, Viewport* out, size_t capacity);

}