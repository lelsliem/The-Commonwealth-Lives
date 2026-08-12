//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Words first; fists when the feud demands it.                              //
//                                                                             //
//=============================================================================//

#pragma once

#include "Bonds.h"
#include "Gossip.h"
#include "ConflictGates.h"
#include "WorldFacts.h"

#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Simulation.h"

namespace TLC::Fights
{
    using namespace LCE::Simulation;

    //-------------------------------------------------------------------------
    // Fights (0.7.5) — the physical escalation. The row (0.7.2) is the
    // feud's audible half; a fight is its physical one. When rivals and
    // enemies row at the bench and tempers flare, an altercation can
    // escalate: the aggressor lands a punch (the engine's Combat
    // channel — an unprompted wrong, −0.25 each way, deeper than the
    // row's wrong because violence is worse than words), the settlement
    // hears the blows (gossip), the radio carries it (news), and the
    // victim carries a threat memory — the engine's danger-awareness
    // names the strongest remembered fight as a thing to flee, so the
    // feud's victim starts avoiding the aggressor on sight. Pure: no
    // game types, testable without the game, like Rows and Gossip.
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // AlreadyFoughtToday — the pair has come to blows on this day. The
    // gate is the adapter's ConflictGates map (0.7.5 fix): a Combat
    // memory cannot gate it — memory fades (a weight-1.0 event erases
    // itself in seconds under the default sim.memory.fade), so the
    // gate re-armed mid-day and pairs re-fought every ~10s — the fight
    // waves and double-shoves of the first 0.7.5 build. The map is
    // day-scoped, O(1), and co-saved (v7), so save/load never
    // double-fights.
    //-------------------------------------------------------------------------
    inline bool AlreadyFoughtToday(
        const ConflictGates::Map& a_gates,
        EntityId a_entityA, EntityId a_entityB,
        std::uint64_t a_day) noexcept
    {
        return ConflictGates::FoughtToday(
            a_gates, a_entityA, a_entityB, a_day);
    }

    //-------------------------------------------------------------------------
    // RollFight — the escalation decision, pure. Three gates, all must
    // pass, and the third is a coin the world flips:
    //   1. The feud line is crossed — only enemies trade blows. A rival
    //      row stays verbal (the design's verbal-first rule: fights are
    //      rare and earned, or every rival pair becomes a brawl and the
    //      world turns into a tavern).
    //   2. The aggressor's temper is at or above the line
    //      (sim.fight.temper, the same JitteredTraits shape as the
    //      slight's sim.slight.temper) — the churlish throw the punch;
    //      a calm mind swallows the insult.
    //   3. The chance roll lands under sim.fight.chance. 1.0 forces
    //      every eligible escalation — the test knob.
    //-------------------------------------------------------------------------
    inline bool RollFight(
        Bonds::BondKind a_kind,
        float a_aggressorTemper,
        float a_temperLine,
        float a_chance,
        float a_roll01)
    {
        if (a_kind != Bonds::BondKind::Enemy)
        {
            return false;
        }

        if (a_aggressorTemper < a_temperLine)
        {
            return false;
        }

        return a_roll01 < a_chance;
    }

    //-------------------------------------------------------------------------
    // BookFight — the fight happens. Both remember the Combat (the
    // decided unprompted-wrong channel; the engine's Combat kind always
    // subtracts — the feud deepens on both sides, and each crosses a
    // line the instant it does, publishing RelationshipChangedEvent on
    // the bus), the settlement hears the blows (SpreadBond gossip —
    // the feud's face becomes a known face), and the fight serves the
    // aggression the row's wrong already started. Returns whether the
    // fight was booked (false when the pair is not an enemy feud or has
    // already fought today).
    //-------------------------------------------------------------------------
    inline bool BookFight(
        EntityRegistry& a_registry,
        const Bonds::BondMap& a_bonds,
        ConflictGates::Map& a_gates,
        EntityId a_aggressor, EntityId a_victim,
        std::uint64_t a_day,
        const SimulationTuning& a_tuning,
        LCE::Events::EventBus* a_events)
    {
        if (!a_aggressor.IsValid() || !a_victim.IsValid()
            || a_aggressor == a_victim)
        {
            return false;
        }

        const auto kind =
            Bonds::CurrentKind(a_bonds, a_aggressor, a_victim);

        if (kind != Bonds::BondKind::Enemy)
        {
            return false;
        }

        // Blows once a day — the pair already fought today.
        if (AlreadyFoughtToday(a_gates, a_aggressor, a_victim, a_day))
        {
            return false;
        }

        Remember(
            a_registry, a_aggressor,
            MemoryEvent{
                a_victim, InteractionKind::Combat,
                WorldFacts::kFactWeight },
            a_tuning, WorldTime{ a_day }, a_events);

        Remember(
            a_registry, a_victim,
            MemoryEvent{
                a_aggressor, InteractionKind::Combat,
                WorldFacts::kFactWeight },
            a_tuning, WorldTime{ a_day }, a_events);

        // The settlement hears the blows: both faces spread through the
        // group's memory like a feud's formation gossip.
        Gossip::SpreadBond(
            a_registry, a_aggressor, a_victim,
            InteractionKind::Social, a_day);

        // Blows once a day — the gate closes for the pair until
        // tomorrow (the day rolls by comparison, never a reset pass).
        ConflictGates::MarkFought(
            a_gates, a_aggressor, a_victim, a_day);

        return true;
    }
}
