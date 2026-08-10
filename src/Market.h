//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Come one, come all — the caps are fresh and the rads are free.                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/EntityId.h"
#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/Memory.h"

#include <cstdint>
#include <functional>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Market — where the settlement trades. The core reasons over
    // InteractionKind::Trade and never names a game form (ADR-0024); the
    // adapter's job is to make sure a mind knows where to trade, so a
    // hungry settler can decide MoveTo -> market instead of Explore.
    //
    // One constant, one seed — both free of game types, both testable.
    //-------------------------------------------------------------------------

    // The Sanctuary workshop — the settlement's market for the walking
    // stone. kMarketFormId is the placed reference 000250FE
    // ("SanctuaryWorkshopREF"; its base is the vanilla WorkshopWorkbench
    // "Workshop", 000C1AEB). It must be the REFR, not the base form:
    // walking needs a placed object with a position. Pinned by scanning
    // Fallout4.esm — the record's EDID is SanctuaryWorkshopREF and its
    // DATA position (−79048, 89587, far NW — the northernmost workshop)
    // cross-checks the canonical settlement table. The runtime
    // loaded-guard keeps a wrong pin harmless. A per-location lookup
    // becomes the refinement after this stone proves the road.
    inline constexpr std::uint32_t kMarketFormId = 0x000250FEu;

    //-------------------------------------------------------------------------
    // Seeds every mind's memory with its food source — the market by
    // default, or whatever a_source resolves per mind (the 0.5.0
    // per-species food sources: an animal's owner, when the game assigns
    // one). a_include, when given, filters which minds are seeded at all
    // (the market radius). A mind that remembers its food source — and is
    // hungry — decides to move to it.
    //
    // The memory kind is Trade for every species — the engine's hunger
    // branch finds food only through Trade-kind memories (ChooseTarget).
    // The lie is deliberate and documented (Behaviour.md): the memory
    // says "food is over there"; the arrival outcome decides what getting
    // there means (a human trades, a dog is fed).
    //
    // Pure: registry + entity ids + resolvers, no game types. The weight
    // is the core's default seed; it fades (MemoryFadeRate 0.2/s,
    // forgotten below 0.1) unless arrival reinforces it.
    //
    // Idempotent on purpose: a mind that already remembers its food
    // source is left alone. The core erases faded events below its
    // threshold, so a missing event means truly forgotten — the seed only
    // ever adds the fact back to minds that lost it. This makes periodic
    // re-seeding safe (the adapter re-pushes the fact, per the 0.5.0
    // world-facts design) without growing memory.
    //-------------------------------------------------------------------------
    inline void SeedMarketMemory(
        LCE::Simulation::EntityRegistry& a_registry,
        LCE::Simulation::EntityId a_market,
        const std::function<LCE::Simulation::EntityId(LCE::Simulation::EntityId)>& a_source = {},
        const std::function<bool(LCE::Simulation::EntityId)>& a_include = {})
    {
        a_registry.ForEachWithComponent<LCE::Simulation::Memory>(
            [a_market, &a_source, &a_include](
                LCE::Simulation::EntityId a_entity, LCE::Simulation::Memory& a_memory)
            {
                if (a_include && !a_include(a_entity))
                {
                    return;   // not near the market — no market memory
                }

                const auto source = a_source ? a_source(a_entity) : a_market;

                if (!source.IsValid())
                {
                    return;   // no food source for this mind — a grazer
                }

                for (const auto& event : a_memory.Events)
                {
                    if (event.Other == source
                        && event.Kind == LCE::Simulation::InteractionKind::Trade)
                    {
                        return;   // already remembers — keep its memory
                    }
                }

                a_memory.Events.push_back(LCE::Simulation::MemoryEvent{
                    source, LCE::Simulation::InteractionKind::Trade, 1.0f });
            });
    }
}
