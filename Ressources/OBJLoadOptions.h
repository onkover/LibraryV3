#pragma once

namespace LV3
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

        // Suffixe de clé de cache : les 4 booléens + les 32 bits EXACTS du float.
        // Avec les options par défaut, la clé devient
        // "C:/…/rock_a.obj|n1s0w0u1x3F800000", où 3F800000 est l’écriture binaire exacte de 1.0f.
        [[nodiscard]] std::string CacheKey() const
        {
            return std::format("|n{}s{}w{}u{}x{:08X}",
                        int(generateNormalsIfMissing), 
                        int(generateSmoothNormals),
                        int(flipWindingOrder), 
                        int(flipUVsVertically), 
                        std::bit_cast<std::uint32_t>(scale));            
        }
    };

    // Garde-fou : un champ ajouté sans mettre CacheKey() à jour ne compile plus.
    static_assert(sizeof(OBJLoadOptions) == 8, "OBJLoadOptions a change : mettre CacheKey() a jour");
}