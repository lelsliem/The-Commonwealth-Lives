//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Foot after foot — the oldest pathfinding library there is.                                      //
//                                                                             //
//=============================================================================//

#include "Movement.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/Actor.h>
#include <RE/C/COMMAND_TYPE.h>
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
    // The game's walking machinery, pinned to 1.11.221.
    //
    // Actor::InitiateCommandModeTravelPackage(TESObjectREFR*, COMMAND_TYPE)
    // — the game's own "move here": this = the actor that walks, arg1 =
    // the destination refr, arg2 = the command type. It sets the command
    // relationship, reads the destination from the refr, writes it into the
    // actor's AI process, sets the command state, and evaluates the
    // package. A command package outranks the sandbox package — the bare
    // planner call lost to sandbox, the probe proved it.
    //
    // Where the pin came from: the old flat database and the FO4 PDB name
    // the function; the old CSV's ID era doesn't bridge to the current
    // address library (its numbering changed), so the 1.11.221 address was
    // located by anchoring — names in commonlibf4's IDs.h resolve to
    // current-era IDs (verified: SetCommandType 2231826 -> 0xD00890), the
    // old database gives the same names' 1.10.163 RVAs, and the local
    // old->new shift interpolates travel's old 0xD82300 to ~0xC6BD00.
    // Disassembly at 0xC6BE90 confirmed it: the only candidate in the
    // window that (a) is ~557 bytes (the old build's exact size), (b) takes
    // (actor, target refr, command type), (c) writes the target's position
    // into the actor's process as the travel destination, and (d) calls
    // SetCommandType (0xD00890) with the passed type. The earlier 0xD77440
    // pin was wrong (disassembly showed a stats/report loop, and the
    // byte-check refused it in-game — the check exists exactly for that).
    //
    // Not wrapped in CommonLibF4 nor present in the current address
    // library — pinned here with a byte check against Fallout4.exe
    // 1.11.221. A wrong pin refuses (never teleports) and logs the truth.
    //
    // Crash history (kept honest): calling this cold was blamed for a
    // heap-corruption fail-fast (0xC0000409 in ucrtbase), but the crash
    // was later proven to be a corrupt save — the identical signature hit
    // builds where this call never executed and a run with the DLL
    // removed entirely. On a stable save this call is the game's own
    // "move here" and deserves its real test.
    constexpr std::uintptr_t kTravelPackageRva = 0xC6BE90;   // Actor::InitiateCommandModeTravelPackage

    // The first eight bytes in 1.11.221: mov [rsp+0x18],rbx; push rdi;
    // push r12.
    constexpr std::array<std::uint8_t, 8> kTravelPrologue{
        0x48, 0x89, 0x5C, 0x24, 0x18, 0x57, 0x41, 0x54 };

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

        // Byte-verify the pin so this refusal is grounded in the real
        // exe — a wrong pin would mean the whole analysis is off.
        if (!Matches(kTravelPackageRva, kTravelPrologue))
        {
            REX::ERROR(
                "LCE: WalkTo refused — InitiateCommandModeTravelPackage pin "
                "mismatch at {:#x}; intent dropped (never teleport).",
                REL::Offset{ kTravelPackageRva }.address());
            return false;
        }

        // The game's own "move here": command this actor to travel to the
        // target reference. A command package outranks the sandbox package
        // that ate the bare planner call — this is the vanilla command-
        // mode call the game itself uses. Pin is byte-verified above.
        using TravelFn = void (*)(RE::Actor*, RE::TESObjectREFR*, RE::COMMAND_TYPE);
        const auto travel =
            reinterpret_cast<TravelFn>(REL::Offset{ kTravelPackageRva }.address());

        travel(a_actor, a_target, RE::COMMAND_TYPE::kMove);

        REX::INFO(
            "LCE: WalkTo — command-mode travel package issued for target {:#x}.",
            a_target->GetFormID());
        return true;
    }

    bool HoldPlace(RE::Actor* a_actor)
    {
        if (a_actor == nullptr || a_actor->currentProcess == nullptr)
        {
            return false;
        }

        if (!Matches(kTravelPackageRva, kTravelPrologue))
        {
            return false;   // same pin, same refusal — never teleport
        }

        // Command the actor to "travel" to itself: the destination is
        // its own position, so the package parks it and suspends the
        // sandbox. The caller rate-limits the issue; this logs once per
        // hold (debug — a held world is visible, not deafening).
        using TravelFn = void (*)(RE::Actor*, RE::TESObjectREFR*, RE::COMMAND_TYPE);
        const auto travel =
            reinterpret_cast<TravelFn>(REL::Offset{ kTravelPackageRva }.address());

        travel(a_actor, a_actor, RE::COMMAND_TYPE::kMove);

        REX::DEBUG(
            "LCE: hold — settler {:#x} commanded in place (Rest/Explore).",
            a_actor->GetFormID());
        return true;
    }
}
