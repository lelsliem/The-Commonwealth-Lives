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
#include <vector>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Market — where the settlement trades. The core reasons over
    // InteractionKind::Trade and never names a game form (ADR-0024); the
    // adapter's job is to make sure a mind knows where to trade, so a
    // hungry settler can decide MoveTo -> market instead of Explore.
    //
    // One census, one seed — both free of game types, both testable.
    //-------------------------------------------------------------------------

    // The vanilla workshop workbench base form (FURN "Workshop",
    // 000C1AEB) — every standard settlement's workshop REFR is based on
    // it. The settlement census (Adapter::RefreshWorkshops) scans the
    // game's REFR form array for placed refs of this base; each one is a
    // settlement's market. Verified 2026-08-10 (the Sanctuary market
    // pin's base form, cross-checked against the canonical settlement
    // table). Modded workshops sharing the base join the census;
    // custom-base workshops (Vault 88, the Mechanist's lair) are missed
    // and documented — their settlers explore until the nearest standard
    // market is remembered.
    inline constexpr std::uint32_t kWorkshopBaseFormId = 0x000C1AEBu;

    // The legacy market pin — the Sanctuary workshop REFR (000250FE).
    // With the settlement census live, the Sanctuary bench is just one
    // workshop among many, found by base form like the rest; this
    // constant remains only as the fallback when the census finds
    // nothing (no REFRs loaded — an interior, a bare world): a lone
    // known market beats no market, and the sim degrades to the walking
    // stone's behavior rather than forgetting to eat.
    inline constexpr std::uint32_t kMarketFormId = 0x000250FEu;

    // The market seed's radius: a mind only remembers a market within
    // walking distance of where it stands. ~10,000 units (≈140 m) covers
    // a settlement and excludes the neighbors — Red Rocket is ~13,000
    // units from Sanctuary, Abernathy ~22,000. The probe proved why this
    // matters: the process lists carry settler-faction actors from
    // settlements kilometers away, and every one of them was issued a
    // walk to the Sanctuary workbench.
    inline constexpr float kMarketRadius = 10000.0f;

    //-------------------------------------------------------------------------
    // WorkshopPosition
    //
    // One settlement's market, as the pure seed sees it: a placed
    // workshop REFR's form and its world position. The census (the
    // edge, in Adapter.cpp) fills these from the game; the seed reasons
    // over them without touching the game again.
    //-------------------------------------------------------------------------
    struct WorkshopPosition
    {
        std::uint32_t FormId;
        float X;
        float Y;
    };

    //-------------------------------------------------------------------------
    // NearestWorkshop
    //
    // The settlement a mind belongs to, spatially: the nearest workshop
    // within a_maxDistance of (a_x, a_y), or 0 when none is in range
    // (in the wastes — no market to remember, and a hungry mind
    // explores until it finds one). Squared distances, no sqrt. Pure:
    // this is the whole "which settlement" rule, testable without the
    // game.
    //-------------------------------------------------------------------------
    inline std::uint32_t NearestWorkshop(
        float a_x, float a_y,
        const std::vector<WorkshopPosition>& a_workshops,
        float a_maxDistance) noexcept
    {
        const auto maxSq = a_maxDistance * a_maxDistance;
        float bestSq = maxSq;
        std::uint32_t best = 0;

        for (const auto& workshop : a_workshops)
        {
            const auto dx = workshop.X - a_x;
            const auto dy = workshop.Y - a_y;
            const auto distSq = dx * dx + dy * dy;

            if (distSq < bestSq)
            {
                bestSq = distSq;
                best = workshop.FormId;
            }
        }

        return best;
    }

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
