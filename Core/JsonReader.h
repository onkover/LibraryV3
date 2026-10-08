#pragma once
// ============================================================
// Core/JsonReader.h — HORS pch.h
// Lecteur JSON a cle tracee (R5) — generique.
// Vec3f est une exception assumee : type Core (Maths), multi-systeme,
// meme critere que CoreTypes.h (Lecon 01, §7.2). Tout type de DOMAINE
// (EProjectionType, ELightType...) reste hors de ce fichier.
// ============================================================

#include <string>
#include <string_view>
#include <set>
#include <optional>
#include <format>
#include <type_traits>
#include "Core/Logger.h"
#include "Maths/Vectorlib.h"
#include "../Ressources/json.hpp"

namespace LV3
{
    class JsonReader
    {
    public:
        // delegated = true : bloc ecrit null par l'auteur -> chaque cle absente est ANNONCEE (info), pas un souci.
        JsonReader(const nlohmann::json& j, std::string comp, std::string owner, bool delegated = false) noexcept
            : m_j(j), m_comp(std::move(comp)), m_owner(std::move(owner)), m_delegated(delegated) {
        }

        //template<typename T>
        //[[nodiscard]] T Read(const char* key, T def)
        //{
        //    m_seen.insert(key);
        //    if (!Has(key))
        //    {
        //        Logger::warn("[" + m_comp + "] cle absente '" + key + "' sur " + m_owner + ". Prise en compte de la clé par défaut : ");
        //        return def;
        //    }
        //    try
        //    {
        //        return m_j.value(key, def);
        //    }
        //    catch (const nlohmann::json::type_error&)
        //    {
        //        Logger::warn("[" + m_comp + "] '" + key + "' de type invalide sur " + m_owner + " — défaut utilisé");
        //        return def;
        //    }
        //}

                // ── REGLE (Onky, 2026-10-07) : trois formes d'ecriture, aucune muette ──
        //   valeur exacte -> prise, silence
        //   cle absente   -> SOUCI : warn + defaut pris
        //   null          -> delegation ecrite : info + valeur retenue
        //   type invalide -> SOUCI : warn + defaut pris

        template<typename T>
        [[nodiscard]] T Read(const char* key, T def)
        {
            m_seen.insert(key);

            const auto it = m_j.find(key);                 // R28 : find(), jamais operator[]
            if (it == m_j.end()) { Absent(key, ToLog(def));  return def; }
            if (it->is_null()) { AnnounceDefault(key, ToLog(def));        return def; }

            try
            {
                return it->get<T>();
            }
            catch (const nlohmann::json::type_error&)
            {
                WarnDefault(key, "de type invalide", ToLog(def));
                return def;
            }
        }

        // Surcharge d'une valeur MOTEUR (camera -> EngineConfig).
        // nullopt = "suivre le moteur" ; engineValue ne sert QU'AUX messages.
        template<typename T>
        [[nodiscard]] std::optional<T> TryRead(const char* key, const T& engineValue)
        {
            m_seen.insert(key);

            const std::string taken = ToLog(engineValue) + " (moteur)";
            const auto it = m_j.find(key);
            if (it == m_j.end()) { Absent(key, taken); return std::nullopt; }
            if (it->is_null()) { AnnounceDefault(key, taken);        return std::nullopt; }

            try
            {
                return it->get<T>();
            }
            catch (const nlohmann::json::type_error&)
            {
                WarnDefault(key, "de type invalide", taken);
                return std::nullopt;
            }
        }

        //── Trois verbes, trois contrats. C'est le SITE D'APPEL qui sait. ──
         //  Read    : cle REQUISE.     Absente -> avertissement + def.
         //  ReadOr  : cle OPTIONNELLE. Absente -> silence + def (choix de l'auteur).
         //  TryRead : cle OPTIONNELLE sans defaut. Absente -> nullopt
         //            (surcharges : "absente" != toute valeur, pas de sentinelle).
         //  Pour les trois : type invalide -> avertissement. Une erreur d'auteur
         //  n'est JAMAIS muette. Les trois marquent la cle comme vue (WarnUnread).

