#include "pch.h"
#include "ContentRoot.h"
#include "Platform.h"      // SEUL point d'entree de windows.h pour ce service
#include "Logger.h"

namespace LV3
{
    // ────────────────────────────────────────────────────────────
    //  Helpers internes ('static' : invisibles hors de ce fichier)
    // ────────────────────────────────────────────────────────────

    // Une racine est valide si, et seulement si, elle contient le marqueur.
    // On ne teste JAMAIS l'existence du dossier seul : un dossier vide au
    // bon endroit ferait echouer les replis suivants sans rien resoudre.
    static bool IsContentRoot(const std::filesystem::path& dir)
    {
        if (dir.empty()) return false;
        std::error_code ec;
        return std::filesystem::exists(dir / kContentMarker, ec) && !ec;
    }

    // Lecture d'une variable d'environnement en LARGE.
    // Pas std::getenv : narrow (donc casse sur un chemin accentue hors
    // page de code active) et signale par C4996 sous /W4.
    static std::wstring ReadEnvW(const wchar_t* name)
    {
        const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
        if (needed == 0) return {};              // absente ou erreur

        std::wstring value(needed, L'\0');
        const DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
        if (written == 0 || written >= needed) return {};

        value.resize(written);                   // written EXCLUT le zero final
        return value;
    }

    // ────────────────────────────────────────────────────────────
    std::filesystem::path ExecutableDir()
    {
        // hModule = nullptr -> le module de l'EXE du processus.
        // /!\ Si LibraryV3 devient une DLL, ceci renverra toujours l'EXE hote :
        //     il faudrait GetModuleHandleExW + GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS.
        //     Aujourd'hui c'est un .lib statique, nullptr est correct.
        std::wstring buf(MAX_PATH, L'\0');

        for (;;)
        {
            const DWORD n = GetModuleFileNameW(nullptr, buf.data(),
                static_cast<DWORD>(buf.size()));
            if (n == 0)
            {
                Logger::error("ExecutableDir : GetModuleFileNameW a echoue, code="
                    + std::to_string(GetLastError()));
                return {};
            }

            if (n < buf.size()) { buf.resize(n); break; }   // succes : n exclut le zero final

            if (buf.size() >= 32768)                        // borne dure de l'API large
            {
                Logger::error("ExecutableDir : chemin superieur a 32767 caracteres");
                return {};
            }
            buf.resize(buf.size() * 2);                     // tronque : on double, on recommence
        }

        std::error_code ec;
        const std::filesystem::path full{ buf };
        const std::filesystem::path canon = std::filesystem::weakly_canonical(full, ec);
        return ec ? full.parent_path() : canon.parent_path();
    }

    // ────────────────────────────────────────────────────────────
    //  Trois candidats, dans un ordre qui n'est pas negociable :
    //    1. surcharge explicite  -> l'operateur a toujours le dernier mot
    //    2. a cote de l'exe      -> le seul cas qui marche sur une machine
    //                               qui n'a jamais vu le code source
    //    3. arborescence projet  -> confort du developpeur, Debug uniquement
    // ────────────────────────────────────────────────────────────
    std::filesystem::path ResolveContentRoot(std::span<const std::filesystem::path> extraCandidates)
    {
        std::vector<std::filesystem::path> tried;   // pour un diagnostic utilisable

        // --- 1. Surcharge explicite -------------------------------------
        //     Vaut dans TOUTES les configurations : c'est ce qui permettra
        //     de lancer la meme binaire Release sur les trois scenes de la
        //     campagne de mesure sans recompiler.
        if (const std::wstring env = ReadEnvW(L"LV3_CONTENT_ROOT"); !env.empty())
        {
            const std::filesystem::path p{ env };
            if (IsContentRoot(p))
            {
                Logger::info("[Content] racine = LV3_CONTENT_ROOT : " + p.string());
                return p;
            }
            tried.push_back(p);
        }

        // --- 2. A cote de l'executable ----------------------------------
        if (const std::filesystem::path exeDir = ExecutableDir(); !exeDir.empty())
        {
            if (IsContentRoot(exeDir))
            {
                //Logger::info("[Content] racine = dossier de l'executable : " + exeDir.string());
                LV3_LOG_DEBUG("[Content] racine = dossier de l'executable : " + exeDir.string());
                return exeDir;
            }
            tried.push_back(exeDir);
        }

        // --- 3. Candidats fournis par l'application ---------------------
        //     Le moteur ne les interprete pas : il les teste dans l'ordre recu.
        for (const std::filesystem::path& p : extraCandidates)
        {
            if (IsContentRoot(p))
            {
                Logger::info("[Content] racine = candidat applicatif : " + p.string());
                return p;
            }
            tried.push_back(p);
        }

        // --- Echec : on dit CE QU'ON A ESSAYE ---------------------------
        //     Un "fichier introuvable" sans la liste des chemins testes
        //     coute une heure. Avec la liste, il coute trente secondes.
        Logger::error("[Content] aucune racine valide : '" + std::string(kContentMarker)
            + "' introuvable.");
        for (const auto& p : tried)
            Logger::error("[Content]   essaye : " + p.string());
        if (tried.empty())
            Logger::error("[Content]   aucun candidat : ni LV3_CONTENT_ROOT, "
                "ni dossier d'executable exploitable.");

        return {};
    }
}