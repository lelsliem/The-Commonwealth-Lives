//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   What's in a name? That which we call a rose by any other name             //
//   would smell as sweet.                                                     //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Config/Configuration.h"
#include "LCE/Simulation/EntityId.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Name — the identity stone (0.7.0 Stone 1). Every mind carries one,
    // persisted in the co-save like a pouch: the same settler keeps the
    // same name across reload, fast travel, and despawning. The game's
    // own names win (Sturges stays Sturges); a generic "Settler" gets a
    // procedural Commonwealth name — male or female from the actor's
    // sex, drawn deterministically per entity id and deduped against the
    // world's live names — and an owned animal gets an animal name from
    // its own pool (a stray stays nameless). Children are named by their
    // household's family name. Defined here, the component namespace
    // beside FormRef and CapPouch; the pools and generation live in
    // Names below.
    //-------------------------------------------------------------------------
    struct Name
    {
        std::string Full;
    };
}

namespace TLC::Names
{
    using LCE::Simulation::EntityId;

    //-------------------------------------------------------------------------
    // Gender — the naming pools are gendered for people. The sim-only
    // children (no game actor) draw their gender deterministically from
    // their entity id; animals use their own pool entirely.
    //-------------------------------------------------------------------------
    enum class Gender
    {
        Male,
        Female,
    };

    //-------------------------------------------------------------------------
    // The name lists — the author's own Commonwealth. Defaults here are
    // deliberately free of the canon's characters (Sturges, Marcy, Piper
    // all keep their game names through the game-name-first rule; these
    // are only for the nameless), and every list is overridable in the
    // INI — names.first.male, names.first.female, names.first.animal,
    // names.last — comma-separated, so the author curates the world's
    // people without a recompile. A missing or broken list keeps the
    // default (a bad line must never break the world).
    //-------------------------------------------------------------------------
    struct NamePool
    {
        std::vector<std::string> MaleFirsts;
        std::vector<std::string> FemaleFirsts;
        std::vector<std::string> AnimalFirsts;
        std::vector<std::string> Lasts;
    };

    inline NamePool DefaultPool()
    {
        static const NamePool kPool{
            // Male firsts
            { "Cole", "Titus", "Wes", "Sal", "Otis", "Bram",
              "Rook", "Tommy", "Hank", "Lester", "Cyrus", "Gus",
              "Deke", "Milo", "Rafe", "Zeke" },
            // Female firsts
            { "Vera", "Mara", "June", "Dot", "Petra", "Lila",
              "Nessa", "Ida", "Greta", "Poppy", "Mae", "Willa",
              "Sable", "Fern", "Corrin", "Ada" },
            // Animal firsts — a separate pool: dogs, brahmin, cats and
            // the rest of the Commonwealth's companions get their own
            // kind of name, not a human one.
            { "Rex", "Bessie", "Bandit", "Daisy", "Mutt", "Ruff",
              "Patch", "Sadie", "Bruiser", "Fern", "Biscuit", "Scout",
              "Mabel", "Tater", "Widget", "Bones" },
            // Lasts
            { "Hart", "Wells", "Price", "Kade", "Marsh", "Ortiz",
              "Slater", "Vance", "Bishop", "Crane", "Dale", "Fitch",
              "Hawke", "Grey", "Nash", "Pratt", "Quinn", "Sutton" },
        };

        return kPool;
    }

    //-------------------------------------------------------------------------
    // ParseList — one INI list ("Vera, Cole, Mara") into strings: split
    // on commas, trim whitespace, drop empties. A malformed line yields
    // fewer names — or none, in which case the caller keeps the default.
    //-------------------------------------------------------------------------
    inline std::vector<std::string> ParseList(std::string_view a_text)
    {
        std::vector<std::string> out;

        std::size_t start = 0;

        while (start <= a_text.size())
        {
            auto comma = a_text.find(',', start);

            if (comma == std::string_view::npos)
            {
                comma = a_text.size();
            }

            auto item = a_text.substr(start, comma - start);

            while (!item.empty()
                && std::isspace(static_cast<unsigned char>(item.front())))
            {
                item.remove_prefix(1);
            }

            while (!item.empty()
                && std::isspace(static_cast<unsigned char>(item.back())))
            {
                item.remove_suffix(1);
            }

            if (!item.empty())
            {
                out.emplace_back(item);
            }

            if (comma == a_text.size())
            {
                break;
            }

            start = comma + 1;
        }

        return out;
    }

