#pragma once
#include <filesystem>
#include <span>

namespace LV3
{
    inline constexpr const char* kContentMarker = "engine.json";

    // Dossier de l'executable. Pure abstraction OS : sa place est ici.
    [[nodiscard]] std::filesystem::path ExecutableDir();

    // Racine du contenu, cherchee dans cet ordre :
    //   1. LV3_CONTENT_ROOT (environnement)  -> l'operateur a le dernier mot
    //   2. dossier de l'executable           -> le cas livre
    //   3. extraCandidates, dans l'ordre     -> fournis par l'APPLICATION
    //
    // Le moteur ignore ce que sont ces candidats : un arbre de sources, un
    // partage reseau, un dossier de test. Il ne connait que le marqueur.
    [[nodiscard]] std::filesystem::path ResolveContentRoot(
        std::span<const std::filesystem::path> extraCandidates = {});
}