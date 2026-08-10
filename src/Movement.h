//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

namespace RE
{
    class Actor;
    class TESObjectREFR;
}

namespace TLC
{
    //-------------------------------------------------------------------------
    // Movement — the adapter's walk. One seam, one truth: the game already
    // knows how to walk NPCs; the adapter invokes it (Actor::
    // InitiateCommandModeTravelPackage — the game's "move here" command —
    // pinned to 1.11.221 in Movement.cpp). The core never names a game
    // action and never appears here (ADR-0024).
    //-------------------------------------------------------------------------
    namespace Movement
    {
        // Walks the actor to the target using the game's own machinery:
        // the command-mode travel package (the game's "move here", which
        // outranks the sandbox package). Returns false when walking is
        // unavailable or the runtime verification fails — the intent is
        // dropped and the sim re-decides next tick. Never teleports.
        bool WalkTo(RE::Actor* a_actor, RE::TESObjectREFR* a_target);
    }
}