        // Lit un tableau JSON de 3 nombres et retourne un Vec3f. Si la clé est absente ou invalide, retourne la valeur par défaut.
        //[[nodiscard]] Vec3f ReadVector(const char* key, const Vec3f& def)
        //{
        //    m_seen.insert(key);

        //    // R28 (variante nlohmann) : operator[] const EXIGE la clé (assert/UB si absente),
        //    // operator[] non-const l'INSÈRE. Ni l'un ni l'autre n'est un lookup : find().
        //    const auto it = m_j.find(key);
        //    if (it == m_j.end()) return def;

        //    const nlohmann::json& a = *it;
        //    if (!a.is_array() || a.size() < 3) return def;
        //    if (!a[0].is_number() || !a[1].is_number() || !a[2].is_number()) return def;
        //    return Vec3f(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());

        //}
        
        // Tableau de EXACTEMENT 3 nombres. Meme regle que Read.
        [[nodiscard]] Vec3f ReadVector(const char* key, const Vec3f & def)
        {
            m_seen.insert(key);

            const auto it = m_j.find(key);
            if (it == m_j.end()) { Absent(key, ToLog(def));  return def; }
            if (it->is_null()) { AnnounceDefault(key, ToLog(def));        return def; }

            const nlohmann::json& a = *it;
            if (!a.is_array() || a.size() != 3
                || !a[0].is_number() || !a[1].is_number() || !a[2].is_number())
            {
                WarnDefault(key, "mal formee (attendu : 3 nombres)", ToLog(def));
                return def;
            }
            return Vec3f(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());
        }

        // DELEGATION : la cle est consommee ICI, son contenu sera lu par un AUTRE
        // parseur (qui ouvrira son propre JsonReader). 'meaning' dit, pour les
        // messages, ce que vaut l'absence ("racine", "aucun composant").
        //   absente -> souci + nullptr   null -> info + nullptr   sinon -> &contenu
        [[nodiscard]] const nlohmann::json* Delegate(const char* key, std::string_view meaning)
        {
            m_seen.insert(key);

            const auto it = m_j.find(key);
            if (it == m_j.end()) { Absent(key, std::string(meaning));  return nullptr; }
            if (it->is_null()) { AnnounceDefault(key, std::string(meaning));        return nullptr; }
            return &*it;
        }


        //// Descente dans un sous-objet. Accepte un objet ou un tableau
        //// NON const : elle consomme une cle.        
        //[[nodiscard]] JsonReader Child(const char* key)
        //{
        //    m_seen.insert(key);
        //    static const nlohmann::json s_empty = nlohmann::json::object();

        //    const auto it = m_j.find(key);
        //    if (it != m_j.end() && !it->is_object() && !it->is_array())
        //        Logger::warn("[" + m_comp + "] '" + key + "' n'est ni un objet ni un tableau sur " + m_owner + " — bloc ignoré");

        //    const bool ok = (it != m_j.end()) && (it->is_object() || it->is_array());
        //    return JsonReader(ok ? *it : s_empty, m_comp + "." + key, m_owner);
        //}
                // Meme regle que pour une cle :
        //   absent       -> souci (bloc) ; chaque cle lue dedans : souci + defaut
        //   null         -> info  (bloc) ; chaque cle lue dedans : ANNONCEE (lecteur delegue)
        //   mauvais type -> souci (bloc) ; comme absent
        //   La delegation se transmet aux sous-blocs.
        [[nodiscard]] JsonReader Child(const char* key)
        {
            m_seen.insert(key);
            static const nlohmann::json s_empty = nlohmann::json::object();
            const std::string comp = m_comp + "." + key;

            const auto it = m_j.find(key);
            if (it == m_j.end())
            {
                Absent(key, "bloc entier par defaut");
                return JsonReader(s_empty, comp, m_owner, m_delegated);
            }
            if (it->is_null())
            {
                AnnounceDefault(key, "bloc entier par defaut");
                return JsonReader(s_empty, comp, m_owner, true);
            }
            if (!it->is_object() && !it->is_array())
            {
                WarnDefault(key, "ni objet ni tableau", "bloc entier par defaut");
                return JsonReader(s_empty, comp, m_owner, m_delegated);
            }
            return JsonReader(*it, comp, m_owner, m_delegated);
        }

