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

#include "Behaviour.h"  // Species — the generic-name rule is species-aware

#include "LCE/Config/Configuration.h"
#include "LCE/Simulation/Entity/EntityId.h"

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
        // The author's curated Commonwealth, from Docs/name.md: the
        // canon's characters keep their game names through the
        // game-name-first rule, so these are only for the nameless. The
        // tail of each list is the author's family first names — kept
        // as a quiet easter egg, never reordered.
        static const NamePool kPool{
            // Male firsts (82)
            { "Aiden", "Alexander", "Asher", "Benjamin", "Caleb",
              "Daniel", "Dylan", "Elias", "Elijah", "Ethan", "Ezra",
              "Gabriel", "Grayson", "Henry", "Hudson", "Isaac",
              "Jack", "Jacob", "James", "Joseph", "Julian", "Leo",
              "Levi", "Liam", "Logan", "Luke", "Mason", "Michael",
              "Miles", "Nathan", "Noah", "Oliver", "Owen", "Samuel",
              "Sebastian", "Theodore", "Thomas", "William", "Wyatt",
              "Zachary", "Zane", "Archer", "Atlas", "Beckett",
              "Callum", "Colton", "Declan", "Enzo", "Finn",
              "Holden", "Jasper", "Jonah", "Landon", "Luca",
              "Maddox", "Micah", "Nico", "Parker", "Reid", "Rowan",
              "Sawyer", "Silas", "Theo", "Tucker", "Walker",
              "Weston", "Milo", "Kai", "Ronan", "Ellis", "Flynn",
              // The family names — the tail, kept as-is.
              "Corey", "Leslie", "Frederick", "Amos", "Tyler",
              "Steven", "Harrison", "Damien", "Kassius", "Sebastien",
              "Shea" },
            // Female firsts (78)
            { "Olivia", "Amelia", "Isla", "Ava", "Ivy", "Freya",
              "Lily", "Florence", "Mia", "Willow", "Rosie",
              "Sophia", "Isabella", "Grace", "Daisy", "Sienna",
              "Poppy", "Elsie", "Emily", "Ella", "Evelyn", "Phoebe",
              "Sofia", "Evie", "Charlotte", "Harper", "Millie",
              "Matilda", "Maya", "Sophie", "Alice", "Emilia",
              "Isabelle", "Ruby", "Luna", "Maisie", "Aria",
              "Penelope", "Mila", "Bonnie", "Eva", "Hallie",
              "Eliza", "Ada", "Violet", "Esme", "Arabella",
              "Imogen", "Delilah", "Lottie", "Chloe", "Thea",
              "Layla", "Eleanor", "Aurora", "Margot", "Mabel",
              "Elizabeth", "Emma", "Scarlett", "Harriet", "Lola",
              "Nancy", "Rose", "Zara", "Iris", "Robyn", "Molly",
              "Olive", "Ellie", "Beatrice", "Sara",
              // The family names — the tail, kept as-is.
              "Áine", "Rosalee", "Debbie", "Pheonix", "Teresa",
              "Rita" },
            // Animal firsts (54) — a separate pool: dogs, brahmin, cats
            // and the rest of the Commonwealth's companions get their
            // own kind of name, not a human one. Only owned animals are
            // named; a stray stays nameless.
            { "Rex", "Bandit", "Daisy", "Sadie", "Biscuit", "Scout",
              "Bear", "Benji", "Bentley", "Buddy", "Charlie",
              "Chewie", "Coco", "Duke", "Echo", "Gizmo", "Gracie",
              "Gus", "Hank", "Harley", "Honey", "Hooch", "Jack",
              "Jasper", "Jax", "Jet", "Juno", "Koda", "Lucky",
              "Maggie", "Max", "Milo", "Misty", "Mocha", "Nala",
              "Nellie", "Patch", "Pebbles", "Penny", "Pepper",
              "Rocco", "Rocky", "Rusty", "Sam", "Scrappy", "Shadow",
              "Simba", "Socks", "Sparky", "Tank", "Toby", "Wally",
              // The family names — the tail, kept as-is.
              "Fluffy", "Winston" },
            // Lasts (75) — the Commonwealth's common families.
            { "Smith", "Johnson", "Williams", "Brown", "Jones",
              "Garcia", "Miller", "Davis", "Rodriguez", "Martinez",
              "Hernandez", "Lopez", "Wilson", "Anderson", "Thomas",
              "Taylor", "Moore", "Jackson", "Martin", "Lee", "Perez",
              "Thompson", "White", "Harris", "Sanchez", "Clark",
              "Lewis", "Robinson", "Walker", "Young", "Allen",
              "King", "Wright", "Scott", "Torres", "Hill", "Green",
              "Adams", "Baker", "Hall", "Rivera", "Campbell",
              "Mitchell", "Carter", "Roberts", "Turner", "Parker",
              "Edwards", "Collins", "Stewart", "Morris", "Murphy",
              "Cook", "Rogers", "Morgan", "Cooper", "Bailey",
              "Reed", "Kelly", "Howard", "Ward", "Brooks", "Wood",
              "James", "Bennett", "Gray", "Price", "Wells", "Hart",
              "Ford", "Cole", "Burns", "Stone", "Fox", "Rose" },
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
    // EqualsFold — case-insensitive whole-string compare (ASCII). The
    // name rules are case-insensitive: "settler" is as generic as
    // "Settler", and "PROVISIONER" is as much a role as "Provisioner".
    //-------------------------------------------------------------------------
    inline bool EqualsFold(
        std::string_view a, std::string_view b) noexcept
    {
        if (a.size() != b.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const auto c = static_cast<unsigned char>(a[i]);
            const auto o = static_cast<unsigned char>(b[i]);

            if (std::tolower(c) != std::tolower(o))
            {
                return false;
            }
        }

        return true;
    }

    //-------------------------------------------------------------------------
    // IsGenericName — is this the game's placeholder for "no name yet"?
    // A real NPC keeps its name; a generic settler is named by the sim.
    // Empty counts (some refs read an empty full-name), the two stock
    // placeholders FO4 uses for workshop humans ("Settler", "Workshop
    // Worker"), and — for animals — the plain species words ("Dog",
    // "Cat", "Brahmin"): a species label is not a name, so an owned
    // companion gets a real one. A creature with a proper name keeps it
    // (Dogmeat stays Dogmeat). Case-insensitive.
    //-------------------------------------------------------------------------
    inline bool IsGenericName(
        std::string_view a_name, Species a_species = Species::Human) noexcept
    {
        if (a_name.empty())
        {
            return true;
        }

        const auto equals = [a_name](std::string_view other)
        {
            return EqualsFold(a_name, other);
        };

        if (a_species == Species::Animal)
        {
            static constexpr std::string_view kSpeciesWords[] = {
                "Dog",           "Cat",          "Brahmin",
                "Rabbit",        "Molerat",      "Gorilla",
                "Radstag",       "Yao Guai",     "Deathclaw",
                "Mirelurk",      "Bloatfly",     "Bloodbug",
                "Radroach",      "Stingwing",    "Radscorpion",
                "Feral Ghoul",   "Ghoul",        "Raider Dog",
                "Super Mutant",  "Synth",        "Eyebot",
                "Protectron",    "Sentry Bot",   "Assaultron",
                "Handy",         "Turret",       "Vertibird",
                "Mirelurk King", "Mirelurk Queen",
                // The Red Rocket resident: "Junkyard Dog" is a role
                // label like "Dog", not a name — owned, it gets one.
                "Junkyard Dog",
            };

            for (const auto word : kSpeciesWords)
            {
                if (equals(word))
                {
                    return true;
                }
            }

            return false;
        }

        return equals("Settler") || equals("Workshop Worker");
    }

    //-------------------------------------------------------------------------
    // IsRoleName — the game's role labels (0.7.3 Stone 1). A generic
    // "Provisioner", "Guard" or "Minuteman" is a title, not a name:
    // every provisioner in the Commonwealth reads identical in memory,
    // and a trade needs to tell two provisioners apart. The sim keeps
    // the role as a prefix and adds the person — "Provisioner Cole".
    // Returns the canonical role word (matched case-insensitively) or
    // empty for a real name. People only: an animal's "Junkyard Dog"
    // is a species word, owned by IsGenericName's animal list.
    //-------------------------------------------------------------------------
    inline std::string_view IsRoleName(std::string_view a_name) noexcept
    {
        static constexpr std::string_view kRoles[] = {
            "Provisioner", "Guard", "Minuteman",
            "Caravan Guard", "Trader", "Merchant",
        };

        for (const auto role : kRoles)
        {
            if (EqualsFold(a_name, role))
            {
                return role;
            }
        }

        return {};
    }

    //-------------------------------------------------------------------------
    // HasRolePrefix — does this name already carry the role's title?
    // "Provisioner Cole" has the "Provisioner" prefix; "Cole Hart"
    // and a bare "Provisioner" do not. The converge rule uses it to
    // know when a mind already wears its role name.
    //-------------------------------------------------------------------------
    inline bool HasRolePrefix(
        std::string_view a_name, std::string_view a_role) noexcept
    {
        return a_name.size() > a_role.size()
            && a_name.compare(0, a_role.size(), a_role) == 0
            && a_name[a_role.size()] == ' ';
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

    // A role name: the title the game gave ("Provisioner") plus a first
    // name drawn from the person's gender pool — "Provisioner Cole",
    // "Guard Mara". No family name: the role is the person's calling
    // card in memory, and two provisioners are told apart by their
    // firsts. Deterministic per id like any name.
    inline std::string GenerateRoleName(
        std::string_view a_role, EntityId a_id, const NamePool& a_pool,
        Gender a_gender, std::uint32_t a_attempt = 0) noexcept
    {
        const auto& firsts = a_gender == Gender::Male
            ? a_pool.MaleFirsts : a_pool.FemaleFirsts;

        if (firsts.empty())
        {
            return std::string(a_role);
        }

        const auto seed = Mix(
            a_id.Value() * 0x9E3779B97F4A7C15ull
            + static_cast<std::uint64_t>(a_attempt) * 0xC2B2AE3D27D4EB4Full);

        return std::string(a_role) + " "
            + firsts[seed % firsts.size()];
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

    // The role variant of GenerateUnique: step attempts until the role
    // name is free of the world's used set (a settlement can't gain two
    // "Provisioner Cole"s). Deterministic, like every draw.
    inline std::string GenerateUniqueRole(
        std::unordered_set<std::string>& a_used,
        std::string_view a_role, EntityId a_id,
        const NamePool& a_pool, Gender a_gender) noexcept
    {
        for (std::uint32_t attempt = 0; attempt < 64; ++attempt)
        {
            auto candidate =
                GenerateRoleName(a_role, a_id, a_pool, a_gender, attempt);

            if (a_used.insert(candidate).second)
            {
                return candidate;
            }
        }

        return GenerateRoleName(a_role, a_id, a_pool, a_gender,
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
