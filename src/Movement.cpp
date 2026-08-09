//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#include "Movement.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/AIProcess.h>
#include <RE/A/Actor.h>
#include <RE/N/NiPoint3.h>

#include <REL/Offset.h>
#include <REL/Relocation.h>

#include <REX/LOG.h>

namespace
{
    //-------------------------------------------------------------------------
    // The game's walking call: AIProcess::CreateMovementPlanner — the call
    // the game uses to walk settlers to workshop jobs. It is not wrapped in
    // CommonLibF4 and has no address-library ID; mods declare it per
    // runtime. The 1.11.221 RVA is pending in-game verification; until it
    // is confirmed, WalkTo refuses rather than teleport.
    //-------------------------------------------------------------------------
    constexpr std::uintptr_t kCreateMovementPlannerRva = 0;

    using CreateMovementPlannerFn =
        void (*)(RE::AIProcess* a_process, RE::Actor* a_actor, const RE::NiPoint3& a_destination);
}

namespace TLC::Movement
{
    bool WalkTo(RE::Actor* a_actor, const RE::NiPoint3& a_destination)
    {
        if (a_actor == nullptr || a_actor->currentProcess == nullptr)
        {
            REX::DEBUG("LCE: WalkTo refused — no actor or no AI process.");
            return false;
        }

        if constexpr (kCreateMovementPlannerRva != 0)
        {
            static REL::Relocation<CreateMovementPlannerFn> createMovementPlanner{
                REL::Offset{ kCreateMovementPlannerRva }
            };

            createMovementPlanner(a_actor->currentProcess, a_actor, a_destination);
            return true;
        }

        REX::INFO(
            "LCE: WalkTo — movement planner for 1.11.221 pending verification; "
            "intent dropped (never teleport).");
        return false;
    }
}
