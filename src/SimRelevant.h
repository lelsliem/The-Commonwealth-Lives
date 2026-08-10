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

    // Cached runtime lookup of the faction form (null until the game loads).
    [[nodiscard]] const RE::TESFaction* SettlerFaction();

    // A settler is sim-relevant: an actor in the settler faction, player
    // excluded. This is the whole rule for the translation stone — one
    // named function to change if the first in-game test mis-captures.
    [[nodiscard]] bool IsSimRelevant(const RE::Actor* a_actor);
}
