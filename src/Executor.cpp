//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#include "Executor.h"

namespace TLC
{
    std::vector<PlanEntry> BuildPlan(
        LCE::Simulation::EntityRegistry& a_registry,
        const std::function<bool(LCE::Simulation::EntityId)>& a_actorLoaded,
        const std::function<bool(LCE::Simulation::EntityId)>& a_targetLoaded,
        const std::function<bool(LCE::Simulation::EntityId)>& a_available)
    {
        std::vector<PlanEntry> plan;

        a_registry.ForEachWithComponent<LCE::Simulation::Intent>(
            [&](LCE::Simulation::EntityId a_entity, const LCE::Simulation::Intent& a_intent) {
                PlanEntry entry;
                entry.Entity = a_entity;
                entry.Intent = a_intent;
                entry.ActorLoaded = a_actorLoaded(a_entity);
                entry.TargetLoaded = a_targetLoaded(a_intent.Target);
                entry.Available = a_available(a_entity);
                plan.push_back(entry);
            });

        return plan;
    }
}
