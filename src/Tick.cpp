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
    // The per-frame pump, DelayFunctorQueue — 1.11.221.
    //
    // The address library knows the function (ID 2251368 → 0x010F04A0 —
    // verified against the game's own version-1-11-221-0.bin), but THook
    // patches *call sites*, not function prologues, so the four
    // `call DelayFunctorQueue` instructions inside the game loop are the
    // hooks. They are not in the address library (mid-function addresses),
    // so they are pinned to this runtime — the same discipline F4SE itself
    // uses for its own hooks (RelocAddr offsets).
    //-------------------------------------------------------------------------
    constexpr std::uintptr_t kDelayFunctorQueueCallSites[] = {
        0x010E9F7E,
        0x010EA08E,
        0x010EA24B,
        0x010EA2F6,
    };

    // The call ABI at these sites: rcx, rdx, r8, r9, then the budget float
    // on the stack (the 5th parameter). The hook passes them through
    // untouched — the game's own queue must keep running.
    using DelayFunctorQueueFn = void(void*, void*, void*, void*, float);

    void (*g_OnTick)(double) = nullptr;

    std::chrono::steady_clock::time_point g_LastTick{};
    bool g_HasLastTick = false;

    //-------------------------------------------------------------------------
    // The tick: at most once per frame, with the real delta since the last
    // tick. Several call sites may fire in one frame; the guard collapses
    // them. The original is always invoked first — the game's delay-functor
    // queue depends on it.
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

            // Same frame (a second call site) — the sim already ticked.
            if (delta < 0.001)
            {
                return;
            }
        }

        g_LastTick = now;
        g_HasLastTick = true;

        g_OnTick(delta);
    }

    // The four hooks. Declared before the functions that call them, defined
    // after — each registers itself with FHookStore on construction (DLL
    // load), is Init'd at PreLoad, and enabled at Load by F4SE::Init.
    extern REL::THook<DelayFunctorQueueFn> g_CallSite0;
    extern REL::THook<DelayFunctorQueueFn> g_CallSite1;
    extern REL::THook<DelayFunctorQueueFn> g_CallSite2;
    extern REL::THook<DelayFunctorQueueFn> g_CallSite3;

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

    void Hook2(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite2(a1, a2, a3, a4, aBudget);
        RunTick();
    }

    void Hook3(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite3(a1, a2, a3, a4, aBudget);
        RunTick();
    }

    REL::THook<DelayFunctorQueueFn> g_CallSite0{
        REL::Offset{ kDelayFunctorQueueCallSites[0] }, 0, &Hook0
    };
    REL::THook<DelayFunctorQueueFn> g_CallSite1{
        REL::Offset{ kDelayFunctorQueueCallSites[1] }, 0, &Hook1
    };
    REL::THook<DelayFunctorQueueFn> g_CallSite2{
        REL::Offset{ kDelayFunctorQueueCallSites[2] }, 0, &Hook2
    };
    REL::THook<DelayFunctorQueueFn> g_CallSite3{
        REL::Offset{ kDelayFunctorQueueCallSites[3] }, 0, &Hook3
    };
}

namespace TLC::Tick
{
    void Install(void (*a_onTick)(double))
    {
        g_OnTick = a_onTick;
    }
}