    //-------------------------------------------------------------------------
    // PoolFrom — the INI's version of the pool. Each key overrides its
    // list; a missing or unparsable key keeps the default list (never a
    // broken world). One line per key, comma-separated:
    //
    //     names.first.male   = Cole, Titus, Wes
    //     names.first.female = Vera, Mara, June
    //     names.first.animal = Rex, Bessie, Bandit
    //     names.last         = Hart, Wells, Price
    //-------------------------------------------------------------------------
    inline NamePool PoolFrom(const LCE::Config::Configuration& a_config)
    {
        auto pool = DefaultPool();

        const auto read = [&a_config](
                              std::string_view key,
                              const std::vector<std::string>& a_fallback)
        {
            const auto raw = a_config.Get(key);

            if (raw.empty())
            {
                return a_fallback;
            }

            const auto parsed = ParseList(raw);

            return parsed.empty() ? a_fallback : parsed;
        };

        pool.MaleFirsts = read("names.first.male", pool.MaleFirsts);
        pool.FemaleFirsts = read("names.first.female", pool.FemaleFirsts);
        pool.AnimalFirsts = read("names.first.animal", pool.AnimalFirsts);
        pool.Lasts = read("names.last", pool.Lasts);

        return pool;
    }

    //-------------------------------------------------------------------------
    // IsGenericName — is this the game's placeholder for "no name yet"?
    // A real NPC keeps its name; a generic settler is named by the sim.
    // Empty counts (some refs read an empty full-name) and the two stock
    // placeholders FO4 uses for workshop actors.
    //-------------------------------------------------------------------------
    inline bool IsGenericName(std::string_view a_name) noexcept
    {
        if (a_name.empty())
        {
            return true;
        }

        constexpr std::string_view kSettler = "Settler";
        constexpr std::string_view kWorker = "Workshop Worker";

        const auto equals = [a_name](std::string_view other)
        {
            if (a_name.size() != other.size())
            {
                return false;
            }

            for (std::size_t i = 0; i < a_name.size(); ++i)
            {
                const auto c = static_cast<unsigned char>(a_name[i]);
                const auto o = static_cast<unsigned char>(other[i]);

                if (std::tolower(c) != std::tolower(o))
                {
                    return false;
                }
            }

            return true;
        };

        return equals(kSettler) || equals(kWorker);
    }

    //-------------------------------------------------------------------------
    // The deterministic draw: fold the entity id and the attempt counter
    // into a stable 64-bit mix (splitmix-style), then take the first name
    // from the low bits and the family name from the high bits — one id,
    // one name, forever. The attempt shifts the mix so a collision can
    // step to the next candidate without losing determinism.
    //-------------------------------------------------------------------------
    inline std::uint64_t Mix(std::uint64_t a_seed) noexcept
    {
        a_seed += 0x9E3779B97F4A7C15ull;
        a_seed = (a_seed ^ (a_seed >> 30)) * 0xBF58476D1CE4E5B9ull;
        a_seed = (a_seed ^ (a_seed >> 27)) * 0x94D049BB133111EBull;

        return a_seed ^ (a_seed >> 31);
    }

    // A person's name: first from the gender's list, family from the
    // shared lasts. Both lists must be non-empty — a fully emptied INI
    // list falls back to the defaults (PoolFrom guarantees it), and the
    // defensive fallback below keeps a name even for a broken pool.
    inline std::string GenerateName(
        EntityId a_id, const NamePool& a_pool,
        Gender a_gender, std::uint32_t a_attempt = 0) noexcept
    {
        const auto& firsts = a_gender == Gender::Male
            ? a_pool.MaleFirsts : a_pool.FemaleFirsts;

        if (firsts.empty() || a_pool.Lasts.empty())
        {
            return "Settler";
        }

        const auto seed = Mix(
            a_id.Value() * 0x9E3779B97F4A7C15ull
            + static_cast<std::uint64_t>(a_attempt) * 0xC2B2AE3D27D4EB4Full);

        const auto first = firsts[seed % firsts.size()];
        const auto last = a_pool.Lasts[(seed >> 32) % a_pool.Lasts.size()];

        return first + " " + last;
    }

