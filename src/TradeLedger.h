//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   The road between settlements is paved with good intentions —              //
//   and the settlement's caps.                                                //
//                                                                             //
//=============================================================================//

#pragma once

#include <cstdint>

namespace TLC
{
    //-------------------------------------------------------------------------
    // TradeLedger — the pure math of the inter-settlement trade design
    // (Docs/Design/InterSettlementTrade.md, the roads candidate). The
    // stone is designed, not scheduled; this header locks the ledger's
    // arithmetic so the 0.8.6a audit (or the post-beta build) starts from
    // a pinned contract instead of a blank page. Purely functional —
    // no game state, no engine surface, nothing wired into the sim.
    //
    // The shape: each settlement's market keeps a day-scoped budget of
    // meals. When the stall runs dry while bellies remain, the keeper
    // spends the settlement's hoard to send one courier to the nearest
    // fed settlement; the courier's return tops up tomorrow's budget.
    //-------------------------------------------------------------------------
    namespace TradeLedger
    {
        // How many meals the stall can serve today: what it fed
        // yesterday, never below the floor (the INI's budget base —
        // a settlement that fed nobody still has a little to offer).
        inline std::uint32_t NextBudget(
            std::uint32_t a_fedYesterday,
            std::uint32_t a_floor)
        {
            return a_fedYesterday > a_floor ? a_fedYesterday : a_floor;
        }

        // One meal served: the budget drops by one, never below zero —
        // a stall that is dry stays dry, and a mind is never fed into
        // debt.
        inline std::uint32_t AfterMeal(std::uint32_t a_budget)
        {
            return a_budget > 0u ? a_budget - 1u : 0u;
        }

        // The ran-dry read: the stall has nothing left today.
        inline bool IsDry(std::uint32_t a_budget)
        {
            return a_budget == 0u;
        }

        // The courier returned with food: tomorrow's budget grows by
        // the caravan's haul (sim.trade.caravanFood).
        inline std::uint32_t CaravanTopUp(
            std::uint32_t a_budget,
            std::uint32_t a_caravanFood)
        {
            return a_budget + a_caravanFood;
        }

        // One courier at a time: a settlement that ran dry yesterday
        // sends help only when no courier is already on the road — a
        // famine can't spawn a caravan army.
        inline bool CanSendCaravan(
            bool a_ranDryYesterday,
            bool a_courierOnRoad)
        {
            return a_ranDryYesterday && !a_courierOnRoad;
        }

        // The keeper spends what the hoard can cover, never into debt:
        // the smaller of the pouch and the caravan's price.
        inline std::uint32_t CaravanCost(
            std::uint32_t a_pouchCaps,
            std::uint32_t a_caravanCaps)
        {
            return a_pouchCaps < a_caravanCaps ? a_pouchCaps
                                               : a_caravanCaps;
        }
    }
}
