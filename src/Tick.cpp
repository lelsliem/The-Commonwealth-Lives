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

#include <chrono>

namespace
{
    //-------------------------------------------------------------------------
    // The per-frame hooks, pinned to 1.11.221.
    //
    // The per-frame path is ProcessVMTick (address library ID 2251368 →
    // 0x010F04A0 — the budget-ticked Papyrus VM queue F4SE itself hooks
    // for its delay functors). THook patches *call sites*, and two of the
    // four sites fire once per frame: 0x010E9F7E (site [0]) and 0x010EA08E
    // (site [1]). Verified in-game with per-hook fire counters: both
    // climbed at exactly the tick rate for 12,600+ frames (~2 minutes);
    // the other two sites never fired (event-driven VM batch processing),
    // and the game-driver site fired once at startup then never again —
    // all three pruned. The once-per-frame guard collapses the pair into
    // a single tick.
    //-------------------------------------------------------------------------
    constexpr std::uintptr_t kTickCallSites[] = {
        0x010E9F7E,   // ProcessVMTick caller A — per-frame
        0x010EA08E,   // ProcessVMTick caller B — per-frame
    };

    // The call ABI at these sites: up to four register args and a budget
    // float on the stack. The hooks pass them through untouched — the
    // game's own calls must keep working.
    using TickFn = void(void*, void*, void*, void*, float);

    void (*g_OnTick)(double) = nullptr;

    std::chrono::steady_clock::time_point g_LastTick{};
    bool g_HasLastTick = false;

    //-------------------------------------------------------------------------
    // The tick: at most once per frame, with the real delta since the last
    // tick. The two call sites may both fire in one frame; the guard
    // collapses them. The original is always invoked first — the game's
    // own calls depend on it.
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

            // Same frame (the other call site) — the sim already ticked.
            if (delta < 0.001)
            {
                return;
            }
        }

        g_LastTick = now;
        g_HasLastTick = true;

        g_OnTick(delta);
    }

    // The two hooks. Declared before the functions that call them, defined
    // after — each registers itself with FHookStore on construction (DLL
    // load), is Init'd at PreLoad, and enabled at Load by F4SE::Init.
    extern REL::THook<TickFn> g_CallSite0;
    extern REL::THook<TickFn> g_CallSite1;

    void Hook0(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite0(a1, a2, a3, a4, aBudget);
        RunTick();
    }

    void Hook1(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite1(a1, a2, a3, a4, aBudget);
        RunTick();
    }

    REL::THook<TickFn> g_CallSite0{
        REL::Offset{ kTickCallSites[0] }, 0, &Hook0
    };
    REL::THook<TickFn> g_CallSite1{
        REL::Offset{ kTickCallSites[1] }, 0, &Hook1
    };
}

namespace TLC::Tick
{
    void Install(void (*a_onTick)(double))
    {
        g_OnTick = a_onTick;
    }
}
