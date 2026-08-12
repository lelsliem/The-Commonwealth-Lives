//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   News travels fast when everyone knows everyone.                           //
//                                                                             //
//=============================================================================//

#pragma once

#include "Names.h"  // ParseList — the pools split the same way

#include "LCE/Config/Configuration.h"
#include "LCE/Simulation/Entity/EntityId.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TLC::Dialogue
{
    using LCE::Simulation::EntityId;

    //-------------------------------------------------------------------------
    // Pool — the situations life throws at a mind. Each pool is a list of
    // one-liners in the author's tone: the good (greet, gossip, family),
    // the bad (trade, row), the ugly (grief, fight, feud). Speech is a
    // presentation layer on interactions the sim already makes — this
    // header only names the words, never decides when they are said.
    //-------------------------------------------------------------------------
    enum class Pool
    {
        Greet,
        Gossip,
        Row,
        Trade,
        Family,
        Grief,
        Fight,
        Feud,
    };

    //-------------------------------------------------------------------------
    // DialoguePool — the lists. Every list is overridable in the INI —
    // dialogue.greet, dialogue.gossip, dialogue.row, dialogue.trade,
    // dialogue.family, dialogue.grief, dialogue.fight, dialogue.feud —
    // comma-separated one-liners. A missing or broken list keeps the
    // default (a bad line must never break the world), exactly like the
    // name pools.
    //-------------------------------------------------------------------------
    struct DialoguePool
    {
        std::vector<std::string> Greet;
        std::vector<std::string> Gossip;
        std::vector<std::string> Row;
        std::vector<std::string> Trade;
        std::vector<std::string> Family;
        std::vector<std::string> Grief;
        std::vector<std::string> Fight;
        std::vector<std::string> Feud;
    };

    //-------------------------------------------------------------------------
    // DefaultPool — the author's starter sets (drafted 2026-08-11), one
    // line per comma: short, punchy, wasteland dialect — if it takes more
    // than ten words, cut it. The row pool is the author's verbal→
    // physical ramp in five lines: the complaint, the dare, the taunt,
    // the break point, and the escalation — a row uses the early lines,
    // a fight is earned when the last one lands.
    //-------------------------------------------------------------------------
    inline DialoguePool DefaultPool()
    {
        static const DialoguePool kPool{
            // Greet — the good, a hello.
            { "Mornin'", "You're up early", "Settler", "Peaceful day",
              "Still breathing — good", "Good to see you", "Quiet today",
              "Coffee's cold — company's warm", "Heard you was coming" },
            // Gossip — the settlement's small talk.
            { "Heard about the market?", "There's trouble brewing",
              "Keep it between us", "They're at it again",
              "Storm's on the way", "Caps are tight this week",
              "Saw a trader on the road", "The kids are driving me mad",
              "Another settlement's gone quiet", "Don't trust the weather" },
            // Row — the bad, the author's five-line ramp plus the edges.
            { "You ripped me off", "You do that again, I dare you",
              "Go on, one more time", "I've had it with you",
              "Want some? Let's go", "You looking at me?",
              "Say that again", "Don't start with me", "We had a deal",
              "That's my stall", "Keep walking", "You owe me",
              "We're done here" },
            // Trade — the market's words.
            { "Fresh goods", "Caps first", "Deal", "Don't short me",
              "That's robbery!", "Fair price", "Take it or leave it",
              "You'll not get better", "Done — next",
              "Good doing business", "That's daylight robbery" },
            // Family — the warmth.
            { "Dinner's ready", "Good night", "Stay close",
              "We'll make it work", "Look after the little one",
              "Home's where the bench is", "We eat together",
              "Don't wander far" },
            // Grief — the ugly.
            { "They're gone", "Who feeds us now?", "I miss them",
              "They'd want us to keep going", "Can't believe it",
              "The house is too quiet", "We'll remember them",
              "It ain't right" },
            // Fight — the escalation's pay-off.
            { "Come on then!", "Let's settle this", "You asked for it",
              "This ends now", "Enough talk", "Put 'em up",
              "Last one standing wins" },
            // Feud — the long cold shoulder.
            { "I'll remember this", "You made an enemy today",
              "Don't turn your back", "This ain't over",
              "We're done — you and me", "The whole town knows what you did",
              "Mark my words" },
        };

        return kPool;
    }

    //-------------------------------------------------------------------------
    // PoolFrom — the INI's version of the pools. Each key overrides its
    // list; a missing or unparsable key keeps the default list. One line
    // per key, comma-separated:
    //
    //     dialogue.greet  = Mornin', Settler, Peaceful day
    //     dialogue.gossip = Heard about the market?, Caps are tight
    //     dialogue.row    = You ripped me off, Want some? Let's go
    //     ...
    //-------------------------------------------------------------------------
    inline DialoguePool PoolFrom(const LCE::Config::Configuration& a_config)
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

            const auto parsed = Names::ParseList(raw);

            return parsed.empty() ? a_fallback : parsed;
        };

        pool.Greet = read("dialogue.greet", pool.Greet);
        pool.Gossip = read("dialogue.gossip", pool.Gossip);
        pool.Row = read("dialogue.row", pool.Row);
        pool.Trade = read("dialogue.trade", pool.Trade);
        pool.Family = read("dialogue.family", pool.Family);
        pool.Grief = read("dialogue.grief", pool.Grief);
        pool.Fight = read("dialogue.fight", pool.Fight);
        pool.Feud = read("dialogue.feud", pool.Feud);

        return pool;
    }

    //-------------------------------------------------------------------------
    // Lines — the pool a category names. A missing list (a broken pool)
    // yields nothing, and the caller simply says nothing — speech is
    // optional; silence is a safe default.
    //-------------------------------------------------------------------------
    inline const std::vector<std::string>& Lines(
        const DialoguePool& a_pool, Pool a_category) noexcept
    {
        switch (a_category)
        {
            case Pool::Greet: return a_pool.Greet;
            case Pool::Gossip: return a_pool.Gossip;
            case Pool::Row: return a_pool.Row;
            case Pool::Trade: return a_pool.Trade;
            case Pool::Family: return a_pool.Family;
            case Pool::Grief: return a_pool.Grief;
            case Pool::Fight: return a_pool.Fight;
            case Pool::Feud: return a_pool.Feud;
        }

        return a_pool.Greet;
    }

    //-------------------------------------------------------------------------
    // Pick — one line, deterministic per mind and per day: the same mind
    // says the same line all day (a greeting you can get used to) and a
    // different one tomorrow (the world moves on). Seeded like the names
    // (a splitmix fold) with the pool's own salt so two categories never
    // pick in lockstep. An empty pool says nothing.
    //-------------------------------------------------------------------------
    inline std::string Pick(
        const DialoguePool& a_pool,
        Pool a_category, EntityId a_id, std::uint64_t a_day) noexcept
    {
        const auto& lines = Lines(a_pool, a_category);

        if (lines.empty())
        {
            return {};
        }

        // The mix folds the entity, the day, and a per-category salt —
        // deterministic, and two pools never echo each other.
        std::uint64_t seed = a_id.Value();
        seed ^= a_day * 0x9E3779B97F4A7C15ull;
        seed ^= static_cast<std::uint64_t>(a_category) * 0xC2B2AE3D27D4EB4Full;

        seed += 0x9E3779B97F4A7C15ull;
        seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ull;
        seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBull;
        seed ^= seed >> 31;

        return lines[seed % lines.size()];
    }
}