        // itérer les enfants d'un OBJET et retourner un JsonReader par enfant
        template<typename Fn>
        void ForEachChild(Fn&& fn)
        {
            for (auto& [key, value] : m_j.items())
            {
                m_seen.insert(key);                 // itere = lu, quel que soit le nom
                if (!value.is_object())
                {
                    Logger::warn("[" + m_comp + "] '" + key + "' n'est pas un objet, ignore");
                    continue;
                }
                fn(key, JsonReader(value, m_comp + "." + key, m_owner));
            }
        }

        // itérer les enfants d'un TABLEAU et retourner un JsonReader par enfant
        template<typename Fn>
        void ForEachElement(Fn&& fn)
        {
            //if (!m_j.is_object())
            //{
            //    Logger::warn("[" + m_comp + "] attendu comme objet sur " + m_owner);
            //    return;
            //}

            if (!m_j.is_array())
            {
                Logger::warn("[" + m_comp + "] attendu comme tableau sur " + m_owner);
                return;
            }
            std::size_t i = 0;
            for (const auto& value : m_j)
            {
                if (!value.is_object())
                {
                    Logger::warn("[" + m_comp + "][" + std::to_string(i) + "] n'est pas un objet, ignore");
                    ++i;
                    continue;
                }
                fn(i, JsonReader(value, m_comp + "[" + std::to_string(i) + "]", m_owner));
                ++i;
            }
        }


        [[nodiscard]] bool Has(std::string_view key) const { return m_j.contains(key); }

        // A appeler en DERNIER : toute cle jamais passee par Read()/Child() est inconnue.
        void WarnUnread() const
        {
            if (!m_j.is_object()) return;   // les tableaux passent par ForEachElement, pas par ici

            for (auto& [key, _] : m_j.items())
            {
                if (key.starts_with('_')) continue;   // "_comment", "_version"... : assume
                if (!m_seen.contains(key))
                    Logger::warn("[" + m_comp + "] cle ignoree '" + key + "' sur " + m_owner);
            }
        }

    private:
        // Cle absente : SOUCI, sauf dans un bloc delegue (null) ou l'auteur a ecrit "tout par defaut".
        void Absent(const char* key, const std::string& taken) const
        {
            if (m_delegated)
                Logger::info("[" + m_comp + "] cle '" + key + "' (bloc null) sur " + m_owner + " — valeur retenue : " + taken);
            else
                WarnDefault(key, "absente", taken);
        }

        // SOUCI : compte dans Logger::warnCount().
        void WarnDefault(const char* key, std::string_view why, const std::string& taken) const
        {
            Logger::warn("[" + m_comp + "] cle '" + key + "' " + std::string(why)
                + " sur " + m_owner + " — defaut pris : " + taken);
        }

        // ANNONCE : l'auteur a ecrit null. Visible, mais pas un souci.
        void AnnounceDefault(const char* key, const std::string& taken) const
        {
            Logger::info("[" + m_comp + "] cle '" + key + "' = null sur " + m_owner
                + " — valeur retenue : " + taken);
        }

        template<typename T>
        [[nodiscard]] static std::string ToLog(const T& v)
        {
            if constexpr (std::is_same_v<T, std::string>) return "\"" + v + "\"";
            else if constexpr (std::is_same_v<T, Vec3f>)  return std::format("({}, {}, {})", v.x, v.y, v.z);
            else                                          return std::format("{}", v);
        }

        const nlohmann::json& m_j;
        std::string          m_comp, m_owner;
        std::set<std::string, std::less<>> m_seen;
        bool m_delegated = false;   // bloc null : absences annoncees, pas signalees
    };

} // namespace LV3