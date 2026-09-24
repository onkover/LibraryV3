#pragma once
#include <filesystem>
#include <span>

namespace LV3
{
    inline constexpr const char* kContentMarker = "engine.json";

    // Dossier de l'executable. Pure abstraction OS : sa place est ici.
    [[nodiscard]] std::filesystem::path ExecutableDir();

    // Une racine est valide si, et seulement si, elle contient le marqueur.
    // PUBLIE depuis le bug 68 : l'APPLICATION doit pouvoir verifier une
    // racine qu'un operateur a explicitement demandee, AVANT de la remettre
    // comme simple candidat. Un seul critere de validite dans tout le projet,
    // ici — jamais une copie du test dans main.cpp.
    [[nodiscard]] bool IsContentRoot(const std::filesystem::path& dir);

    // Racine du contenu, cherchee dans cet ordre (discussion D, 18/09/2026) :
    //   1. LV3_CONTENT_ROOT (environnement)  -> INTENTION : si posee et
    //      invalide, echec immediat, AUCUN repli (bug 68)
    //   2. extraCandidates, dans l'ordre     -> fournis par l'APPLICATION :
    //      la source de verite qu'on edite ou qu'on mesure, jamais une copie
    //   3. dossier de l'executable           -> dernier recours, le cas livre
    //
    // Le moteur ignore ce que sont les extraCandidats : un arbre de sources,
    // un partage reseau, un dossier de test. Il ne connait que le marqueur, et
    // il ne peut PAS savoir lesquels etaient un ordre et lesquels un defaut —
    // c'est a l'appelant de valider ceux qui sont des ordres (cf. IsContentRoot).
    [[nodiscard]] std::filesystem::path ResolveContentRoot(
        std::span<const std::filesystem::path> extraCandidates = {});
}