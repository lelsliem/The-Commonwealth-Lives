//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   The hardest translation is a person into a mind.                                      //
//                                                                             //
//=============================================================================//

#include "Translator.h"

namespace TLC
{
    void Translator::Add(std::uint32_t formId, LCE::Simulation::EntityId id)
    {
        m_EntityByForm[formId] = id;
        m_FormByEntity[id] = formId;
    }

    void Translator::Remove(std::uint32_t formId)
    {
        const auto it = m_EntityByForm.find(formId);

        if (it == m_EntityByForm.end())
        {
            return;
        }

        m_FormByEntity.erase(it->second);
        m_EntityByForm.erase(it);
    }

    void Translator::Clear() noexcept
    {
        m_EntityByForm.clear();
        m_FormByEntity.clear();
    }

    LCE::Simulation::EntityId Translator::EntityFor(std::uint32_t formId) const
    {
        const auto it = m_EntityByForm.find(formId);

        if (it == m_EntityByForm.end())
        {
            return LCE::Simulation::EntityId{};
        }

        return it->second;
    }

    std::uint32_t Translator::FormFor(LCE::Simulation::EntityId id) const
    {
        const auto it = m_FormByEntity.find(id);

        if (it == m_FormByEntity.end())
        {
            return 0;
        }

        return it->second;
    }

    std::size_t Translator::Size() const noexcept
    {
        return m_EntityByForm.size();
    }
}
