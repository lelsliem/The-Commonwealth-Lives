//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/EntityId.h"
#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/Memory.h"

#include <cstdint>

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
    // Seeds every mind's memory with the market as a trade source. A mind
    // that remembers the market — and is hungry — decides to move to it.
    // Pure: registry + entity id, no game types. The weight is the core's
    // default seed; it fades (MemoryFadeRate 0.2/s, forgotten below 0.1)
    // unless arrival reinforces it — the next stone's work.
    //-------------------------------------------------------------------------
    inline void SeedMarketMemory(
        LCE::Simulation::EntityRegistry& a_registry,
        LCE::Simulation::EntityId a_market)
    {
        a_registry.ForEachWithComponent<LCE::Simulation::Memory>(
            [a_market](LCE::Simulation::EntityId, LCE::Simulation::Memory& a_memory)
            {
                a_memory.Events.push_back(LCE::Simulation::MemoryEvent{
                    a_market, LCE::Simulation::InteractionKind::Trade, 1.0f });
            });
    }
}
