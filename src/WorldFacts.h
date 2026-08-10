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
        //-------------------------------------------------------------------------
        inline void ApplyFact(
            LCE::Simulation::Memory& a_memory,
            LCE::Simulation::InteractionKind a_kind,
            bool a_active) noexcept
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
            }
            else
            {
                a_memory.Events.push_back(LCE::Simulation::MemoryEvent{
                    {}, a_kind, kFactWeight });
            }
        }
    }
}
