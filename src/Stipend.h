//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Every mind a wage: the settlement pays its people.                        //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/Entity/EntityId.h"
#include "LCE/Simulation/Entity/EntityRegistry.h"

#include "Components.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Stipend — the earn-caps economy (0.8.6b). The field gap it closes:
    // pouches only spend, so every non-keeper runs dry and a broke sick
    // mind rests instead of buying medicine. The design (Economy.md):
    // every human mind draws a small stipend from its settlement's
    // workshop once per world-day. Household-shared — the pouch lives on
    // one member, so iterating the pouch-carriers pays exactly one wage
    // per couple (the same one-wallet invariant as trade).
    //
    // Pure: registry + an injected home-market resolver, no game types —
    // testable exactly like SeedMarketMemory.
    //-------------------------------------------------------------------------

    // One settlement's pay receipt: who drew, how much left the treasury.
    struct StipendReceipt
    {
        std::uint32_t MarketFormId = 0;   // 0 = the wastes (no home market)
        std::uint32_t Paid = 0;           // how many minds drew this wage
        std::uint64_t Caps = 0;           // total caps paid out
    };

    //-------------------------------------------------------------------------
    // PayStipends — the once-per-day pay run. Every pouch-carrying mind
    // whose StipendMark is before a_today draws a_stipend into its pouch
    // and its mark advances to a_today (so the sweep is naturally
    // once-per-day: the second call of the same day finds all marks
    // current and pays nothing). A mind without a mark is due immediately
    // — the first tick of a world pays everyone once, the same back-fill
    // spirit as SeedPouch, and a restored world's marks come back with
    // the co-save so it never double-pays.
    //
    // a_homeMarket resolves a mind to the form id of its settlement's
    // workshop (the edge's NearestWorkshop — injected so this stays pure);
    // 0 means the mind is in the wastes, tallied under "the wastes".
    //
    // a_stipend == 0 is the off switch (the design ships default-off;
    // the credit path alone keeps the hungry fed).
    //-------------------------------------------------------------------------
    inline void PayStipends(
        LCE::Simulation::EntityRegistry& a_registry,
        std::uint32_t a_stipend,
        std::uint64_t a_today,
        const std::function<std::uint32_t(LCE::Simulation::EntityId)>&
            a_homeMarket,
        std::vector<StipendReceipt>& a_receipts)
    {
        if (a_stipend == 0)
        {
            return;   // the stipend is off — nothing to pay
        }

        // The tally keyed by market form id, in first-seen order.
        std::unordered_map<std::uint32_t, std::size_t> index;

        a_registry.ForEachWithComponent<CapPouch>(
            [&](LCE::Simulation::EntityId a_entity, CapPouch& a_pouch)
            {
                const auto mark =
                    a_registry.GetComponent<StipendMark>(a_entity);

                if (mark && mark->Day >= a_today)
                {
                    return;   // already drew today — no double pay
                }

                // The wage lands in the household wallet — the pouch this
                // entity carries IS that wallet (one pouch per couple).
                a_pouch.Caps += a_stipend;

                if (mark)
                {
                    mark->Day = a_today;
                }
                else
                {
                    a_registry.AddComponent<StipendMark>(
                        a_entity, StipendMark{ a_today });
                }

                const auto market = a_homeMarket(a_entity);

                const auto it = index.find(market);

                if (it != index.end())
                {
                    auto& receipt = a_receipts[it->second];
                    receipt.Paid += 1;
                    receipt.Caps += a_stipend;
                }
                else
                {
                    index.emplace(market, a_receipts.size());
                    a_receipts.push_back(
                        StipendReceipt{ market, 1, a_stipend });
                }
            });
    }
}
