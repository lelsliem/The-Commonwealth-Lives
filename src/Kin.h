//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Family first — and family is off the menu.                               //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/Entity/EntityId.h"

#include <cstdint>
#include <set>
#include <utility>

namespace TLC::Kin
{
    using namespace LCE::Simulation;

    //-------------------------------------------------------------------------
    // Kin (0.7.5 field find) — the game's families never romance. The
    // sim derives friends, sweethearts, and spouses from dispositions —
    // but the world it seeds already contains families, and a father
    // and daughter whose dispositions warm over shared meals must never
    // cross the sweetheart line. The bond book has no idea who is kin;
    // this module is the adapter's family knowledge, and the bond gates
    // (Reconcile, the event channel) refuse a romantic bond for a kin
    // pair — family can be friends, never lovers. Pure: no game types,
    // testable without the game.
    //-------------------------------------------------------------------------

    // A kin pair: two base forms that are family and must never
    // romance. Verified against the Fallout wiki (2026-08-12) — the
    // vanilla settler families' parent-child lines:
    //   Abernathy Farm — Lucy is Blake and Connie's daughter.
    //   Finch Farm     — Daniel is Abraham and Abigail's son.
    // The Longs and the Warwick couple are MARRIED, not kin-blocked:
    // the sim marrying them is lore-correct. Actual children — game or
    // sim-born — are Child species, and a child never romances anyone
    // (the species gate, which needs no table). A future quest or mod
    // that adds a family adds its base pair here.
    struct KinPair
    {
        std::uint32_t BaseA = 0;
        std::uint32_t BaseB = 0;
    };

    inline constexpr KinPair kKinPairs[] = {
        { 0x0006B4D3, 0x0006B4D2 },   // Blake Abernathy x Lucy Abernathy
        { 0x0006B4D1, 0x0006B4D2 },   // Connie Abernathy x Lucy Abernathy
        { 0x0003F23D, 0x0003F22B },   // Abraham Finch x Daniel Finch
        { 0x0003F22C, 0x0003F22B },   // Abigail Finch x Daniel Finch
    };

    // IsKinBasePair — the pure lookup, insensitive to pair order. The
    // base form ids are matched on their low 24 bits (the record id —
    // stable whatever the load order, since Fallout4.esm is always the
    // 00 slot and an override never changes a form id).
    inline bool IsKinBasePair(
        std::uint32_t a_baseA, std::uint32_t a_baseB) noexcept
    {
        const auto lo = (a_baseA & 0x00FFFFFF) < (a_baseB & 0x00FFFFFF)
            ? (a_baseA & 0x00FFFFFF) : (a_baseB & 0x00FFFFFF);
        const auto hi = (a_baseA & 0x00FFFFFF) < (a_baseB & 0x00FFFFFF)
            ? (a_baseB & 0x00FFFFFF) : (a_baseA & 0x00FFFFFF);

        for (const auto& pair : kKinPairs)
        {
            const auto plo = pair.BaseA < pair.BaseB ? pair.BaseA : pair.BaseB;
            const auto phi = pair.BaseA < pair.BaseB ? pair.BaseB : pair.BaseA;

            if (lo == plo && hi == phi)
            {
                return true;
            }
        }

        return false;
    }

    // The derived kin set: ordered entity-value pairs (the same shape
    // as Bonds::PairKey) that must never romance. Built by the adapter
    // from the loaded actors' base forms; read by the bond gates.
    using KinSet = std::set<std::pair<std::uint64_t, std::uint64_t>>;
}
