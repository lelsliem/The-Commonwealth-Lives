//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/EntityId.h"

#include <cstdint>
#include <unordered_map>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Translator — the edge's memory (ADR-0024: translation at the edge,
    // never inside the core). Two tables answer both questions: which
    // entity is this form, and which form is this entity. Deliberately
    // free of game types — a FormID is a uint32 here — so the tables are
    // testable without the game.
    //-------------------------------------------------------------------------
    class Translator
    {
    public:
        void Add(std::uint32_t formId, LCE::Simulation::EntityId id);
        void Remove(std::uint32_t formId);
        void Clear() noexcept;

        // Invalid entity when the form is unknown (the core's convention
        // for "absent"); 0 when the entity has no form.
        [[nodiscard]] LCE::Simulation::EntityId EntityFor(std::uint32_t formId) const;
        [[nodiscard]] std::uint32_t FormFor(LCE::Simulation::EntityId id) const;
        [[nodiscard]] std::size_t Size() const noexcept;

    private:
        std::unordered_map<std::uint32_t, LCE::Simulation::EntityId> m_EntityByForm;
        std::unordered_map<LCE::Simulation::EntityId, std::uint32_t> m_FormByEntity;
    };
}
