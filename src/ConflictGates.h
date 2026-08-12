//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Once a day — words, then blows. The gate that holds a feud to a          //
//   single scene per day.                                                     //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/Entity/EntityId.h"

#include <cstdint>
#include <map>
#include <utility>

namespace TLC::ConflictGates
{
    using namespace LCE::Simulation;

    //-------------------------------------------------------------------------
    // ConflictGates (0.7.5 fix) — the once-per-day gate for a feud's
    // scenes. The row (0.7.2) and the fight (0.7.5) both want "words
    // once a day per pair" and "blows once a day per pair". The first
    // build gated them on the pair's Wronged/Combat memory — but memory
    // fades (sim.memory.fade 0.2/s, forget at 0.1: a weight-1.0 event
    // erases itself in 4.5 seconds), so the gate re-armed mid-day and
    // pairs re-rowed and re-fought every ~10s forever — the fight
    // waves, the double-shoves, 445 fights in one session. The gate
    // cannot live in fading memory: "today" is a day-scoped fact, not a
    // salience. So the adapter owns a durable pair → day map instead —
    // explicit, O(1), rolled by the clock (a stored day != today means
    // the pair is free again), and co-saved (v7) so save/load never
    // double-rows or double-fights. Pure: no game types, testable
    // without the game.
    //-------------------------------------------------------------------------

    // The ordered pair key — same shape as Bonds::PairKey, so A-B and
    // B-A are one entry no matter who threw the punch.
    using Key = std::pair<std::uint64_t, std::uint64_t>;

    // A pair's gates: the last world day they had words (RowDay) and
    // the last world day they came to blows (FightDay). 0 = never.
    struct Gate
    {
        std::uint64_t RowDay = 0;
        std::uint64_t FightDay = 0;
    };

    using Map = std::map<Key, Gate>;

    inline Key PairKey(EntityId a_a, EntityId a_b) noexcept
    {
        const auto va = a_a.Value();
        const auto vb = a_b.Value();

        return va <= vb ? Key{ va, vb } : Key{ vb, va };
    }

    //-------------------------------------------------------------------------
    // The reads — the pair's gate for today. A stored day != today is
    // not "today", so the gate rolls over by comparison alone; stale
    // entries cost nothing until the pair meets again.
    //-------------------------------------------------------------------------
    inline bool RowedToday(
        const Map& a_gates, EntityId a_a, EntityId a_b,
        std::uint64_t a_day) noexcept
    {
        const auto it = a_gates.find(PairKey(a_a, a_b));

        return it != a_gates.end() && it->second.RowDay == a_day;
    }

    inline bool FoughtToday(
        const Map& a_gates, EntityId a_a, EntityId a_b,
        std::uint64_t a_day) noexcept
    {
        const auto it = a_gates.find(PairKey(a_a, a_b));

        return it != a_gates.end() && it->second.FightDay == a_day;
    }

    //-------------------------------------------------------------------------
    // The writes — the pair had words / came to blows today.
    //-------------------------------------------------------------------------
    inline void MarkRowed(
        Map& a_gates, EntityId a_a, EntityId a_b,
        std::uint64_t a_day)
    {
        a_gates[PairKey(a_a, a_b)].RowDay = a_day;
    }

    inline void MarkFought(
        Map& a_gates, EntityId a_a, EntityId a_b,
        std::uint64_t a_day)
    {
        a_gates[PairKey(a_a, a_b)].FightDay = a_day;
    }
}
