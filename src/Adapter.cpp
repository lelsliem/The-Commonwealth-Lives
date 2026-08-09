//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#include "Adapter.h"

#include "Components.h"
#include "Serialization.h"
#include "SimRelevant.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/Actor.h>
#include <RE/P/ProcessLists.h>

#include <LCE/Logging/Logger.h>

#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Relationships.h"

#include <REX/LOG.h>

#include <string>

namespace TLC
{
    namespace
    {
        //-------------------------------------------------------------------------
        // The translation itself (ADR-0024: at the edge). Every loaded
        // actor that is sim-relevant becomes an entity: a FormRef so the
        // entity knows its game form, and a fresh mind (satisfied needs,
        // empty memory, empty relationships). The game is read once and
        // never written — the write-through belongs to the executor stone.
        //-------------------------------------------------------------------------
        std::size_t TranslateLoadedActors(
            LCE::Simulation::EntityRegistry& registry,
            Translator& translator)
        {
            using namespace LCE::Simulation;

            const auto* processLists = RE::ProcessLists::GetSingleton();

            if (!processLists)
            {
                return 0;
            }

            std::size_t count = 0;

            // The four process lists: high, low, middle-high, middle-low —
            // every actor currently loaded and simulated near the player.
            for (const auto* list : processLists->allProcesss)
            {
                if (!list)
                {
                    continue;
                }

                for (const auto& handle : *list)
                {
                    // handle.get() is a NiPointer<Actor>; .get() is the raw
                    // pointer the predicate and the registry want.
                    const auto* actor = handle.get().get();

                    if (!actor || !IsSimRelevant(actor))
                    {
                        continue;
                    }

                    const auto formId = actor->GetFormID();

                    // Defensive: an actor may appear in more than one list.
                    if (translator.EntityFor(formId).IsValid())
                    {
                        continue;
                    }

                    const auto id = registry.CreateEntity();

                    registry.AddComponent<FormRef>(id, FormRef{ formId });
                    registry.AddComponent<Needs>(id, SeededNeeds());
                    registry.AddComponent<Memory>(id, Memory{});
                    registry.AddComponent<Relationships>(id, Relationships{});

                    translator.Add(formId, id);
                    ++count;
                }
            }

            return count;
        }
    }

    Adapter::Adapter()
    {
        // Once, before any world exists; survives Clear() so Restore (the
        // co-save stone) always has its serializers.
        RegisterAllSerializers(m_Registry);
    }

    void Adapter::StartWorld()
    {
        if (m_Started)
        {
            return;
        }

        const auto count = TranslateLoadedActors(m_Registry, m_Translator);

        REX::INFO("The Commonwealth wakes up: {} settlers became minds.", count);
        LCE::Logging::Info(
            "The Commonwealth wakes up: " + std::to_string(count) + " settlers became minds.");
        LCE::Logging::Flush();

        m_Started = true;
    }

    void Adapter::EndWorld()
    {
        if (!m_Started)
        {
            return;
        }

        // Clear keeps the serializers (registered once at init) so the
        // next StartWorld can translate fresh. The co-save stone will
        // replace this with Capture/Restore.
        m_Registry.Clear();
        m_Translator.Clear();
        m_Started = false;
    }
}