    // An owned animal's name: its own pool, no family name — a dog is
    // "Rex", not "Rex Hart". A brahmin and a cat share the pool; the
    // species split is the Behaviour table's business, the name is just
    // theirs. Only owned animals are named — a stray stays nameless and
    // the log labels it by species and hex.
    inline std::string GenerateAnimalName(
        EntityId a_id, const NamePool& a_pool,
        std::uint32_t a_attempt = 0) noexcept
    {
        if (a_pool.AnimalFirsts.empty())
        {
            return "Mutt";
        }

        const auto seed = Mix(
            a_id.Value() * 0x9E3779B97F4A7C15ull
            + static_cast<std::uint64_t>(a_attempt) * 0xC2B2AE3D27D4EB4Full);

        return a_pool.AnimalFirsts[seed % a_pool.AnimalFirsts.size()];
    }

    //-------------------------------------------------------------------------
    // GenderOf — a sim-only mind's gender, drawn deterministically from
    // its entity id (a mind with a game actor reads the actor's sex at
    // seed time instead). One bit of the mix — stable for the entity's
    // whole life, so a restored child stays the same gender.
    //-------------------------------------------------------------------------
    inline Gender GenderOf(EntityId a_id) noexcept
    {
        return (Mix(a_id.Value()) & 1u) != 0 ? Gender::Male : Gender::Female;
    }

    //-------------------------------------------------------------------------
    // GenerateUnique — a free name for a_id. The caller keeps the world's
    // used set (rebuilt at world start and restore); this steps attempts
    // until the draw is free. Deterministic: the same world always finds
    // the same first free name. The animal variant draws from the animal
    // pool and reserves the name against people too (a settlement with a
    // dog named "Patch" never gains a person named "Patch").
    //-------------------------------------------------------------------------
    inline std::string GenerateUnique(
        std::unordered_set<std::string>& a_used,
        EntityId a_id, const NamePool& a_pool, Gender a_gender) noexcept
    {
        for (std::uint32_t attempt = 0; attempt < 64; ++attempt)
        {
            auto candidate = GenerateName(a_id, a_pool, a_gender, attempt);

            if (a_used.insert(candidate).second)
            {
                return candidate;
            }
        }

        return GenerateName(a_id, a_pool, a_gender,
            static_cast<std::uint32_t>(a_id.Value()));
    }

    inline std::string GenerateUniqueAnimal(
        std::unordered_set<std::string>& a_used,
        EntityId a_id, const NamePool& a_pool) noexcept
    {
        for (std::uint32_t attempt = 0; attempt < 64; ++attempt)
        {
            auto candidate = GenerateAnimalName(a_id, a_pool, attempt);

            if (a_used.insert(candidate).second)
            {
                return candidate;
            }
        }

        return GenerateAnimalName(a_id, a_pool,
            static_cast<std::uint32_t>(a_id.Value()));
    }

    //-------------------------------------------------------------------------
    // FamilyOf — the family name of a full name: the last word ("Marcy
    // Long" → "Long", "Rex" → ""). Children born to a household carry
    // their parents' family name.
    //-------------------------------------------------------------------------
    inline std::string_view FamilyOf(std::string_view a_full) noexcept
    {
        const auto space = a_full.find_last_of(' ');

        if (space == std::string_view::npos
            || space + 1 >= a_full.size())
        {
            return {};
        }

        return a_full.substr(space + 1);
    }

    //-------------------------------------------------------------------------
    // ChildName — a child's first name (drawn deterministically from the
    // child's id and gender) plus the family name of its household: "Mara
    // Vance" of the Vances. No family name yet (a procedural parent) —
    // fall back to a plain procedural name.
    //-------------------------------------------------------------------------
    inline std::string ChildName(
        std::string_view a_family,
        EntityId a_id, const NamePool& a_pool, Gender a_gender) noexcept
    {
        auto name = GenerateName(a_id, a_pool, a_gender);

        if (a_family.empty())
        {
            return name;
        }

        const auto space = name.find(' ');

        if (space != std::string::npos)
        {
            name = name.substr(0, space);
        }

        return name + " " + std::string(a_family);
    }
}
