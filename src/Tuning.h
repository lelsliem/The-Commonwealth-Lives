//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Open at eight, closed at eight — unless you say otherwise.                //
//                                                                             //
//=============================================================================//

#pragma once

#include "Behaviour.h"
#include "Illness.h"
#include "WorldFacts.h"

#include "LCE/Config/Configuration.h"

#include <cctype>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>

namespace TLC::Tuning
{
    //-------------------------------------------------------------------------
    // Tuning — the modder's knob (0.5.0). One text file next to the DLL,
    // `Data\F4SE\Plugins\TheLivingCommonwealth.ini`:
    //
    //     ; The Living Commonwealth tuning
    //     sim.memory.fade = 0.2        ; feeds the core (FromConfiguration)
    //     sim.hunger.decay = 0.001     ; the adapter's own keys ride along
    //     market.open.hour = 8
    //     market.close.hour = 20
    //
    // Unknown keys are ignored (the core's rule) so one file serves both
    // sides. Missing, empty, or unparsable values keep the default — a
    // broken line must never break the world. Pure here: parsing and the
    // settings read are free of the game; only the file IO is the edge's.
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // ParseConfig
    //
    // Parses the file text into the core's Configuration service. One
    // `key = value` per line; blank lines and lines starting with `;` or
    // `#` are skipped; surrounding whitespace is trimmed. A malformed
    // line (no `=`) is skipped, not fatal.
    //-------------------------------------------------------------------------
    inline LCE::Config::Configuration ParseConfig(std::string_view a_text)
    {
        LCE::Config::Configuration config;

        std::size_t start = 0;

        while (start <= a_text.size())
        {
            auto end = a_text.find('\n', start);

            if (end == std::string_view::npos)
            {
                end = a_text.size();
            }

            auto line = a_text.substr(start, end - start);

            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }

            const auto trim = [](std::string_view s)
            {
                while (!s.empty() && std::isspace(
                    static_cast<unsigned char>(s.front())))
                {
                    s.remove_prefix(1);
                }

                while (!s.empty() && std::isspace(
                    static_cast<unsigned char>(s.back())))
                {
                    s.remove_suffix(1);
                }

                return s;
            };

            line = trim(line);

            if (!line.empty() && line.front() != ';' && line.front() != '#')
            {
                const auto equals = line.find('=');

                if (equals != std::string_view::npos)
                {
                    const auto key = trim(line.substr(0, equals));
                    const auto value = trim(line.substr(equals + 1));

                    if (!key.empty())
                    {
                        config.Set(key, value);
                    }
                }
            }

            if (end == a_text.size())
            {
                break;
            }

            start = end + 1;
        }

