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

        settings.GriefDecay =
            read("sim.arc.grief.decay", settings.GriefDecay);

        // The arc/birth toggles are bools, not rates — parse them
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
