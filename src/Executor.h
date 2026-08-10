//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "Translator.h"

#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/EntityRegistry.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Executor — the read + the table (ADR-0024: the adapter's art). After
    // Update, every mind carries a fresh Intent; the executor sweeps them
    // and decides, per intent, whether the game can honor it right now.
    //
    // Pure by design: the plan builder knows nothing of the game — "loaded"
    // and "available" are injected predicates. The adapter supplies the game
    // answers; the tests supply tables. No RE:: type appears here.
    //-------------------------------------------------------------------------
    struct PlanEntry
    {
        LCE::Simulation::EntityId Entity;
        LCE::Simulation::Intent Intent;
        bool ActorLoaded = false;   // the entity's form is a loaded actor
        bool TargetLoaded = false;  // true when the intent has no target;
                                    // else whether the target's form is loaded
        bool Available = false;     // the actor is free to act
    };

    //-------------------------------------------------------------------------
    // Sweeps every mind with an Intent and builds the action plan. Refusal
    // is the contract, honored cheaply: an entry with any flag false is
    // dropped by the executor — nothing is queued, nothing blocks; the sim
    // computes a fresh intent next tick.
    //-------------------------------------------------------------------------
    [[nodiscard]] std::vector<PlanEntry> BuildPlan(
        LCE::Simulation::EntityRegistry& a_registry,
        const std::function<bool(LCE::Simulation::EntityId)>& a_actorLoaded,
        const std::function<bool(LCE::Simulation::EntityId)>& a_targetLoaded,
        const std::function<bool(LCE::Simulation::EntityId)>& a_available);
}
