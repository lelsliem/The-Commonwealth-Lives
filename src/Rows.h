//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Words first — a feud is fought with a mouth before a fist.               //
//                                                                             //
//=============================================================================//

#pragma once

#include "Bonds.h"
#include "ConflictGates.h"
#include "Gossip.h"
#include "WorldFacts.h"

#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Simulation.h"

namespace TLC::Rows
{
    using namespace LCE::Simulation;

    //-------------------------------------------------------------------------
    // Rows (0.7.2) — the verbal altercation. The conflict source
    // (Identity.md) feeds a feud; a row gives it a scene: two rivals or
    // enemies who cross paths at the same bench have words. Each
    // remembers the other wronged them — the engine's unprompted-wrong
    // channel (Wronged, sim.disposition.loss −0.25), distinct from the
    // executed-interaction let-down of a shut stall (Social/Failure
    // −0.1). The settlement hears the shouting (gossip), and the wrong
    // can push the pair over the feud line. Pure: no game types,
    // testable without the game, like Gossip.
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // AlreadyRowedToday — the pair has had words on this day. The gate
    // is the adapter's ConflictGates map (0.7.5 fix): a Wronged memory
    // cannot gate it — memory fades (a weight-1.0 event erases itself
    // in seconds under the default sim.memory.fade), so the gate
    // re-armed mid-day and pairs re-rowed every ~10s. The map is
    // day-scoped, O(1), and co-saved (v7), so save/load never
    // double-rows.
    //-------------------------------------------------------------------------
    inline bool AlreadyRowedToday(
        const ConflictGates::Map& a_gates,
        EntityId a_entityA, EntityId a_entityB,
        std::uint64_t a_day) noexcept
    {
        return ConflictGates::RowedToday(
            a_gates, a_entityA, a_entityB, a_day);
    }

    //-------------------------------------------------------------------------
    // Exchange — the crossing row. Both minds remember the other wronged
    // them (the decided Wronged channel), and the settlement hears the
    // shouting (SpreadBond gossip, so a feud in progress becomes a known
    // face — the 0.7.0 mediation backstop's raw material). The wrongs
    // publish on the bus, so a crossing that pushes the pair over the
    // enemy line fires OnBondChange the instant it happens. Returns
    // whether words were exchanged.
    //-------------------------------------------------------------------------
    inline bool Exchange(
        EntityRegistry& a_registry,
        const Bonds::BondMap& a_bonds,
        ConflictGates::Map& a_gates,
        EntityId a_entityA, EntityId a_entityB,
        std::uint64_t a_day,
        const SimulationTuning& a_tuning,
        LCE::Events::EventBus* a_events)
    {
        if (!a_entityA.IsValid() || !a_entityB.IsValid()
            || a_entityA == a_entityB)
        {
            return false;
        }

        // A row needs a feud: only rivals and enemies trade words. A
        // first meeting is the slight's business (the shut stall); a
        // friend's greeting is the greet pool's business.
        const auto kind = Bonds::CurrentKind(a_bonds, a_entityA, a_entityB);

        if (kind != Bonds::BondKind::Rival
            && kind != Bonds::BondKind::Enemy)
        {
            return false;
        }

        // Words once a day — the pair already had words today.
        if (AlreadyRowedToday(a_gates, a_entityA, a_entityB, a_day))
        {
            return false;
        }

        // Both directions: each remembers the other wronged them —
        // unprompted, full loss (the engine's decided Wronged channel).
        Remember(
            a_registry, a_entityA,
            MemoryEvent{
                a_entityB, InteractionKind::Wronged,
                WorldFacts::kFactWeight },
            a_tuning, WorldTime{ a_day }, a_events);

        Remember(
            a_registry, a_entityB,
            MemoryEvent{
                a_entityA, InteractionKind::Wronged,
                WorldFacts::kFactWeight },
            a_tuning, WorldTime{ a_day }, a_events);

        // The settlement hears the shouting: both faces spread through
        // the group's memory like a feud's formation gossip.
        Gossip::SpreadBond(
            a_registry, a_entityA, a_entityB,
            InteractionKind::Social, a_day);

        // Words once a day — the gate closes for the pair until
        // tomorrow (the day rolls by comparison, never a reset pass).
        ConflictGates::MarkRowed(
            a_gates, a_entityA, a_entityB, a_day);

        return true;
    }
}
