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
    // knows how to walk NPCs; the adapter invokes it. The game's "move
    // here" (Actor::InitiateCommandModeTravelPackage, pinned to 1.11.221
    // in Movement.cpp) is the verified entry, but it requires the game's
    // command-mode state — calling it cold crashed the game (heap
    // corruption, 0xC0000409) — so WalkTo currently refuses and the next
    // stone drives the command sequence or switches to a pathing
    // primitive. The core never names a game action and never appears
    // here (ADR-0024).
    //-------------------------------------------------------------------------
    namespace Movement
    {
        // Walks the actor to the target using the game's own machinery.
        // Currently refuses (with a logged reason) because the verified
        // travel-package entry needs the game's command state, which the
        // adapter does not drive yet — refusing is the contract: never
        // crash, never teleport; the intent is dropped and the sim
        // re-decides next tick.
        bool WalkTo(RE::Actor* a_actor, RE::TESObjectREFR* a_target);
    }
}
