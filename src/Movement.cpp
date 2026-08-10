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
#include <RE/T/TESObjectREFR.h>

#include <REL/Offset.h>
#include <REL/Relocation.h>

#include <REX/LOG.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace
{
    //-------------------------------------------------------------------------
    // The game's walking machinery, pinned to 1.11.221. Two layers:
    //
    // 1. The command package — the game's real "move here". The old
    //    address database and the Fallout 4 public PDB both name it
    //    `Actor::InitiateCommandModeTravelPackage(TESObjectREFR*, COMMAND_TYPE)`
    //    — this = the actor being commanded, arg1 = the destination refr.
    //    It sets the actor's command target to the refr, builds a command
    //    package (which outranks the sandbox package — the bare planner
    //    call below lost to sandbox, the probe proved it), and starts the
    //    walk. In 1.11.221 it is at 0xD77440, byte-verified (prologue
    //    55 53 56 57 41 54 41 55 — push rbp/rbx/rsi/rdi/r14/r15). The
    //    COMMAND_TYPE argument of the PDB signature is not read by this
    //    build (the command type rides in the package it creates).
    //
    // 2. The movement planner — the game walks an NPC to a point through
    //    its movement controller. The actor's `movementController` is a
    //    0x1A8-byte object: the AI base (MovementControllerAI, with the
    //    active-arbiter set) at +0x00, and the NPC subobject at +0x138.
    //    The NPC subobject's vtable — 0x2567B68, RTTI-verified (name →
    //    type descriptor → COL → vtable) — carries the DoSet* family;
    //    slot [2] (function 0xdc92f0) is the game's
    //    DoSetPlannerDirectControl: it activates the planner arbiter in
    //    the controller's active set and sets the destination NiPoint3.
    //
    // Neither is wrapped in CommonLibF4 nor present in the address
    // library — both are pinned here by RTTI/byte checks against
    // Fallout4.exe 1.11.221. A wrong pin refuses (never teleports) and
    // logs the truth.
    //
    // Order matters: the command package first (it is the order — sandbox
    // cannot override it), then the planner activation (the destination
    // the package's own pathing can begin from), then the kMove command
    // state on the AI process (the game's own "commanding" marker).
    //-------------------------------------------------------------------------
    constexpr std::uintptr_t kTravelPackageRva = 0xD77440;   // Actor::InitiateCommandModeTravelPackage
    constexpr std::uintptr_t kNpcSubobjectOffset = 0x138;    // the NPC part of the controller
    constexpr std::uintptr_t kNpcVtableRva = 0x2567B68;      // the DoSet* vtable
    constexpr std::size_t kDoSetPlannerSlot = 2;             // 0xdc92f0

    using InitiateTravelFn = void (*)(RE::Actor* a_actor, RE::TESObjectREFR* a_target);
    using DoSetPlannerFn = void (*)(void* a_controller, const RE::NiPoint3* a_destination);

    // The first eight bytes of InitiateCommandModeTravelPackage in
    // 1.11.221: push rbp; push rbx; push rsi; push rdi; push r14; push r15.
    constexpr std::array<std::uint8_t, 8> kTravelPrologue{
        0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55 };

    bool Matches(const std::uintptr_t a_rva, const std::array<std::uint8_t, 8>& a_bytes)
    {
        const auto address = REL::Offset{ a_rva }.address();
        return std::memcmp(
            reinterpret_cast<const void*>(address), a_bytes.data(), a_bytes.size()) == 0;
    }
}

namespace TLC::Movement
{
    bool WalkTo(RE::Actor* a_actor, RE::TESObjectREFR* a_target)
    {
        if (a_actor == nullptr || a_actor->currentProcess == nullptr)
        {
            REX::DEBUG("LCE: WalkTo refused — no actor or no AI process.");
            return false;
        }

        if (a_target == nullptr)
        {
            REX::DEBUG("LCE: WalkTo refused — no target reference.");
            return false;
        }

        // Byte-verify the two pins before first use: a wrong pin (runtime
        // changed, address library mismatch) must refuse, never crash.
        if (!Matches(kTravelPackageRva, kTravelPrologue))
        {
            REX::ERROR(
                "LCE: WalkTo refused — InitiateCommandModeTravelPackage pin "
                "mismatch at {:#x}; intent dropped (never teleport).",
                REL::Offset{ kTravelPackageRva }.address());
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

        // 1. The order: the command-mode travel package. This is the game's
        //    "move here" — a command package that outranks the sandbox
        //    package (the probe proved the bare planner loses to sandbox).
        const auto initiateTravel = *reinterpret_cast<InitiateTravelFn*>(
            REL::Offset{ kTravelPackageRva }.address());

        initiateTravel(a_actor, a_target);

        REX::DEBUG(
            "LCE: WalkTo — command-mode travel package issued for target {:#010x}.",
            a_target->GetFormID());

        // 2. The destination: drive the movement planner to the target's
        //    position, so the command package's pathing has the exact
        //    point to walk to.
        const auto destination = a_target->GetPosition();

        const auto fn = *reinterpret_cast<DoSetPlannerFn*>(vtable + kDoSetPlannerSlot * 8);

        fn(npc, &destination);

        // 3. The command marker on the AI process (the game's own
        //    "commanding" state; kRelease is a later stone's work).
        a_actor->currentProcess->SetCommandType(RE::COMMAND_TYPE::kMove);

        REX::DEBUG(
            "LCE: WalkTo — planner activated for destination ({}, {}, {}).",
            destination.x, destination.y, destination.z);

        return true;
    }
}
