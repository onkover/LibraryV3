#pragma once

namespace LibV3
{

    /// <summary>
    /// Options pour le chargement de fichiers OBJ.
    /// </summary>
    struct OBJLoadOptions
    {
        bool  generateNormalsIfMissing = true;
        bool  generateSmoothNormals = false;
        bool  flipWindingOrder = false;
        bool  flipUVsVertically = true;
        float scale = 1.0f;
    };

}