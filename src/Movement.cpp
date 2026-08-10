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

#include <RE/A/Actor.h>
#include <RE/C/COMMAND_TYPE.h>
#include <RE/N/NiPoint3.h>

#include <REL/Offset.h>
#include <REL/Relocation.h>

#include <REX/LOG.h>

#include <cstdint>

namespace
{
    //-------------------------------------------------------------------------
    // The game's walking call, pinned to 1.11.221.
    //
    // The game walks an NPC to a point through its movement controller.
    // The actor's `movementController` is a 0x1A8-byte object: the AI
    // base (MovementControllerAI, with the active-arbiter set) at +0x00,
    // and the NPC subobject at +0x138. The NPC subobject's vtable —
    // 0x2567B68, RTTI-verified (name → type descriptor → COL → vtable)
    // — carries the DoSet* family; slot [2] (function 0xdc92f0) is the
    // game's DoSetPlannerDirectControl: it activates the planner arbiter
    // in the controller's active set and sets the destination NiPoint3.
    // That is the call the game itself uses to walk an actor to a point
    // ("move here", the command system).
    //
    // Not wrapped in CommonLibF4 and absent from the address library —
    // pinned here by the RTTI chain against Fallout4.exe 1.11.221. The
    // runtime vtable check below keeps a wrong pin harmless: walking
    // refuses (never teleports) and logs the truth.
    //
    // The planner destination alone is NOT enough — the probe proved it:
    // the planner activated every run, yet the settler stood still. The
    // settler's sandbox package keeps overriding the movement mode, and a
    // bare planner destination is a hint, not an order. The game honors
    // commands over packages ("move here" goes through the command
    // system), so WalkTo also marks the walk as a command (kMove) via
    // AIProcess::SetCommandType — wrapped in CommonLibF4 with a current
    // address-library ID. Command + destination = the walk the game
    // itself would produce.
    //-------------------------------------------------------------------------
    constexpr std::uintptr_t kNpcSubobjectOffset = 0x138;   // the NPC part of the controller
    constexpr std::uintptr_t kNpcVtableRva = 0x2567B68;     // the DoSet* vtable
    constexpr std::size_t kDoSetPlannerSlot = 2;            // 0xdc92f0

    using DoSetPlannerFn = void (*)(void* a_controller, const RE::NiPoint3* a_destination);
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

        const auto controller =
            reinterpret_cast<std::uintptr_t>(a_actor->movementController.get());

        if (controller == 0)
        {
            REX::DEBUG("LCE: WalkTo refused — no movement controller.");
            return false;
        }

        auto* npc = reinterpret_cast<void*>(controller + kNpcSubobjectOffset);

        const auto vtable = *reinterpret_cast<std::uintptr_t*>(npc);
        const auto expected = REL::Offset{ kNpcVtableRva }.address();

        if (vtable != expected)
        {
            REX::ERROR(
                "LCE: WalkTo refused — movement controller vtable mismatch "
                "(got {:#x}, want {:#x}); intent dropped (never teleport).",
                vtable, expected);
            return false;
        }

        // Mark the walk as a command (kMove) so the sandbox package cannot
        // override it. Releasing the command (kRelease) is a later stone's
        // work — for the walking stone the session ends after arrival.
        a_actor->currentProcess->SetCommandType(RE::COMMAND_TYPE::kMove);

        const auto fn = *reinterpret_cast<DoSetPlannerFn*>(vtable + kDoSetPlannerSlot * 8);

        fn(npc, &a_destination);

        REX::DEBUG(
            "LCE: WalkTo — planner activated for destination ({}, {}, {}).",
            a_destination.x, a_destination.y, a_destination.z);

        return true;
    }
}
