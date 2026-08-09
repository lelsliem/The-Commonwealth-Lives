//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/Needs.h"

#include <cstdint>

namespace TLC
{
    //-------------------------------------------------------------------------
    // FormRef — the entity knows its game form. This is the adapter's half
    // of the translation (ADR-0024): the core reasons over EntityIds, and
    // this component is the edge's memory of which game form that is. The
    // FormID is a plain uint32 here so the component (and its serializer)
    // stay testable without the game — the edge casts at the boundary.
    //-------------------------------------------------------------------------
    struct FormRef
    {
        std::uint32_t FormId = 0;
    };

    //-------------------------------------------------------------------------
    // SeededNeeds — a fresh mind: every need satisfied. Decay rates are the
    // seed defaults; real tuning arrives with the executor stone (the
    // simulation does not tick yet, so these values are initial state).
    //-------------------------------------------------------------------------
    [[nodiscard]]
    inline LCE::Simulation::Needs SeededNeeds()
    {
        using namespace LCE::Simulation;

        return Needs{
            { Need{ NeedType::Hunger, 1.0f, 0.1f },
              Need{ NeedType::Fatigue, 1.0f, 0.1f },
              Need{ NeedType::Social, 1.0f, 0.1f },
              Need{ NeedType::Safety, 1.0f, 0.1f },
              Need{ NeedType::Comfort, 1.0f, 0.1f } }
        };
    }
}
