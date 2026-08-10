//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#include "Tick.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <REL/Offset.h>
#include <REL/THook.h>

#include <REX/LOG.h>

#include <chrono>
#include <cstdint>

namespace
{
    //-------------------------------------------------------------------------
    // The per-frame hook candidates, pinned to 1.11.221.
    //
    //   [0..3]  ProcessVMTick's four call sites (address library ID 2251368
    //           → 0x010F04A0 — the budget-ticked Papyrus VM queue that
    //           F4SE itself hooks for its delay functors).
    //   [4]     A call inside the game's frame driver: the 5KB function
    //           0x00C2FD12 (no direct callers; entered via function
    //           pointer; dozens of internal loops) into the update
    //           function 0x00C32450.
    //
    // In-game evidence so far: the sim DID tick and log intents with all
    // five installed (they appeared at the same millisecond as hook 1's
    // first fire — 15s after the world started). The first per-hook proof
    // logging could not attribute the shared tick counter to a hook, so
    // the single-hook build over-pruned. This build restores all five and
    // counts each hook's fires — the periodic report names the per-frame
    // path exactly, then dead sites get pruned.
    //-------------------------------------------------------------------------
    constexpr std::uintptr_t kTickCallSites[] = {
        0x010E9F7E,   // ProcessVMTick caller A
        0x010EA08E,   // ProcessVMTick caller B
        0x010EA24B,   // ProcessVMTick caller C
        0x010EA2F6,   // ProcessVMTick caller C
        0x00C30C0A,   // the game driver → update
    };

    constexpr std::size_t kHookCount = std::size(kTickCallSites);

    // The call ABI at these sites: up to four register args and a budget
    // float on the stack. The hooks pass them through untouched — the
    // game's own calls must keep working.
    using TickFn = void(void*, void*, void*, void*, float);

    void (*g_OnTick)(double) = nullptr;

    std::chrono::steady_clock::time_point g_LastTick{};
    bool g_HasLastTick = false;

    std::uint32_t g_TickCount = 0;
    std::uint32_t g_FireCounts[kHookCount] = {};

    //-------------------------------------------------------------------------
    // The tick: at most once per frame, with the real delta since the last
    // tick. Several call sites may fire in one frame; the guard collapses
    // them. The original is always invoked first — the game's own calls
    // depend on it.
    //-------------------------------------------------------------------------
    void RunTick()
    {
        if (g_OnTick == nullptr)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        double delta = 0.0;
        if (g_HasLastTick)
        {
            delta = std::chrono::duration<double>(now - g_LastTick).count();

            // Same frame (another call site) — the sim already ticked.
            if (delta < 0.001)
            {
                return;
            }
        }

        g_LastTick = now;
        g_HasLastTick = true;

        if (++g_TickCount % 600 == 0)
        {
            // Per-hook attribution: [0..3] are the ProcessVMTick sites,
            // [4] is the driver. Rates tell the per-frame path from the
            // event-driven stragglers.
            REX::INFO(
                "Tick {} frames; fires: [0]={} [1]={} [2]={} [3]={} [4]={}",
                g_TickCount,
                g_FireCounts[0], g_FireCounts[1], g_FireCounts[2],
                g_FireCounts[3], g_FireCounts[4]);
        }

        g_OnTick(delta);
    }

    // The five hooks. Declared before the functions that call them, defined
    // after — each registers itself with FHookStore on construction (DLL
    // load), is Init'd at PreLoad, and enabled at Load by F4SE::Init.
    extern REL::THook<TickFn> g_CallSite0;
    extern REL::THook<TickFn> g_CallSite1;
    extern REL::THook<TickFn> g_CallSite2;
    extern REL::THook<TickFn> g_CallSite3;
    extern REL::THook<TickFn> g_CallSite4;

    void Hook0(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite0(a1, a2, a3, a4, aBudget);
        ++g_FireCounts[0];
        RunTick();
    }

    void Hook1(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite1(a1, a2, a3, a4, aBudget);
        ++g_FireCounts[1];
        RunTick();
    }

    void Hook2(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite2(a1, a2, a3, a4, aBudget);
        ++g_FireCounts[2];
        RunTick();
    }

    void Hook3(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite3(a1, a2, a3, a4, aBudget);
        ++g_FireCounts[3];
        RunTick();
    }

    void Hook4(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite4(a1, a2, a3, a4, aBudget);
        ++g_FireCounts[4];
        RunTick();
    }

    REL::THook<TickFn> g_CallSite0{
        REL::Offset{ kTickCallSites[0] }, 0, &Hook0
    };
    REL::THook<TickFn> g_CallSite1{
        REL::Offset{ kTickCallSites[1] }, 0, &Hook1
    };
    REL::THook<TickFn> g_CallSite2{
        REL::Offset{ kTickCallSites[2] }, 0, &Hook2
    };
    REL::THook<TickFn> g_CallSite3{
        REL::Offset{ kTickCallSites[3] }, 0, &Hook3
    };
    REL::THook<TickFn> g_CallSite4{
        REL::Offset{ kTickCallSites[4] }, 0, &Hook4
    };
}

namespace TLC::Tick
{
    void Install(void (*a_onTick)(double))
    {
        g_OnTick = a_onTick;
    }
}
