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
#include "Core/Logger.h"
#include "Maths/Vectorlib.h"
#include "../Ressources/json.hpp"

namespace LV3
{
    class JsonReader
    {
    public:
        JsonReader(const nlohmann::json& j, std::string comp, std::string owner) noexcept
            : m_j(j), m_comp(std::move(comp)), m_owner(std::move(owner)) {}

        template<typename T>
        [[nodiscard]] T Read(const char* key, T def)
        {
            m_seen.insert(key);
            if (!Has(key))
            {
                Logger::warn("[" + m_comp + "] cle absente '" + key + "' sur " + m_owner + ". Prise en compte de la clé par défaut.");
                return def;
            }
            try
            {
                return m_j.value(key, def);
            }
            catch (const nlohmann::json::type_error&)
            {
                Logger::warn("[" + m_comp + "] '" + key + "' de type invalide sur " + m_owner + " — défaut utilisé");
                return def;
            }
        }


        // Lit un tableau JSON de 3 nombres et retourne un Vec3f. Si la clé est absente ou invalide, retourne la valeur par défaut.
        [[nodiscard]] Vec3f ReadVector(const char* key, const Vec3f& def)
        {
            m_seen.insert(key);

            // R28 (variante nlohmann) : operator[] const EXIGE la clé (assert/UB si absente),
            // operator[] non-const l'INSÈRE. Ni l'un ni l'autre n'est un lookup : find().
            const auto it = m_j.find(key);
            if (it == m_j.end()) return def;

            const nlohmann::json& a = *it;
            if (!a.is_array() || a.size() < 3) return def;
            if (!a[0].is_number() || !a[1].is_number() || !a[2].is_number()) return def;
            return Vec3f(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());

        }
        
        // Descente dans un sous-objet. Accepte un objet ou un tableau
        // NON const : elle consomme une cle.        
        [[nodiscard]] JsonReader Child(const char* key)
        {
            m_seen.insert(key);
            static const nlohmann::json s_empty = nlohmann::json::object();

            const auto it = m_j.find(key);
            if (it != m_j.end() && !it->is_object() && !it->is_array())
                Logger::warn("[" + m_comp + "] '" + key + "' n'est ni un objet ni un tableau sur " + m_owner + " — bloc ignoré");

            const bool ok = (it != m_j.end()) && (it->is_object() || it->is_array());
            return JsonReader(ok ? *it : s_empty, m_comp + "." + key, m_owner);
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
            if (!m_j.is_object())
            {
                Logger::warn("[" + m_comp + "] attendu comme objet sur " + m_owner);
                return;
            }

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
        const nlohmann::json& m_j;
        std::string                        m_comp, m_owner;
        std::set<std::string, std::less<>> m_seen;
    };

} // namespace LV3