        return config;
    }

    // The feeling rhythm (0.6.0 Stone 2): how fast dispositions and
    // trusts decay toward neutral when no experience refreshes them.
    // The core's default (0.05/s, half-life ~14 s) was tuned for a fast
    // demo — a shared meal's warmth is erased within a minute, so no
    // relationship can ever accumulate into a bond. The adapter's world
    // runs the same slow clock the shipped INI sets (sim.drift.rate):
    // half-life ~58 real minutes, so a bench-meal keeps its glow and
    // repeated meetings actually add up. A meal every ~8 min (0.002
    // hunger) retains ~90% of its warmth; four shared meals cross the
    // friend line. Lower = feelings last longer (bonds form faster);
    // higher = closer to the old demo.
    inline constexpr float kLivingDriftRate = 0.0002f;

    //-------------------------------------------------------------------------
    // AdapterSettings
    //
    // The adapter's own keys — the ones the core's FromConfiguration does
    // not know. Defaults are the pre-tuning constants (the WorldFacts
    // hours; the Behaviour.h NeedRates); the config file overrides them.
    // The `sim.*.decay` keys share the sim.* prefix with the core's keys
    // but belong to the adapter: the core decays a need by its own rate
    // (need.DecayRate), it never configures one — the seeded rhythm is
    // the adapter's, so it reads its own. One file, two readers.
    //-------------------------------------------------------------------------
    struct AdapterSettings
    {
        float MarketOpenHour = WorldFacts::kMarketOpenHour;
        float MarketCloseHour = WorldFacts::kMarketCloseHour;

        // The seeded need rhythm. A need empties in ~1/rate seconds:
        // 0.1/s is the tuning stone's default (a fast demo); "a few
        // meals a day" is ~0.001–0.005/s (the math is in Tuning.md).
        NeedRates Rates;

        // The trader's half of a sale (the trade stone): how much a
        // stall-keeper's disposition toward a customer warms per sale.
        // Matches the core's DispositionGain default (0.1) so a sale
        // warms like good company.
        float SaleWarmth = 0.1f;

        // The sleep cycle (0.6.0): how fast a resting mind recovers the
        // needs a nap fixes — Fatigue, Safety, and Comfort — per second
        // of simulation time. 0.2/s fills a drained need in ~5 s of rest
        // — a nap, a night. Without it, a fed mind with drained needs
        // parks: the need loop only ever decays, and only Hunger is
        // restored (on the meal). Safety is recovered here too — a
        // drained Safety with no threat parks the mind outright (the
        // engine returns nullopt, the intent is removed; 2026-08-11).
        float RestRecovery = 0.2f;

        // How many settlers may walk at once (sim.walk.cap). The
        // command-mode travel package flags each walker as commanded;
        // hundreds at once risks a flood, so the issued walks are
        // capped. Arrival ends a session and frees its slot immediately,
        // so the default 16 is generous — but big saves can raise it
        // without a rebuild. Was a constant (16) until the review pass;
        // the starved-world hunt proved a silent hard cap is a trap.
        std::size_t WalkCap = 16;

        // The physical exchange (the economy stone): a meal's price in
        // caps. A buyer pays what they can afford up to this; the rest
        // the settlement covers. 5 caps, a modest market.
        float MealPrice = 5.0f;

        // The meal-cadence wander (0.6.0 Stone 3.75): how often a
        // resting or exploring mind is re-commanded to a real nearby
        // reference (seconds — re-issuing mid-walk would yank the
        // actor, so the cooldown must exceed a short walk) and how far
        // the wander may range (game units, ~1.4 cm each — 4000 is a
        // settlement-sized circle around the actor).
        float WanderCooldown = 30.0f;
        float WanderRadius = 4000.0f;

        // The grief arc (0.6.0 Stone 5): how much faster a grieving
        // mind's Social need empties, per second — they seek company.
        // 0.01/s is a quiet ache; higher makes the bereaved visibly
        // restless.
        float GriefDecay = 0.01f;

        // The feud arc (0.6.0 Stone 5): whether a settlement that has
        // heard of a feud may try to mediate it (once per pair per day).
        bool MediationEnabled = true;

        // The birth stone (0.6.0 Stone 6, experimental): off by default.
        // When on, a spouse household may have a child — a sim-only mind
        // (no game actor), fed by the household, bonded to both parents,
        // living in the co-save.
        bool BirthEnabled = false;

        // The conception chance (0.7.7): each eligible spouse pair
        // rolls once per sim day; this is the probability [0,1]. 0.05
        // means ~1 birth per 20 couple-days — a slow trickle, not a
        // baby boom. Higher values flood settlements quickly.
        float BirthChance = 0.05f;

        // The gestation window (0.7.7): sim-days between conception
        // and birth. Short enough to watch a full cycle in testing
        // (set to 1 for instant births); long enough to feel like a
        // pregnancy at the default (3 = ~3 in-game days).
        float BirthGestation = 3.0f;

        // The childhood duration (0.7.7): sim-days a child stays a
        // child before growing into a walking adult mind. Longer than
        // gestation — a childhood is a life, not a window.
        float BirthChildhood = 10.0f;

        // The visible-children flag (0.7.8): when on and the external
        // baby mod ("Baby Sim - Babies That Grow Up") is loaded, a
        // grown child pairs with a real game actor — it walks, trades,
        // and bonds like any mind. When off (the default) or the mod
        // is absent, children stay sim-only (0.7.7 behavior).
        bool BirthVisible = false;

        // The illness stone (0.8.0 — Illness & Medicine): the whole
        // curve and every vector. The adapter owns a Health component
        // per mind (co-save additive); these keys tune the price of
        // being in the wastes.
        bool IllnessEnabled = true;
        IllnessSettings Illness;


        // The conflict source (0.7.0 Stone 2): the temper line. A mind
        // whose temperament (Behaviour.h TemperOf, ~0.8–1.2 around 1.0)
        // is at or above this blames the stall-keeper when a hungry
        // arrival finds the market shut — the slight that can cross the
        // rival and enemy lines and begin a feud. Below the line, a warm
        // mind forgives (the stall was just shut — no one to blame).
        float SlightTemper = 1.0f;

        // The fight's escalation (0.7.5 Fights): when an enemy pair
        // rows (or a slighted mind faces an enemy keeper), the physical
        // roll. Chance is the coin — 0.1 keeps fights rare (verbal-first
        // is the rule), 1.0 forces every eligible escalation (the test
        // knob). Temper is the aggressor's line, the same JitteredTraits
        // shape as the slight's: at or above it, the churlish throw the
        // punch; below it, they swallow the insult.
        float FightChance = 0.1f;
        float FightTemper = 1.0f;

        // The punch's shove (0.7.5 polish): the victim is knocked back
        // The fall's force (0.7.6, ADR-0050): the paired push IS the
        // shove — the victim is carried by the animation, so this
        // force only has to put them DOWN. 3 is the tip-over zone the
        // loop tests measured: it knocks the victim over in place
        // without the ground-slide that force 6–9 dragged (the
        // "ghost push" the field tests saw after the kick). Magnitude
        // in game units; above 10 is the ragdoll launch zone. The
        // jitter (capped at 1.15×) spreads it 2.6–3.5. 0 turns the
        // fight's physics off (the fight books without the fall).
        float FightPush = 3.0f;

        // The standing shove's flinch (0.7.5 polish): the game's melee
        // hit-reaction — the same stagger that plays when a punch
        // actually lands (Actor::DoHitMe with a zero-damage HitData).
        // KnockExplosion alone collapses the victim in place with no
        // visible push — the loop tests proved force alone never reads
        // as a shove; the flinch plays first, so the victim stumbles
        // back, then the fall reads as the punch's consequence.
        // Magnitude: 1 small, 2 medium, 3 large, 4 extra-large. 0
        // turns the flinch off (the fall still lands).
        int FightStagger = 2;

        // The flinch's push-back (0.7.5 polish): the linear impulse
        // behind the stagger — how far the victim stumbles before the
        // fall, in the game's units. Tuned so the stumble reads but
        // never launches. 0 = flinch in place.
        float FightPushBack = 25.0f;

        // The fall's beat (0.7.5 polish): seconds between the flinch
        // and the knock-down. The flinch and the fall MUST NOT land in
        // the same frame — the knock overrides the stagger animation
        // before it is visible, and the punch reads as a silent
        // collapse (ADR-0043). A short beat lets the stagger play,
        // then the fall lands as its consequence.
        float FightFallDelay = 0.9f;

        // The scuffle's beat (0.7.5): a hot-headed victim answers the
        // punch after a beat, not in the same instant — push, fall, get
        // up, push back. Seconds between the two shoves. Tuned so the
        // get-up animation has time to finish before the answer lands
        // (0.7.5 field: the fall is ~1s, the get-up a couple more — a
        // 4s beat blurred the two; 7s reads as a real pause).
        float RetaliationDelay = 7.0f;

        // The parting beat (0.7.5 field): seconds between the
        // counter-fall and the loser walking off. The loser must GET UP
        // before fleeing — a same-instant walk-off was a ghost sprint
        // from the ground. 3s lets the get-up play, then the slink.
        float FightPartDelay = 3.0f;

        // The test hook (0.7.5): sim.test.forceFight pins two minds by
        // base form id (low 24 bits) and brawls them on a loop every
        // ForceFightInterval seconds — the full fight machinery (shove,
        // retaliation, news, gossip) on demand, the pair pinned to an
        // enemy feud and the once-per-day gate bypassed, for spectating
        // and testing without waiting on the sim's coin. Off by default.
        std::uint32_t ForceFightA = 0;
        std::uint32_t ForceFightB = 0;
        float ForceFightInterval = 15.0f;

        // The player window (0.7.0 Stone 3): the news feed. Events go
        // on-screen as HUD notifications, throttled by NewsCooldown
        // seconds — a flood of lines is noise, not news. The feed is the
        // settlement radio's story (RadioCaptionEvery seconds between
        // captions while a radio is near, within RadioRadius units).
        bool NewsEnabled = true;
        float NewsCooldown = 5.0f;
        float RadioCaptionEvery = 45.0f;
        float RadioRadius = 3000.0f;

        // The loud line's hearing range (0.7.5 Fights): a spoken fight
        // line shows as a bottom-of-screen subtitle only when the
        // player is within this many game units of the speaker — a
        // nearby brawl is loud, a cross-settlement squabble is not.
        float SubtitleRadius = 500.0f;

        // The settlement radio's base form. The default is the workshop
        // "Radio" — a hardcoded FormID, so it is flagged for xEdit
        // verification before it is trusted (the verify ritual); the key
        // exists so a wrong pin is a config line, never a rebuild.
        std::uint32_t RadioBaseFormId = 0x0001A17D;

        // The decision log cadence (0.7.4 verification fix): a mind's
        // "decides X" line is logged when its intent changes, or at most
        // once per this many seconds while it flip-flops between near-tied
        // intents (Rest/Explore tie-breaks re-roll every frame). The
        // verify channel stays readable — one line per mind per second at
        // most — instead of a 150-line-per-second file-I/O flood on the
        // game thread (the 22k-line session that preceded the silent
        // freeze).
        float LogDecisionEvery = 1.0f;

        // The walk probe gate (0.8.0 polish): the per-walker "d = ..."
        // probe is the log's biggest single contributor (2.2k lines of a
        // 12k session). On while the sim is in development — the probe is
        // the verification eyes — and off for release (the arrival and
        // session-end lines stay; only the 2-second progress heartbeats
        // go).
        bool LogWalkProbes = true;
    };

    inline AdapterSettings AdapterSettingsFrom(
        const LCE::Config::Configuration& a_config)
    {
        AdapterSettings settings;

        const auto read = [&a_config](std::string_view key, float fallback)
        {
            const auto raw = a_config.Get(key);

            if (raw.empty())
            {
                return fallback;
            }

            try
            {
                return std::stof(std::string(raw));
            }
            catch (const std::exception&)
            {
                return fallback;
            }
        };

        settings.MarketOpenHour =
            read("market.open.hour", settings.MarketOpenHour);
        settings.MarketCloseHour =
            read("market.close.hour", settings.MarketCloseHour);

        settings.Rates.Hunger = read("sim.hunger.decay", settings.Rates.Hunger);
        settings.Rates.Fatigue =
            read("sim.fatigue.decay", settings.Rates.Fatigue);
        settings.Rates.Safety = read("sim.safety.decay", settings.Rates.Safety);
        settings.Rates.Social = read("sim.social.decay", settings.Rates.Social);
        settings.Rates.Comfort =
            read("sim.comfort.decay", settings.Rates.Comfort);

        settings.SaleWarmth = read("sim.sale.warmth", settings.SaleWarmth);
        settings.MealPrice = read("sim.meal.price", settings.MealPrice);
        settings.RestRecovery =
            read("sim.rest.recovery", settings.RestRecovery);

        settings.WanderCooldown =
            read("sim.wander.cooldown", settings.WanderCooldown);
        settings.WanderRadius =
            read("sim.wander.radius", settings.WanderRadius);

        settings.GriefDecay =
            read("sim.arc.grief.decay", settings.GriefDecay);
        settings.SlightTemper =
            read("sim.slight.temper", settings.SlightTemper);

        settings.FightChance =
            read("sim.fight.chance", settings.FightChance);
        settings.FightTemper =
            read("sim.fight.temper", settings.FightTemper);

        settings.FightPush =
            read("sim.fight.push", settings.FightPush);

        settings.FightStagger = static_cast<int>(
            read("sim.fight.stagger",
                static_cast<float>(settings.FightStagger)));
        settings.FightPushBack =
            read("sim.fight.pushback", settings.FightPushBack);
        settings.FightFallDelay =
            read("sim.fight.fall.delay", settings.FightFallDelay);

        settings.RetaliationDelay = read(
            "sim.fight.retaliation.delay", settings.RetaliationDelay);

        settings.FightPartDelay = read(
            "sim.fight.part.delay", settings.FightPartDelay);

        settings.ForceFightA = static_cast<std::uint32_t>(
            read("sim.test.forceFight.a",
                static_cast<float>(settings.ForceFightA)));
        settings.ForceFightB = static_cast<std::uint32_t>(
            read("sim.test.forceFight.b",
                static_cast<float>(settings.ForceFightB)));
        settings.ForceFightInterval = read(
            "sim.test.forceFight.interval", settings.ForceFightInterval);

        settings.NewsCooldown =
            read("sim.news.cooldown", settings.NewsCooldown);
        settings.RadioCaptionEvery =
            read("sim.radio.caption.every", settings.RadioCaptionEvery);
        settings.RadioRadius =
            read("sim.radio.radius", settings.RadioRadius);
        settings.SubtitleRadius =
            read("sim.subtitle.radius", settings.SubtitleRadius);

        settings.LogDecisionEvery =
            read("sim.log.decisions.every", settings.LogDecisionEvery);

        // The arc/birth/news toggles are bools, not rates — parse them
        // separately: "1", "true", "yes", "on" mean on; anything else
        // off (a broken line never breaks the world).
        const auto readBool = [&a_config](std::string_view key, bool fallback)
        {
            const auto raw = a_config.Get(key);

            if (raw.empty())
            {
                return fallback;
            }

            return raw == "1" || raw == "true"
                || raw == "yes" || raw == "on";
        };

        settings.MediationEnabled =
            readBool("sim.arc.mediation", settings.MediationEnabled);
        settings.BirthEnabled =
            readBool("sim.birth.enabled", settings.BirthEnabled);
        settings.BirthChance =
            read("sim.birth.chance", settings.BirthChance);
        settings.BirthGestation =
            read("sim.birth.gestation", settings.BirthGestation);
        settings.BirthChildhood =
            read("sim.birth.childhood", settings.BirthChildhood);
        settings.BirthVisible =
            readBool("sim.birth.visible", settings.BirthVisible);
        settings.LogWalkProbes =
            readBool("sim.log.probes", settings.LogWalkProbes);

        // The illness stone (0.8.0). The curve and the vectors are all
        // INI keys — a broken line keeps the default, never breaks the
        // world (the standing contract).
        settings.IllnessEnabled =
            readBool("sim.illness.enabled", settings.IllnessEnabled);
        settings.Illness.Hold =
            read("sim.illness.hold", settings.Illness.Hold);
        settings.Illness.FatigueMult =
            read("sim.illness.fatigueMult", settings.Illness.FatigueMult);
        settings.Illness.HungerMult =
            read("sim.illness.hungerMult", settings.Illness.HungerMult);
        settings.Illness.Recovery =
            read("sim.illness.recovery", settings.Illness.Recovery);
        settings.Illness.Duration =
            read("sim.illness.duration", settings.Illness.Duration);
        settings.Illness.RadstormChance = read(
            "sim.illness.radstormChance", settings.Illness.RadstormChance);
        settings.Illness.FoodChance = read(
            "sim.illness.foodChance", settings.Illness.FoodChance);
        settings.Illness.WoundChance = read(
            "sim.illness.woundChance", settings.Illness.WoundChance);
        settings.Illness.ContagionChance = read(
            "sim.illness.contagionChance", settings.Illness.ContagionChance);
        settings.Illness.MedicinePrice = read(
            "sim.illness.medicinePrice", settings.Illness.MedicinePrice);
        settings.Illness.SeverityRate = read(
            "sim.illness.severityRate", settings.Illness.SeverityRate);
        settings.Illness.ChildMult = read(
            "sim.illness.childMult", settings.Illness.ChildMult);
        settings.Illness.DeathSeverity = read(
            "sim.illness.deathSeverity", settings.Illness.DeathSeverity);
        settings.Illness.DeathDrain = read(
            "sim.illness.deathDrain", settings.Illness.DeathDrain);
        settings.Illness.CoughInterval = read(
            "sim.illness.coughInterval", settings.Illness.CoughInterval);

        settings.NewsEnabled =
            readBool("sim.news.enabled", settings.NewsEnabled);

        // The radio's base form is a hex form id, not a rate — parse it
        // separately ("0x1A17D" or "1A17D" or decimal; a broken line
        // keeps the default).
        const auto rawRadio = a_config.Get("radio.base.formid");

        if (!rawRadio.empty())
        {
            try
            {
                std::string value(rawRadio);

                if (value.rfind("0x", 0) == 0
                    || value.rfind("0X", 0) == 0)
                {
                    value = value.substr(2);
                }

                settings.RadioBaseFormId = static_cast<std::uint32_t>(
                    std::stoul(value, nullptr, 16));
            }
            catch (const std::exception&)
            {
                // keep the default — a broken line never breaks the world
            }
        }

        // The walk cap is a size, not a rate — parse it separately.
        const auto rawCap = a_config.Get("sim.walk.cap");

        if (!rawCap.empty())
        {
            try
            {
                settings.WalkCap =
                    static_cast<std::size_t>(std::stoul(std::string(rawCap)));
            }
            catch (const std::exception&)
            {
                // keep the default — a broken line never breaks the world
            }
        }

        return settings;
    }
}
