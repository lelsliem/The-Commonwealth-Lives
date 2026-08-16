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
    // Voice — the banks the game resolves spoken audio by. A line's
    // audio only exists where its bank recorded it: the 8 settler
    // voices (Male/Female x Old/EvenToned/Rough/Boston), the 4 guard
    // voices, and the 2 child voices. The picker gates on this — a
    // line only speaks if the speaker's voice bank recorded it; no
    // match means the mind stays mute (captions only). The exact
    // bit positions are the table's contract; add a bank by extending
    // both the enum and the bitset below.
    //-------------------------------------------------------------------------
    enum class Voice : std::uint8_t
    {
        MaleOld,
        FemaleOld,
        MaleEvenToned,
        FemaleEvenToned,
        MaleRough,
        FemaleRough,
        MaleBoston,
        FemaleBoston,
        GuardMaleDiamondCity1,
        GuardMaleDiamondCity2,
        GuardMaleVault81,
        GuardFemaleVault81,
        MaleChild,
        FemaleChild,
        GhoulMale,
        GhoulFemale,
    };

    //-------------------------------------------------------------------------
    // VoiceSet — a bitset of the voices a line was recorded in. The
    // settler voices are bits 0-7, guards 8-11, children 12-13, ghouls
    // 14-15 (the ghoul banks recorded the same 8v generic lines as the
    // settlers, so a ghoul voice speaks them from the same table).
    //-------------------------------------------------------------------------
    using VoiceSet = std::uint16_t;

    inline constexpr VoiceSet kSettlerVoices = 0x00FF;
    inline constexpr VoiceSet kGuardVoices = 0x0F00;
    inline constexpr VoiceSet kChildVoices = 0x3000;
    inline constexpr VoiceSet kGhoulVoices = 0xC000;

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
    // Normalize — the lookup key for the coverage table: lowercase,
    // punctuation stripped, spaces collapsed. The INI ships comma-safe
    // lines (em-dashes for pauses); the catalog recorded the original
    // commas — normalization makes the two meet.
    //-------------------------------------------------------------------------
    inline std::string Normalize(std::string_view a_text) noexcept
    {
        std::string key;
        key.reserve(a_text.size());

        for (const char c : a_text)
        {
            if (c >= 'A' && c <= 'Z')
            {
                key.push_back(static_cast<char>(c - 'A' + 'a'));
            }
            else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            {
                key.push_back(c);
            }
            else if (!key.empty() && key.back() != ' ')
            {
                key.push_back(' ');
            }
        }

        while (!key.empty() && key.back() == ' ')
        {
            key.pop_back();
        }

        return key;
    }

    //-------------------------------------------------------------------------
    // CoverageFor — the voices a line was actually recorded in, from
    // the LineCatalog survey (Docs/Dialogue/LineCatalog.md, generated
    // by Not for github/gen_coverage.py). The table is keyed by the
    // normalized line text, so it survives the INI's comma-safe
    // em-dash form. A line the survey never saw (a player's custom
    // INI line) has no recording — coverage 0 — and is caption-only:
    // the picker never offers it for audio, which is the "the game
    // doesn't provide it, so stay mute" rule.
    //-------------------------------------------------------------------------
    // The catalog table: normalized text -> the line's INFO form id and
    // the voice bits that recorded it (0.8.7 — the form id is what the
    // audio probe resolves to the line's parent topic for Say; the
    // coverage is what PickForVoice gates on).
    inline const struct CoverageEntry
    {
        const char* Normalized;
        std::uint32_t FormId;
        std::uint16_t Coverage;
    } kCoverageTable[] = {
            // clang-format off
{ "a little but buy the lot and i ll throw in a dose of rad away as a chaser deal", 0x16eb02, 0x0040 },
{ "ain t looking for trouble i hope", 0x156a02, 0xC0FF },
{ "ain t my fight", 0x03429e, 0xC0FF },
{ "all done any other complaints", 0x05fc3d, 0xC0FF },
{ "almost out of stimpacks we ll need more soon", 0x18baaa, 0xC0FF },
{ "another day of hard work it never changes", 0x15524d, 0xC0FF },
{ "are you buyin or just in the way", 0x1ac0e2, 0x3000 },
{ "asshole", 0x1a89c3, 0xC0FC },
{ "aw you re no fun", 0x1ac0d5, 0x3000 },
{ "back off this is mine", 0x03593a, 0x00F4 },
{ "beat it", 0x0187b2, 0x00FC },
{ "better not try that again", 0x0d14a7, 0xC0FF },
{ "better than working right", 0x122ff3, 0x00F7 },
{ "bullshit", 0x18f9ff, 0x003C },
{ "by the way feel free to use our workshop it s the least we can do", 0x10c841, 0xC0FF },
{ "can never have enough ammunition pricey though", 0x05fb8c, 0x003C },
{ "can t remember the last time i had clean fingernails", 0x156a06, 0xC0FF },
{ "caps up front thank you", 0x122238, 0x3000 },
{ "chuckle easy caps", 0x183ef0, 0x3000 },
{ "come back later my stock s always changing", 0x1263da, 0x0080 },
{ "come see me when you ve got the caps", 0x11a084, 0x0001 },
{ "crop s comin in pretty good", 0x1569fc, 0xC0FF },
{ "dad can we leave a light on tonight i like sleeping with a light on", 0x1a70ad, 0x3000 },
{ "damn i m really sorry", 0x18ba94, 0xC0FF },
{ "don t talk to me", 0x1989f9, 0x0008 },
{ "don t talk to me might attract someone s attention", 0x022eae, 0x00FC },
{ "don t think i won t hurt you", 0x034143, 0xC0FF },
{ "don t try anything", 0x0d3fb1, 0x0F00 },
{ "easy living this ain t", 0x1569fe, 0xC0FF },
{ "either way good to see a new face", 0x19df5d, 0x0040 },
{ "farming s as honest as honest work gets", 0x156a0c, 0xC0FF },
{ "finders keepers the law of the wasteland", 0x1478e1, 0x00F0 },
{ "get out of here you", 0x085580, 0xCFFF },
{ "go away it s the middle of the night oh hi", 0x1a95fa, 0xC0FC },
{ "gone gotta be", 0x03412a, 0xC0FF },
{ "good when my husband actually gets around to doing it that is if no one raids the farm first", 0x1a02df, 0x0080 },
{ "got a few supplies i can trade the one thing we don t need is more junk got enough of that already", 0x161f05, 0x0010 },
{ "got a few things for trade if you re interested lord knows it wasn t always that way", 0x16a792, 0x0080 },
{ "got a problem", 0x05fc82, 0x003C },
{ "got some goods i can sell you if you re lookin", 0x04c916, 0x0010 },
{ "got work to do can t talk now", 0x07bb01, 0xC0FF },
{ "he he s dead", 0x17c949, 0x0004 },
{ "hello", 0x04650f, 0x3000 },
{ "hello cutey", 0x062029, 0x0005 },
{ "hello handsome", 0x062028, 0x0008 },
{ "hello neighbor", 0x19e9e6, 0x00CC },
{ "here one second gone the next who moves that fast", 0x034127, 0xC0FF },
{ "hey", 0x03f6f7, 0xC0FF },
{ "hey again step right up little bit of everything from all over", 0x020d39, 0x0080 },
{ "hey bean what s cookin", 0x16ada3, 0x0004 },
{ "hey buddy", 0x19ffcd, 0x0020 },
{ "hey feel free to sit down or whatever but uh well i think i ate a bad can of meat", 0x14ce79, 0x0040 },
{ "hey he brought it up not me as i was saying", 0x123702, 0x0040 },
{ "hey how s it going", 0x123041, 0x00F7 },
{ "hey how s the family doing", 0x04ac25, 0x0020 },
{ "hey i just wanted to introduce myself", 0x112040, 0xC0FF },
{ "hey i m getting over someone okay a little compassion over here", 0x04abfe, 0x0010 },
{ "hey market ll be open in the morning come by then", 0x020d3c, 0x0080 },
{ "hey thanks", 0x1263b1, 0x0020 },
{ "hey thanks for stopping by", 0x18e9f9, 0xC0FF },
{ "hey thanks for your help", 0x05e553, 0xF0FF },
{ "hey there", 0x194324, 0x0004 },
{ "hey there trading", 0x0dc91e, 0x0002 },
{ "hey there welcome back", 0x18f9db, 0x0004 },
{ "hey this paranoia s what keeps us alive", 0x190dff, 0x0008 },
{ "hey wanna make a deal", 0x1ac0df, 0x3000 },
{ "how s it going friend", 0x192340, 0x0004 },
{ "howdy", 0x06202b, 0x000D },
{ "howdy friend grab a seat by the fire everyone s welcome", 0x1a95a5, 0x0004 },
{ "i can help but not for free you ll need to come back when you have the caps you need anything else", 0x0c065d, 0xC0FF },
{ "i can t believe they re dead", 0x1905ab, 0x0004 },
{ "i don t know how they carry on after something like that", 0x18baa6, 0xC0FF },
{ "i don t think we ve met what can i do to help out", 0x112043, 0xC0FF },
{ "i heard there s all sorts of scary monsters in the commonwealth", 0x1ac59e, 0x3000 },
{ "i heard we had a trader in today", 0x111f58, 0x00FF },
{ "i just wanted to provide for my family", 0x133c08, 0x0004 },
{ "i know you re probably busy", 0x1a4d29, 0x0040 },
{ "i ll do whatever it takes to earn back my family s trust", 0x164456, 0x0004 },
{ "i m just here for the caps", 0x06857a, 0x0030 },
{ "i m just so hungry all the time", 0x1153e0, 0xC0FF },
{ "i m not here to haggle you ll get whatever i feel like giving you got it now what s it gonna be", 0x1069f8, 0x0010 },
{ "i m not looking for trouble enough junk here for both of us", 0x0390ed, 0x00F0 },
{ "i m sorry i know how hard this has been", 0x18ba92, 0xC0FF },
{ "i m truly sorry", 0x18ba91, 0xC0FF },
{ "i m warning you back off", 0x0711cc, 0xCFFF },
{ "i should never have gotten my hopes up", 0x1abc3e, 0xC0FC },
{ "i think i ate too much", 0x123056, 0x00F7 },
{ "i think we should get a dog a big furry one that could scare off bandits but would sleep next to me and keep me warm", 0x1a70ac, 0x3000 },
{ "i ve lost everything", 0x19b41e, 0x003C },
{ "if we want things to get better we ve got to start helping each other", 0x11e29e, 0xC0FF },
{ "if you re here to stock up i ve got some supplies the junk you see laying around isn t for sale though", 0x161f06, 0x0010 },
{ "if you re here to trade let s trade", 0x106d05, 0x0020 },
{ "it ain t a lot but i ve got a few basic supplies ammo meds that sort of thing", 0x019a9b, 0x0080 },
{ "it s a good life we ve got here and we re grateful for it", 0x047811, 0x0004 },
{ "it s just horrible", 0x18baa5, 0xC0FF },
{ "it s late ya gonna buy somethin", 0x03d61a, 0x0008 },
{ "it s not everything we expected but we re willing to work hard to make this a home we can be proud of", 0x1909c9, 0xC0FF },
{ "it s not perfect of course but i think with some work we can make a go of it", 0x1909c8, 0xC0FF },
{ "jesus they re dead", 0x192d32, 0x0004 },
{ "just had to push didn t ya 50 caps take it or leave it", 0x11532e, 0x0004 },
{ "just move along", 0x0711cf, 0xC0FF },
{ "just so you know i m keepin my eye on you", 0x159c60, 0x3000 },
{ "just stop okay we got our own problems", 0x022ead, 0x00FC },
{ "keep moving", 0x0711cb, 0xCFFF },
{ "keep walkin", 0x01f97d, 0x00FC },
{ "kids gotta earn their keep too", 0x17fa6e, 0x3000 },
{ "last warning", 0x0842ac, 0xCFFF },
{ "leave me alone", 0x03247b, 0xC0FF },
{ "leave now or else", 0x0711ce, 0xC0FF },
{ "leave us alone", 0x1ac039, 0x3000 },
{ "look just leave me alone", 0x022eb0, 0x00FC },
{ "lookin to make a deal", 0x144f2e, 0x0080 },
{ "looking to buy", 0x076aad, 0xC0FF },
{ "man what i wouldn t give for some snack cakes right now", 0x1ac0e7, 0x3000 },
{ "mom says it s too dangerous to play outside", 0x1ac59f, 0x3000 },
{ "my back hurts my feet hurt everything hurts", 0x1569ff, 0xC0FF },
{ "my friend is all better now being sick sucks", 0x1ac5a6, 0x3000 },
{ "my heart goes out to those folks", 0x18baa4, 0xC0FF },
{ "need armor", 0x19234e, 0xC0FF },
{ "nice to have some good news around here for a change", 0x15f083, 0xC0FF },
{ "no caps no room", 0x1263db, 0x0004 },
{ "no loitering", 0x018791, 0x0F00 },
{ "no one messes with us and gets away with it", 0x1ac0ef, 0x3000 },
{ "not a day goes by that i don t thank my lucky stars that i live here", 0x06202a, 0x000D },
{ "nothing really pretty quiet", 0x192333, 0xC0FF },
{ "oh i m sorry i probably shouldn t have asked i hope i haven t re opened any old wounds", 0x070db4, 0x0080 },
{ "one way of looking at it the other is to be thankful so many people passed", 0x061ff0, 0x000D },
{ "people are having to sleep in shifts it s making everyone a bit cranky", 0x1153e8, 0xC0FF },
{ "running from something welcome home", 0x03459d, 0xC038 },
{ "she s she s dead", 0x032098, 0xC0FF },
{ "so do we have a deal", 0x182964, 0x0004 },
{ "so that s it huh", 0x034141, 0xCFFF },
{ "stay out of trouble", 0x0d3fad, 0x0F00 },
{ "step right up all the clothing fit to wear", 0x115ce5, 0xC0FF },
{ "tattle tale tattle tale", 0x10744f, 0x3000 },
{ "the farm s not much but it s something", 0x1569fd, 0xC0FF },
{ "things are a bit crowded a few more beds would lift everyone s spirits", 0x1153e9, 0xC0FF },
{ "things have almost returned to normal i m grateful for that", 0x18f9d6, 0x0080 },
{ "things were pretty lean for a while but now we ve got plenty to trade interested", 0x16a793, 0x0080 },
{ "this would feel a lot more like a home if everyone had their own bed", 0x1909c1, 0xC0FF },
{ "told you to move along find somewhere else to gawk", 0x15376d, 0x0F00 },
{ "two caps each", 0x018f4b, 0x0010 },
{ "up for a trade i got toys for all ages", 0x1ac0e1, 0x3000 },
{ "watch out around here if they catch you having fun they ll make you do boring science", 0x14e677, 0x3000 },
{ "we just wanted to say how grateful we are for the opportunity", 0x1909c6, 0xC0FF },
{ "we knew the risks of having kids these days and we took em", 0x0a0076, 0x0080 },
{ "we ve got some supplies i can offer if you re interested", 0x047813, 0x0080 },
{ "we were looking for someplace to make a new life so here we are", 0x1909ab, 0xC0FF },
{ "welcome back looking to trade", 0x020d3b, 0x0080 },
{ "well hey there pup", 0x08558b, 0xC0FF },
{ "well hopefully tonight will be better", 0x18ba82, 0xC0FF },
{ "well i guess there s nothing for it but to do our best", 0x05e5c3, 0xC0FF },
{ "well you re home now and you should relax", 0x1ac7ed, 0x0008 },
{ "what i wouldn t give for some real time off", 0x111f56, 0x00FF },
{ "what ll ya have", 0x16cc50, 0xC0FF },
{ "what re ya drinking", 0x16cc4e, 0xC0FF },
{ "what you got a staring problem", 0x0179d3, 0x0F00 },
{ "whatever i got better stuff to do", 0x1ac0d6, 0x3000 },
{ "when i grow up i m gonna have my own farm and make other people do all the work", 0x159c5f, 0x3000 },
{ "why s it gotta be so quiet", 0x034129, 0xC0FF },
{ "woah is it morning already", 0x1a95f8, 0xC0FC },
{ "working on a farm is really boring", 0x159c5b, 0x3000 },
{ "yeah it s gonna take my son a while to get off the chems but we ll make it we always do", 0x106ce4, 0x0020 },
{ "yeah sure whatever i ve got left i m happy to trade for", 0x1a95af, 0x0040 },
{ "yep we re all still here thank god", 0x106e3f, 0x0004 },
{ "you cause trouble there s gonna be trouble got it", 0x018221, 0x0F00 },
{ "you got bricks for ears i said get the hell out of here now", 0x153736, 0x0F00 },
{ "you know the score friend if you ve got the caps we can talk", 0x1642c8, 0x0001 },
{ "you re botherin me", 0x01f97c, 0x00FC },
{ "you re looking at em but i ll sell you a stake in it hundred caps wait no fifty ten", 0x1830f8, 0xC0FF },
{ "your loss", 0x122234, 0x3000 },
        };

    inline VoiceSet CoverageFor(std::string_view a_text) noexcept
    {
        const auto key = Normalize(a_text);

        for (const auto& entry : kCoverageTable)
        {
            if (key == entry.Normalized)
            {
                return entry.Coverage;
            }
        }

        return 0;   // unknown line — no recording, caption-only
    }

    //-------------------------------------------------------------------------
    // FormIdFor — the catalog line's TESTopicInfo form id (the audio
    // probe's bridge: INFO -> parentTopic -> Say). Keyed by the same
    // normalized text as CoverageFor; an unknown line (a player's custom
    // INI line) has no form id — 0 — and can never be voiced.
    //-------------------------------------------------------------------------
    inline std::uint32_t FormIdFor(std::string_view a_text) noexcept
    {
        const auto key = Normalize(a_text);

        for (const auto& entry : kCoverageTable)
        {
            if (key == entry.Normalized)
            {
                return entry.FormId;
            }
        }

        return 0;   // unknown line — no INFO form id recorded
    }

    //-------------------------------------------------------------------------
    // VoiceBit — the table's bit for one voice, and whether a set
    // contains it.
    //-------------------------------------------------------------------------
    inline VoiceSet VoiceBit(Voice a_voice) noexcept
    {
        return static_cast<VoiceSet>(1u << static_cast<unsigned>(a_voice));
    }

    inline bool CanSpeak(VoiceSet a_coverage, Voice a_voice) noexcept
    {
        return (a_coverage & VoiceBit(a_voice)) != 0;
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

    //-------------------------------------------------------------------------
    // PickForVoice — the voice-aware picker (0.8.7): only draws lines
    // the speaker's voice bank actually recorded, so the audio layer
    // never plays a wrong-voice line. A pool where the voice has
    // nothing is silence — the mute rule: if the game doesn't provide
    // the line for that voice, the mind stays quiet (captions still
    // carry the story). The seed folds the voice too, so two voices of
    // the same mind disagree. Deterministic like Pick.
    //-------------------------------------------------------------------------
    inline std::string PickForVoice(
        const DialoguePool& a_pool,
        Pool a_category, EntityId a_id, std::uint64_t a_day,
        Voice a_voice) noexcept
    {
        const auto& lines = Lines(a_pool, a_category);

        if (lines.empty())
        {
            return {};
        }

        const auto voiceBit = VoiceBit(a_voice);

        // Collect the indices this voice can actually say — coverage 0
        // (a custom INI line, or a 1v/2v line whose single voice we
        // can't name) is caption-only and never picked for audio.
        std::vector<std::size_t> sayable;
        sayable.reserve(lines.size());

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if ((CoverageFor(lines[i]) & voiceBit) != 0)
            {
                sayable.push_back(i);
            }
        }

        if (sayable.empty())
        {
            return {};   // no recording for this voice — stay mute
        }

        std::uint64_t seed = a_id.Value();
        seed ^= a_day * 0x9E3779B97F4A7C15ull;
        seed ^= static_cast<std::uint64_t>(a_category) * 0xC2B2AE3D27D4EB4Full;
        seed ^= static_cast<std::uint64_t>(a_voice) * 0x9E3779B97F4A7C15ull;

        seed += 0x9E3779B97F4A7C15ull;
        seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ull;
        seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBull;
        seed ^= seed >> 31;

        return lines[sayable[seed % sayable.size()]];
    }
}
