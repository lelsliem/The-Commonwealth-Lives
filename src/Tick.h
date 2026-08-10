//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   It's not the years, it's the frames.                                      //
//                                                                             //
//=============================================================================//

#pragma once

namespace TLC
{
    //-------------------------------------------------------------------------
    // Tick — the simulation's heartbeat inside the game. A per-frame hook on
    // the game's frame driver, installed with the library's own THook
    // machinery and enabled at Load. Runs on the game thread; zero
    // contention (the contract's 0.4.0 threading decision).
    //
    // The hook sites are pinned to 1.11.221 (mid-function call sites are
    // not in the address library — the same discipline F4SE uses for its
    // own offsets). The sites carry per-hook proof logging so the log
    // shows which paths fire; dead sites are pruned once verified.
    //-------------------------------------------------------------------------
    namespace Tick
    {
        // Installs the frame callback: called on the game thread, once per
        // frame, with the real seconds since the previous tick.
        void Install(void (*a_onTick)(double));
    }
}
