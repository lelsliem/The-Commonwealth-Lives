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
    // DefaultPool — the curated catalog (LineCatalog.md, 2026-08-15):
    // the game's own settler/guard-bank recordings, hand-picked into the
    // pools. One line per comma; a line never contains a comma itself —
    // the INI list parser splits on commas, so mid-line pauses use an
    // em-dash ("Hey — how's it going?"). The row pool carries the guard
    // bank's escalation ("Last warning.", "I'm warning you. Back off!") —
    // the register settlers never got. Fight (2) and Feud (2) are thin by
    // design: rare events whose real content (who, why) is memory-driven
    // caption text, not voiced lines.
    //-------------------------------------------------------------------------
    inline DialoguePool DefaultPool()
    {
        static const DialoguePool kPool{
            // Greet — the good, a hello. (30)
            { "Hey...", "Hey.", "Howdy.", "Hey there.",
              "Hey — how's it going?", "How's it going friend?",
              "Hey — thanks for stopping by.", "Hey there! Welcome back.",
              "Hello neighbor.", "Hey buddy.", "Hello handsome.",
              "Hello cutey.", "Hey bean — what's cookin'?",
              "Howdy friend. Grab a seat by the fire. Everyone's welcome.",
              "Hey — I just wanted to introduce myself.",
              "Hey there. Trading?", "Well hey there pup.",
              "Woah... is it morning already?", "Hey thanks!",
              "Hey again. Step right up. Little bit of everything from all over.",
              "Hey — he brought it up — not me. As I was saying.",
              "Hey — feel free to sit down or whatever. But — uh — well... I think I ate a bad can of meat.",
              "Hey — this paranoia's what keeps us alive.",
              "Hey. Market'll be open in the morning. Come by then.",
              "I know you're probably busy.",
              "Hey — I'm getting over someone — okay? A little compassion over here.",
              "Either way — good to see a new face.",
              "I don't think we've met. What can I do to help out?",
              "By the way — feel free to use our workshop. It's the least we can do.",
              "Go away. It's the middle of the night... oh hi..." },
            // Gossip — the settlement's small talk. (23)
            { "Why's it gotta be so quiet?", "Got work to do — can't talk now.",
              "People are having to sleep in shifts. It's making everyone a bit cranky.",
              "Things are a bit crowded. A few more beds would lift everyone's spirits.",
              "Nothing really. Pretty quiet.", "My heart goes out to those folks.",
              "I heard we had a trader in today.",
              "Almost out of stimpacks. We'll need more soon.",
              "Nice to have some good news around here for a change.",
              "What I wouldn't give for some real time off.",
              "I'm just so hungry all the time.",
              "Another day of hard work. It never changes.",
              "Can't remember the last time I had clean fingernails.",
              "Farming's as honest as honest work gets.",
              "Crop's comin' in pretty good.",
              "The farm's not much — but it's something.",
              "Easy living — this ain't.",
              "It's not perfect — of course — but I think with some work we can make a go of it.",
              "Well... I guess there's nothing for it but to do our best.",
              "If we want things to get better — we've got to start helping each other.",
              "Well — hopefully tonight will be better.",
              "Better than working — right?", "I think I ate too much." },
            // Row — the bad, the guard bank's escalation plus the edges. (32)
            { "Beat it.", "Keep walkin'.", "Leave me alone!",
              "Back off. This is mine.",
              "I'm not looking for trouble. Enough junk here for both of us.",
              "Just move along.", "You're botherin' me.",
              "Look — just leave me alone.", "Got a problem?",
              "Ain't looking for trouble — I hope.",
              "My back hurts — my feet hurt. Everything hurts.",
              "Don't talk to me. Might attract someone's attention.",
              "Don't talk to me.", "Leave. Now. Or else.",
              "Better not try that again.",
              "Just... stop. Okay? We got our own problems.",
              "Finders keepers — the law of the wasteland.", "Bullshit.",
              "Asshole...", "All done. Any other complaints?",
              "I'm warning you. Back off!", "Last warning.",
              "Get out of here you.", "Keep moving.", "So that's it — huh?",
              "What? You got a staring problem?",
              "You cause trouble — there's gonna be trouble — got it?",
              "No loitering.", "Stay out of trouble.", "Don't try anything.",
              "You got bricks for ears? I said get the hell out of here. Now.",
              "Told you to move along. Find somewhere else to gawk." },
            // Trade — the market's words. (30)
            { "Looking to buy?", "Welcome back. Looking to trade?",
              "Lookin' to make a deal?", "It's late. Ya gonna buy somethin'?",
              "If you're here to trade — let's trade.",
              "We've got some supplies I can offer — if you're interested.",
              "Got some goods I can sell you — if you're lookin'.",
              "It ain't a lot — but I've got a few basic supplies. Ammo — meds — that sort of thing.",
              "Got a few supplies I can trade. The one thing we don't need is more junk. Got enough of that already.",
              "If you're here to stock up — I've got some supplies. The junk you see laying around isn't for sale — though.",
              "Got a few things for trade — if you're interested. Lord knows it wasn't always that way.",
              "Things were pretty lean for a while — but now we've got plenty to trade. Interested?",
              "You know the score — friend. If you've got the caps — we can talk.",
              "Yeah sure. Whatever I've got left — I'm happy to trade for.",
              "Come back later — my stock's always changing.", "No caps — no room.",
              "I can help — but not for free. You'll need to come back when you have the caps. You need anything else?",
              "Come see me when you've got the caps.", "I'm just here for the caps.",
              "Just had to push — didn't ya? 50 caps — take it or leave it.",
              "I'm not here to haggle. You'll get whatever I feel like giving you — got it? Now what's it gonna be?",
              "So — do we have a deal?", "Two caps each.",
              "A little — but buy the lot and I'll throw in a dose of Rad Away as a chaser. Deal?",
              "You're looking at 'em. But I'll sell you a stake in it. Hundred caps. Wait — no — fifty. Ten?",
              "What'll ya have?", "What're ya drinking?",
              "Step right up — all the clothing fit to wear.", "Need armor?",
              "Can never have enough ammunition. Pricey — though." },
            // Family — the warmth. (16)
            { "Running from something? Welcome home.",
              "It's a good life we've got here — and we're grateful for it.",
              "Hey — how's the family doing?",
              "Not a day goes by that I don't thank my lucky stars that I live here.",
              "Yep — we're all still here — thank God.",
              "Yeah. It's gonna take my son a while to get off the chems — but we'll make it. We always do.",
              "I just wanted to provide for my family.",
              "I'll do whatever it takes to earn back my family's trust.",
              "Things have almost returned to normal. I'm grateful for that.",
              "This would feel a lot more like a home if everyone had their own bed.",
              "We just wanted to say how grateful we are for the opportunity.",
              "It's not everything we expected — but we're willing to work hard to make this a home we can be proud of.",
              "We knew the risks of having kids these days — and we took 'em.",
              "Well — you're home now — and you should relax.",
              "Good — when my husband actually gets around to doing it. That is — if no one raids the farm first.",
              "We were looking for someplace to make a new life — so here we are." },
            // Grief — the ugly. (15)
            { "She's... she's dead!",
              "Here one second — gone the next. Who moves that fast?",
              "Gone... gotta be...", "I've lost... everything.",
              "Jesus. They're dead...", "I can't believe they're dead...",
              "He... He's dead...", "I'm truly sorry.",
              "I'm sorry. I know how hard this has been.", "Damn. I'm really sorry.",
              "One way of looking at it. The other is to be thankful so many people passed.",
              "Oh... I'm sorry. I probably shouldn't have asked. I hope I haven't re-opened any old wounds.",
              "I don't know how they carry on after something like that.",
              "It's just horrible.", "I should never have gotten my hopes up." },
            // Fight — the escalation's pay-off; the guard bank's threats. (2)
            { "Don't think I won't hurt you!", "Ain't my fight!" },
            // Feud — the long cold shoulder. (2)
            { "Don't talk to me. Might attract someone's attention.",
              "Don't talk to me." },
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
