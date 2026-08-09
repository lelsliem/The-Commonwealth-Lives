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
    class NiPoint3;
}

namespace TLC
{
    //-------------------------------------------------------------------------
    // Movement — the adapter's walk. One seam, one truth: the game already
    // knows how to walk NPCs; the adapter invokes it. The core never names a
    // game action and never appears here (ADR-0024).
    //-------------------------------------------------------------------------
    namespace Movement
    {
        // Walks the actor to the destination using the game's own movement
        // machinery. Returns false when walking is unavailable — the intent
        // is dropped and the sim re-decides next tick. Never teleports.
        bool WalkTo(RE::Actor* a_actor, const RE::NiPoint3& a_destination);
    }
}
