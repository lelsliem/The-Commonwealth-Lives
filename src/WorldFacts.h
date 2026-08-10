//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Markets close; memories fade; hunger doesn't.                             //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/EntityId.h"
#include "LCE/Simulation/Memory.h"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace TLC
{
    //-------------------------------------------------------------------------
    // World facts — the doors the world shuts. The core reasons over
    // memories and never queries the game (ADR-0024); a world fact is a
    // memory event with an *invalid* Other: "the market is closed tonight"
    // is { invalid, Trade, weight }. While such a fact is remembered, the
    // core's Decide treats that interaction kind as unavailable — a hungry
    // mind explores instead of moving to the market — and when the fact
    // fades below the forget threshold, the door reopens. No game types
    // here: the adapter reads the clock and the sky at the edge, then
    // feeds this pure logic (testable without the game).
    //-------------------------------------------------------------------------
    namespace WorldFacts
    {
        // The market's trading hours — the first tuning keys. They become
        // `market.open.hour` / `market.close.hour` in the user-editable
        // tuning file (the tuning stone); constants until then. The market
        // is open from kMarketOpenHour (inclusive) to kMarketCloseHour
        // (exclusive) in game hours 0–24. Overnight hours work: open 20,
        // close 8 means the market trades all night and sleeps by day.
        inline constexpr float kMarketOpenHour = 8.0f;
        inline constexpr float kMarketCloseHour = 20.0f;

        // The weight every world fact is pushed with — the core's default
        // seed weight, the same as the market-location memory. The adapter
        // refreshes active facts back to this weight every second (the
        // refresh pattern, below), so a fact's *lifetime* is irrelevant
        // while the door is shut; what matters is how fast it reopens when
        // the condition flips: (Weight − ForgetThreshold) / MemoryFadeRate
        // = (1.0 − 0.1) / 0.2 = 4.5 s of close-down after the open hour.
        inline constexpr float kFactWeight = 1.0f;

        //-------------------------------------------------------------------------
        // The day's weather (0.5.x) — a memory, not a door.
        //
        // The sky is classified into a small set of categories, and each
        // category is pushed as a day-stamped world fact: { invalid,
        // WeatherRain, 1.0, day } means "day 12 was rainy". Decide never
        // gates these kinds — rain never closes the market — so they are
        // pure labels the sim remembers, and future recall ("it rained
        // the day the caravan arrived") reads them by kind + Day.
        //-------------------------------------------------------------------------

        // The categories. Unknown is the honest fallback: interiors,
        // editor records, and modded skies leave no fact — we do not
        // remember what we do not know.
        enum class WeatherKind
        {
            Unknown,
            Clear,
            Overcast,
            Rain,
            Fog,
            Misty,
            Radstorm
        };

        // ClassifyWeather — the verified live-weather forms (xEdit dump
        // 2026-08-10, Docs/WeatherForms.md). All are 00-prefixed vanilla
        // forms, stable across load orders; the editor backups are never
        // set at runtime and are deliberately absent.
        inline WeatherKind ClassifyWeather(std::uint32_t a_formId) noexcept
        {
            switch (a_formId)
            {
            case 0x0002B52A:   // CommonwealthClear
            case 0x001D670E:   // CommonwealthClearestSkies
            case 0x0012A18E:   // CommonwealthSanctuaryClear
                return WeatherKind::Clear;

            case 0x001C8556:   // CommonwealthOvercast
            case 0x000F1033:   // CommonwealthGSOvercast
                return WeatherKind::Overcast;

            case 0x001CA7E4:   // CommonwealthRain
                return WeatherKind::Rain;

            case 0x001C3473:   // CommonwealthFoggy
            case 0x001BD481:   // CommonwealthGSFoggy
                return WeatherKind::Fog;

            case 0x001CC186:   // CommonwealthMisty
            case 0x001CD096:   // CommonwealthMistyRainy
                return WeatherKind::Misty;

            case 0x001C3D5E:   // CommonwealthGSRadstorm
                return WeatherKind::Radstorm;

            default:
                return WeatherKind::Unknown;
            }
        }

        // WeatherFactKind — a category as the fact label the sim memory
        // carries. Unknown has none: nothing to remember.
        inline std::optional<LCE::Simulation::InteractionKind> WeatherFactKind(
            WeatherKind a_kind) noexcept
        {
            switch (a_kind)
            {
            case WeatherKind::Clear:
                return LCE::Simulation::InteractionKind::WeatherClear;
            case WeatherKind::Overcast:
                return LCE::Simulation::InteractionKind::WeatherOvercast;
            case WeatherKind::Rain:
                return LCE::Simulation::InteractionKind::WeatherRain;
            case WeatherKind::Fog:
                return LCE::Simulation::InteractionKind::WeatherFog;
            case WeatherKind::Misty:
                return LCE::Simulation::InteractionKind::WeatherMisty;
            case WeatherKind::Radstorm:
                return LCE::Simulation::InteractionKind::WeatherRadstorm;
            case WeatherKind::Unknown:
                break;
            }

            return std::nullopt;
        }

        // WeatherLabel — the category as the player reads it.
        inline const char* WeatherLabel(WeatherKind a_kind) noexcept
        {
            switch (a_kind)
            {
            case WeatherKind::Clear:
                return "clear";
            case WeatherKind::Overcast:
                return "overcast";
            case WeatherKind::Rain:
                return "rainy";
            case WeatherKind::Fog:
                return "foggy";
            case WeatherKind::Misty:
                return "misty";
            case WeatherKind::Radstorm:
                return "a radstorm";
            case WeatherKind::Unknown:
                break;
            }

            return "unclassified";
        }

        //-------------------------------------------------------------------------
        // IsMarketClosed
        //
        // Pure hour check with overnight wrap. a_openHour is inclusive,
        // a_closeHour exclusive. A wrap (close < open) means the market is
        // open across midnight: at 23:00 with hours 8–20 the market is
        // closed; with hours 20–8 it is open.
        //-------------------------------------------------------------------------
        inline bool IsMarketClosed(
            float a_hour, float a_openHour, float a_closeHour) noexcept
        {
            if (a_closeHour > a_openHour)
            {
                return a_hour < a_openHour || a_hour >= a_closeHour;
            }

            return a_hour < a_openHour && a_hour >= a_closeHour;
        }

        //-------------------------------------------------------------------------
        // HasFact
        //
        // The idempotent guard: does this mind already remember a world
        // fact of this kind? A fact is any memory event with an invalid
        // Other — nothing to meet, no one to trust — so { invalid, Trade }
        // and { market, Trade } are different memories with different
        // meanings (the door is shut vs. here is where the food is).
        // Mirrors the core's own IsUnavailable test, which the adapter
        // cannot call.
        //-------------------------------------------------------------------------
        inline bool HasFact(
            const LCE::Simulation::Memory& a_memory,
            LCE::Simulation::InteractionKind a_kind) noexcept
        {
            for (const auto& event : a_memory.Events)
            {
                if (!event.Other.IsValid() && event.Kind == a_kind)
                {
                    return true;
                }
            }

            return false;
        }

        //-------------------------------------------------------------------------
        // ApplyFact
        //
        // One door, one mind — the refresh pattern the adapter runs every
        // second. While a_active, the fact is remembered at full weight:
        // found, it is refreshed in place (the tick's fade erodes it, and
        // a refreshed fact never dies while the door is shut); missing, it
        // is pushed. Never duplicated — one fact event per kind, so memory
        // does not grow. When a_active is false the mind is left alone and
        // the core's tick fades the fact out: the designed reopen.
        //
        // a_day stamps the fact with the world day it was remembered
        // (0.5.0 WorldTime) — the weather facts use it ("day 12 was
        // rainy"); the gates pass 0 and stay unstamped. A refresh updates
        // the stamp too: re-seen today is remembered today.
        //-------------------------------------------------------------------------
        inline void ApplyFact(
            LCE::Simulation::Memory& a_memory,
            LCE::Simulation::InteractionKind a_kind,
            bool a_active,
            std::uint64_t a_day = 0) noexcept
        {
            if (!a_active)
            {
                return;
            }

            auto it = std::find_if(
                a_memory.Events.begin(), a_memory.Events.end(),
                [a_kind](const LCE::Simulation::MemoryEvent& a_event)
                {
                    return !a_event.Other.IsValid() && a_event.Kind == a_kind;
                });

            if (it != a_memory.Events.end())
            {
                it->Weight = kFactWeight;   // refresh — the door stays shut
                it->Day = a_day;
            }
            else
            {
                a_memory.Events.push_back(LCE::Simulation::MemoryEvent{
                    {}, a_kind, kFactWeight, a_day });
            }
        }
    }
}
