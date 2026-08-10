//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   First the belly, then the Commonwealth.                                    //
//                                                                             //
//=============================================================================//

#pragma once

#include "Behaviour.h"

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
    // SpeciesTag — what kind of mind this entity is (Behaviour.h). Set once
    // at translation from the actor's race; persisted in the co-save so a
    // restored world keeps its dogs dogs. A missing tag reads as Human —
    // the default for a workshop population.
    //-------------------------------------------------------------------------
    struct SpeciesTag
    {
        Species Value = Species::Human;
    };
}
