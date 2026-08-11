//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   A thousand-mile journey begins with a single navmesh triangle.                                      //
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
    // in Movement.cpp) is the verified entry: it commands the actor to
    // travel to the target reference, outranking the sandbox package. The
    // core never names a game action and never appears here (ADR-0024).
    //-------------------------------------------------------------------------
    namespace Movement
    {
        // Walks the actor to the target using the game's own machinery:
        // issues the command-mode travel package (byte-verified pin).
        // Refuses — with a logged reason — only when the actor, its AI
        // process, or the target is missing, or the pin mismatches:
        // never crash, never teleport; the intent is dropped and the sim
        // re-decides next tick.
        bool WalkTo(RE::Actor* a_actor, RE::TESObjectREFR* a_target);

        // Parks the actor in place: the same command-mode travel package,
        // targeted at the actor itself, so the sandbox package cannot
        // wander it away (the meal-cadence stone — a resting or exploring
        // settler stays where it is, and a fed mind at the market stays
        // at the bench between meals instead of drifting across cells).
        // Same refusals as WalkTo; the caller rate-limits (once per mind
        // per cooldown), because a command package every frame would be
        // a flood of its own.
        bool HoldPlace(RE::Actor* a_actor);
    }
}
