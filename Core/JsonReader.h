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
            return m_j.value(key, def);
        }

        // Lit un tableau JSON de 3 nombres et retourne un Vec3f. Si la clé est absente ou invalide, retourne la valeur par défaut.
        [[nodiscard]] Vec3f ReadVector(const char* key, const Vec3f& def)
        {
            m_seen.insert(key);

            const nlohmann::json& a = m_j[key];
            if (!a.is_array() || a.size() < 3) return def;
            if (!a[0].is_number() || !a[1].is_number() || !a[2].is_number()) return def;
            return Vec3f(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());

            //return ReadVec3(m_j, key, def);
        }

        // Descente dans un sous-objet. NON const : elle consomme une cle.
        [[nodiscard]] JsonReader Child(const char* key)
        {
            m_seen.insert(key);
            static const nlohmann::json s_empty = nlohmann::json::object();
            const auto it = m_j.find(key);
            const nlohmann::json& sub = (it != m_j.end() && it->is_object()) ? *it : s_empty;
            return JsonReader(sub, m_comp + "." + key, m_owner);
        }

        [[nodiscard]] bool Has(std::string_view key) const { return m_j.contains(key); }

        // A appeler en DERNIER : toute cle jamais passee par Read()/Child() est inconnue.
        void WarnUnread() const
        {
            for (auto& [key, _] : m_j.items())
            {
                if (key.starts_with('_')) continue;   // "_comment", "_version"... : assume
                if (!m_seen.contains(key))
                    Logger::warn("\033[31m[" + m_comp + "] cle ignoree '" + key + "' sur " + m_owner + "\033[0m");
            }
        }

    private:
        const nlohmann::json& m_j;
        std::string                        m_comp, m_owner;
        std::set<std::string, std::less<>> m_seen;
    };

} // namespace LV3