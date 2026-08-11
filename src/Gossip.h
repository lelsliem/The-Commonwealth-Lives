//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   News travels fast when everyone knows everyone.                           //
//                                                                             //
//=============================================================================//

#pragma once

#include "WorldFacts.h"

#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Simulation.h"

namespace TLC
{
    //-------------------------------------------------------------------------
    // Gossip — the settlement's news (0.6.0 Stone 4). A bond forms, a
    // feud starts, someone dies — and every mind of the settlement knows
    // it. Pure: given the registry and a subject, write the fact into
    // every other mind's memory. No game types; the adapter calls this
    // from the bond-change handler and the death bookkeeping. Testable
    // without the game, like WorldFacts.
    //
    // The "gossip radius" is the settlement itself: every mind here
    // shares one book. Strangers and fresh arrivals don't know — a mind
    // seeded after the fact simply never received it (gossip is
    // written once, it is not replayed).
    //-------------------------------------------------------------------------
    namespace Gossip
    {
        //-------------------------------------------------------------------------
        // Spread — every OTHER mind (with a Memory) remembers the subject:
        // { a_subject, a_kind, weight, a_day }. A bond or feud names both
        // participants (two calls); a death names the dead. The kind is
        // the news's flavour: Social for a bond or feud (who's who),
        // Death for a loss. The fact fades like any memory — the
        // settlement remembers its news while it stays interesting.
        //-------------------------------------------------------------------------
        inline std::size_t Spread(
            LCE::Simulation::EntityRegistry& a_registry,
            LCE::Simulation::EntityId a_subject,
            LCE::Simulation::InteractionKind a_kind,
            std::uint64_t a_day)
        {
            using namespace LCE::Simulation;

            std::size_t count = 0;

            a_registry.ForEachWithComponent<Memory>(
                [&](EntityId a_entity, Memory& a_memory)
                {
                    if (a_entity == a_subject || !a_subject.IsValid())
                    {
                        return;   // the subject does not gossip to itself
                    }

                    a_memory.Events.push_back(MemoryEvent{
                        a_subject, a_kind, WorldFacts::kFactWeight, a_day });
                    ++count;
                });

            return count;
        }

        //-------------------------------------------------------------------------
        // SpreadBond — the settlement hears that two minds bonded (or
        // feud): each participant becomes a known face. Two Spread calls,
        // one per side of the pair.
        //-------------------------------------------------------------------------
        inline std::size_t SpreadBond(
            LCE::Simulation::EntityRegistry& a_registry,
            LCE::Simulation::EntityId a_entityA,
            LCE::Simulation::EntityId a_entityB,
            LCE::Simulation::InteractionKind a_kind,
            std::uint64_t a_day)
        {
            return Spread(a_registry, a_entityA, a_kind, a_day)
                + Spread(a_registry, a_entityB, a_kind, a_day);
        }
    }
}
