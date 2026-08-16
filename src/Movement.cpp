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
#include <RE/N/NiPoint3.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESObjectCELL.h>
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

namespace
{
    // The game's own "move here": command this actor to travel to the
    // target reference. A command package outranks the sandbox package
    // that ate the bare planner call — this is the vanilla command-mode
    // call the game itself uses. Pin is byte-verified; silent (the
    // callers own their logging — a wander every cooldown across
    // hundreds of minds must not flood the log).
    bool IssueTravel(RE::Actor* a_actor, RE::TESObjectREFR* a_target)
    {
        if (!Matches(kTravelPackageRva, kTravelPrologue))
        {
            return false;   // never teleport on a wrong pin
        }

        using TravelFn = void (*)(RE::Actor*, RE::TESObjectREFR*, RE::COMMAND_TYPE);
        const auto travel =
            reinterpret_cast<TravelFn>(REL::Offset{ kTravelPackageRva }.address());

        travel(a_actor, a_target, RE::COMMAND_TYPE::kMove);
        return true;
    }
}

namespace TLC::Movement
{
    namespace
    {
        // The bisect gate: false refuses every command without touching
        // the game (sim.diag.noWalks). Default true — the sim walks
        // normally unless the hunt enables the gate.
        bool g_commandsEnabled = true;
    }

    void SetCommandsEnabled(const bool a_enabled) noexcept
    {
        g_commandsEnabled = a_enabled;
    }

    bool WalkTo(RE::Actor* a_actor, RE::TESObjectREFR* a_target)
    {
        if (!g_commandsEnabled)
        {
            return false;   // bisect gate — walk refused, sim re-decides
        }

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

        if (!IssueTravel(a_actor, a_target))
        {
            REX::ERROR(
                "LCE: WalkTo refused — InitiateCommandModeTravelPackage pin "
                "mismatch at {:#x}; intent dropped (never teleport).",
                REL::Offset{ kTravelPackageRva }.address());
            return false;
        }

        // Debug, not info (0.8.0 polish): the issued line fires once per
        // walk, and in a 600-mind restored world that is 2.2k lines of a
        // 12k session. The decision line already names the walker and the
        // target; the probe/arrival lines carry the progress. At DEBUG it
        // stays visible while the sim is in development and vanishes when
        // the release log level drops to info.
        REX::DEBUG(
            "LCE: WalkTo — command-mode travel package issued for target {:#x}.",
            a_target->GetFormID());
        return true;
    }

    bool HoldPlace(RE::Actor* a_actor)
    {
        if (!g_commandsEnabled)
        {
            return false;   // bisect gate
        }

        if (a_actor == nullptr || a_actor->currentProcess == nullptr)
        {
            return false;
        }

        return IssueTravel(a_actor, a_actor);
    }

    bool WanderNear(RE::Actor* a_actor, float a_radius)
    {
        if (!g_commandsEnabled)
        {
            return false;   // bisect gate
        }

        if (a_actor == nullptr || a_actor->currentProcess == nullptr)
        {
            return false;
        }

        const auto cell = a_actor->GetParentCell();

        if (cell == nullptr)
        {
            return HoldPlace(a_actor);
        }

        const auto position = a_actor->GetPosition();

        RE::TESObjectREFR* furniture = nullptr;
        float furnitureDistance = a_radius;
        RE::TESObjectREFR* anyObject = nullptr;
        float objectDistance = a_radius;

        // The bounded wander: every real reference in this cell within
        // the radius — furniture preferred (a bench to rest by), any
        // non-actor object as the fallback (a wall, a crate, a tree —
        // the actor walks there and the sandbox idles). Actor refrs are
        // skipped: commanding a walk to another settler would chase a
        // moving target.
        cell->ForEachReferenceInRange(
            position, a_radius,
            [&](RE::TESObjectREFR* a_ref) -> RE::BSContainer::ForEachResult
            {
                if (a_ref == nullptr || a_ref == a_actor)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto base = a_ref->GetObjectReference();

                if (base == nullptr
                    || base->GetFormType() == RE::ENUM_FORM_ID::kACHR)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto delta = a_ref->GetPosition() - position;
                const auto distance = delta.Length();

                if (distance > a_radius)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                if (base->GetFormType() == RE::ENUM_FORM_ID::kFURN)
                {
                    if (distance < furnitureDistance)
                    {
                        furnitureDistance = distance;
                        furniture = a_ref;
                    }
                }
                else if (distance < objectDistance)
                {
                    objectDistance = distance;
                    anyObject = a_ref;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

        // Furniture first (a bench to rest by — the sandbox may even
        // sit them between commands), then any object; nothing at all
        // in the cell — park in place rather than send them walking
        // across the world.
        const auto target = furniture != nullptr ? furniture : anyObject;

        return target != nullptr
            ? IssueTravel(a_actor, target)
            : HoldPlace(a_actor);
    }

    bool WalkAwayFrom(
        RE::Actor* a_actor, const RE::NiPoint3& a_threat,
        float a_minDistance)
    {
        if (!g_commandsEnabled)
        {
            return false;   // bisect gate
        }

        if (a_actor == nullptr || a_actor->currentProcess == nullptr)
        {
            return false;
        }

        const auto cell = a_actor->GetParentCell();

        if (cell == nullptr)
        {
            return HoldPlace(a_actor);
        }

        const auto position = a_actor->GetPosition();

        RE::TESObjectREFR* farObject = nullptr;
        float farDistance = 0.0f;

        // The flee (0.7.5): the reference in this cell FARTHEST from
        // the threat — the loser makes ground instead of milling near
        // the scene. Actor refrs are skipped (chasing a moving target
        // is not a flee); the walk must also clear a_minDistance from
        // the actor's own spot or it would spin in place.
        cell->ForEachReferenceInRange(
            position, 4000.0f,
            [&](RE::TESObjectREFR* a_ref) -> RE::BSContainer::ForEachResult
            {
                if (a_ref == nullptr || a_ref == a_actor)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto base = a_ref->GetObjectReference();

                if (base == nullptr
                    || base->GetFormType() == RE::ENUM_FORM_ID::kACHR)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto spot = a_ref->GetPosition();

                // Ground between the actor and the target, then pick
                // the farthest from the threat.
                if ((spot - position).Length() < a_minDistance)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto fromThreat = (spot - a_threat).Length();

                if (fromThreat > farDistance)
                {
                    farDistance = fromThreat;
                    farObject = a_ref;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

        return farObject != nullptr
            ? IssueTravel(a_actor, farObject)
            : HoldPlace(a_actor);
    }
}
