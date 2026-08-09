//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

namespace TLC
{
    //-------------------------------------------------------------------------
    // Tick — the simulation's heartbeat inside the game. A per-frame hook on
    // the game's own per-frame pump (DelayFunctorQueue — the budget-ticked
    // delay-functor queue F4SE itself hooks to fire OnUpdate), installed with
    // the library's own THook machinery and enabled at Load. Runs on the
    // game thread; zero contention (the contract's 0.4.0 threading decision).
    //-------------------------------------------------------------------------
    namespace Tick
    {
        // Installs the frame callback: called on the game thread, once per
        // frame, with the real seconds since the previous tick.
        void Install(void (*a_onTick)(double));
    }
}
