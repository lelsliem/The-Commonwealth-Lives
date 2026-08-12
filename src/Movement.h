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
    struct NiPoint3;
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
        // wander it away. Now the fallback when a cell offers nothing
        // to walk to (an empty loading cell). Same refusals as WalkTo.
        bool HoldPlace(RE::Actor* a_actor);

        // One bounded wander: walks the actor to a real, nearby
        // reference in its own cell — furniture preferred, then any
        // non-actor object within a_radius — so a resting or exploring
        // settler mills around its settlement instead of standing frozen
        // at the bench (the meal-cadence hold's look, 2026-08-11) or
        // being wandered away by the sandbox (the original gap). The
        // command-mode travel package carries the walk; on arrival the
        // sandbox resumes until the caller re-commands — so between
        // commands the game plays its own idle (and may sit the settler
        // at the furniture). The caller rate-limits (once per mind per
        // cooldown — re-issuing mid-walk would yank the actor). Falls
        // back to HoldPlace when the cell offers nothing. Silent: the
        // plan-entry decision line is the narrative; a wander every
        // cooldown across hundreds of minds must not flood the log.
        bool WanderNear(RE::Actor* a_actor, float a_radius = 4000.0f);

        // One flee (0.7.5): walks the actor to the reference in its
        // cell FARTHEST from a threat position — the loser slinks off
        // after a scuffle instead of standing at the scene of the
        // crime. Needs at least a_minDistance of ground between the
        // actor and the target (a same-spot walk would just spin), or
        // it falls back to HoldPlace. Same refusals as WalkTo.
        bool WalkAwayFrom(
            RE::Actor* a_actor, const RE::NiPoint3& a_threat,
            float a_minDistance = 600.0f);
    }
}
