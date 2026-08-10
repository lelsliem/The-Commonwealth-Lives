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
    // The per-frame hook, pinned to 1.11.221.
    //
    // 0x00C30C0A — a call site inside the game's frame driver: the 5KB
    // function 0x00C2FD12 (no direct callers; entered via function
    // pointer; dozens of internal loops) into the update function
    // 0x00C32450. Verified in-game: this site fires every frame (the tick
    // counter ran 600 frames per ~10s at 60fps for the whole session).
    //
    // The earlier candidate — ProcessVMTick's four call sites (address
    // library ID 2251368, the budget-ticked Papyrus VM queue F4SE itself
    // hooks) — was proven event-driven in-game: hooks 0–3 fired only on
    // VM processing events, never per-frame. Pruned; this one hook is the
    // path.
    //-------------------------------------------------------------------------
    constexpr std::uintptr_t kTickCallSite = 0x00C30C0A;

    // The call ABI at this site: up to four register args and a budget
    // float on the stack. The hook passes them through untouched — the
    // game's own call must keep working.
    using TickFn = void(void*, void*, void*, void*, float);

    void (*g_OnTick)(double) = nullptr;

    std::chrono::steady_clock::time_point g_LastTick{};
    bool g_HasLastTick = false;

    //-------------------------------------------------------------------------
    // The tick: at most once per frame, with the real delta since the last
    // tick. The original is always invoked first — the game's own call
    // depends on it.
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

            // Same frame (a second call) — the sim already ticked.
            if (delta < 0.001)
            {
                return;
            }
        }

        g_LastTick = now;
        g_HasLastTick = true;

        g_OnTick(delta);
    }

    // Declared before the function that calls it, defined after — registers
    // itself with FHookStore on construction (DLL load), is Init'd at
    // PreLoad, and enabled at Load by F4SE::Init.
    extern REL::THook<TickFn> g_CallSite;

    void Hook(void* a1, void* a2, void* a3, void* a4, float aBudget)
    {
        g_CallSite(a1, a2, a3, a4, aBudget);
        RunTick();
    }

    REL::THook<TickFn> g_CallSite{
        REL::Offset{ kTickCallSite }, 0, &Hook
    };
}

namespace TLC::Tick
{
    void Install(void (*a_onTick)(double))
    {
        g_OnTick = a_onTick;
    }
}
