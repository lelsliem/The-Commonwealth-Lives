//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Every mind is born once — even a settlement's.                            //
//                                                                             //
//=============================================================================//

#pragma once

#include "Behaviour.h"
#include "Components.h"

#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Simulation.h"

namespace TLC
{
    //-------------------------------------------------------------------------
    // Birth (0.6.0 Stone 6 — experimental, INI-gated). A spouse household
    // has a child: a new mind, sim-only — there is no game actor, no
    // form, no translator entry. The child is fed by the household (its
    // hunger is restored directly — protected and fed), bonds to both
    // parents, and lives in the co-save like any mind. The census cannot
    // evict it: Lifecycle::Diff classifies the *census*, never the
    // registry, so a mind with no form is simply never scanned.
    //
    // Gated by sim.birth.enabled (default 0) and one birth per sim day —
    // the adapter's choice of *which* household is its edge; this module
    // is the pure act of being born.
    //-------------------------------------------------------------------------
    namespace Birth
    {
        using namespace LCE::Simulation;

        //-------------------------------------------------------------------------
        // Create — a child mind of a_spouseA and a_spouseB. Seeded like
        // any mind (needs, memory, goals) plus a warm relationship to
        // both parents — and the parents warm to the child. No FormRef:
        // the child has no game actor, and the co-save serializes it as
        // a component-carrying entity like any other.
        //-------------------------------------------------------------------------
        [[nodiscard]]
        inline EntityId Create(
            EntityRegistry& a_registry,
            EntityId a_parentA,
            EntityId a_parentB,
            const NeedRates& a_rates)
        {
            if (!a_parentA.IsValid() || !a_parentB.IsValid())
            {
                return {};
            }

            const auto id = a_registry.CreateEntity();

            a_registry.AddComponent<SpeciesTag>(id, SpeciesTag{ Species::Child });
            a_registry.AddComponent<Needs>(
                id, SeededNeeds(Species::Child, a_rates));
            a_registry.AddComponent<Goals>(id, SeededGoals(Species::Child));
            a_registry.AddComponent<Memory>(id, Memory{});

            Relationships relationships;
            relationships.ByEntity[a_parentA] = Relationship{ 0.5f, 0.3f };
            relationships.ByEntity[a_parentB] = Relationship{ 0.5f, 0.3f };
            a_registry.AddComponent<Relationships>(id, std::move(relationships));

            // The parents know their child.
            auto relA = a_registry.GetComponent<Relationships>(a_parentA);
            auto relB = a_registry.GetComponent<Relationships>(a_parentB);

            if (relA)
            {
                relA->ByEntity[id] = Relationship{ 0.6f, 0.4f };
            }

            if (relB)
            {
                relB->ByEntity[id] = Relationship{ 0.6f, 0.4f };
            }

            return id;
        }

        //-------------------------------------------------------------------------
        // FeedChildren — one tick of the child's life: every sim-only
        // child (no FormRef — there is no game actor to walk or eat) is
        // fed by the household: Hunger recovers toward full at the
        // settlement's pace. Returns how many children were fed.
        //-------------------------------------------------------------------------
        inline std::size_t FeedChildren(
            EntityRegistry& a_registry, float a_delta)
        {
            std::size_t count = 0;

            a_registry.ForEachWithComponent<Needs>(
                [&](EntityId a_entity, Needs& a_needs)
                {
                    // Sim-only: a child with a game form is a real,
                    // walkable mind — it eats at the market like an
                    // animal, never here.
                    if (a_registry.GetComponent<FormRef>(a_entity) != nullptr)
                    {
                        return;
                    }

                    const auto species =
                        a_registry.GetComponent<SpeciesTag>(a_entity);

                    if (species == nullptr || species->Value != Species::Child)
                    {
                        return;
                    }

                    for (auto& need : a_needs.List)
                    {
                        if (need.Type == NeedType::Hunger)
                        {
                            need.Value = std::min(1.0f, need.Value + 0.2f * a_delta);
                            ++count;
                            break;
                        }
                    }
                });

            return count;
        }
    }
}
