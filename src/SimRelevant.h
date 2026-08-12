//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Two legs, four legs, or a pack brahmin — all are citizens.                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include <cstdint>

namespace RE
{
    class Actor;
    class TESFaction;
}

namespace TLC
{
    //-------------------------------------------------------------------------
    // The settler faction. WorkshopNPCFaction is the vanilla settler
    // faction — extracted from the game's Fallout4.esm (FACT record
    // 0x000337F3); the often-cited "WorkshopSettlerFaction" does not exist
    // in the base game. Membership decides who becomes a mind.
    //-------------------------------------------------------------------------
    inline constexpr std::uint32_t kSettlerFactionFormId = 0x000337F3;

    // HasBeenCompanionFaction — the game applies it permanently to any
    // actor the moment they are recruited as a companion (FACT record
    // 0x000A1B85, verified in Fallout4.esm 2026-08-12). Membership is
    // the companion signal: a dismissed companion assigned to a
    // settlement gains the settler faction (and becomes a mind), but
    // can never have reached a settlement without having been
    // recruited — so this faction is always set for the minds we must
    // keep out of the dating pool.
    inline constexpr std::uint32_t kHasBeenCompanionFactionFormId = 0x000A1B85;

    // Cached runtime lookup of the faction form (null until the game loads).
    [[nodiscard]] const RE::TESFaction* HasBeenCompanionFaction();

    // Cached runtime lookup of the faction form (null until the game loads).
    [[nodiscard]] const RE::TESFaction* SettlerFaction();

    // A settler is sim-relevant: an actor in the settler faction, player
    // excluded — or, since 0.7.3/0.7.4, one of the road's people (a
    // role-word base form: provisioner, caravan guard, trader) or a
    // seller (anyone with a merchant container). This is the whole rule
    // for the translation stone — one named function to change if the
    // first in-game test mis-captures.
    [[nodiscard]] bool IsSimRelevant(const RE::Actor* a_actor);

    // Who sells (0.7.4 Trade with anyone): an actor with a merchant
    // container. The game computes a runtime vendor faction for anyone
    // who sells; the fast path reads it, the deterministic path scans
    // the base form's factions for a merchant container (the runtime
    // faction is computed lazily — a just-loaded vendor may still read
    // null).
    [[nodiscard]] bool IsVendor(const RE::Actor* a_actor);
}
