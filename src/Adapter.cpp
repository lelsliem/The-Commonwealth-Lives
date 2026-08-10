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
#include "Market.h"
#include "Movement.h"
#include "Serialization.h"
#include "SimRelevant.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/Actor.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESFormUtil.h>   // the header-only definition of TESForm::As<T>
#include <RE/T/TESObjectREFR.h>

#include <LCE/Logging/Logger.h>

#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Relationships.h"
#include "LCE/Simulation/Simulation.h"

#include <REX/LOG.h>

#include <cstdio>
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

        //-------------------------------------------------------------------------
        // The executor's game answers (ADR-0024: every RE:: touch at the
        // edge). The pure plan builder asks "loaded? available?" — these
        // functions answer with the real game.
        //-------------------------------------------------------------------------
        const char* ActionName(LCE::Simulation::ActionType a_action)
        {
            using enum LCE::Simulation::ActionType;

            switch (a_action)
            {
            case MoveTo:
                return "MoveTo";
            case Rest:
                return "Rest";
            case Socialize:
                return "Socialize";
            case Explore:
                return "Explore";
            case Work:
                return "Work";
            case Flee:
                return "Flee";
            }

            return "?";
        }

        std::string FormatHex8(std::uint32_t a_value)
        {
            char buffer[9];
            std::snprintf(buffer, sizeof(buffer), "%08X", a_value);
            return buffer;
        }

        // The entity's form, or null when the form is unknown.
        RE::TESForm* FormFor(const Translator& a_translator, LCE::Simulation::EntityId a_entity)
        {
            const auto formId = a_translator.FormFor(a_entity);
            return formId != 0 ? RE::TESForm::GetFormByID(formId) : nullptr;
        }

        bool IsActorLoaded(const Translator& a_translator, LCE::Simulation::EntityId a_entity)
        {
            const auto formId = a_translator.FormFor(a_entity);
            return formId != 0 && RE::TESForm::GetFormByID<RE::Actor>(formId) != nullptr;
        }

        bool IsTargetLoaded(const Translator& a_translator, LCE::Simulation::EntityId a_entity)
        {
            if (!a_entity.IsValid())
            {
                return false;
            }

            const auto formId = a_translator.FormFor(a_entity);
            return formId != 0 && RE::TESForm::GetFormByID<RE::TESObjectREFR>(formId) != nullptr;
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

        // The market: if the workshop form is loaded it becomes an entity,
        // and every mind remembers where to trade (ADR-0024 — the adapter
        // reports events; the simulation gives them meaning). A mind that
        // knows the market can decide MoveTo; one that doesn't explores.
        EnsureMarket();

        const auto market = m_Translator.EntityFor(kMarketFormId);

        if (market.IsValid())
        {
            SeedMarketMemory(m_Registry, market);

            REX::INFO("The market is open: every mind remembers where to trade (000250FE — the Sanctuary workshop).");
        }
        else
        {
            REX::INFO("The market is not loaded — settlers explore until it is.");
        }

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
        m_LastLogged.clear();
        m_Walks.clear();
        m_Started = false;
    }

    void Adapter::EnsureMarket()
    {
        // Already known — the market entity survives within one world.
        if (m_Translator.EntityFor(kMarketFormId).IsValid())
        {
            return;
        }

        // The workshop form must be a loaded reference to be walked to.
        if (RE::TESForm::GetFormByID<RE::TESObjectREFR>(kMarketFormId) == nullptr)
        {
            return;
        }

        const auto id = m_Registry.CreateEntity();

        m_Registry.AddComponent<FormRef>(id, FormRef{ kMarketFormId });
        m_Translator.Add(kMarketFormId, id);
    }

    void Adapter::Tick(double a_deltaSeconds)
    {
        if (!m_Started)
        {
            return;
        }

        using namespace LCE::Simulation;

        // The core's stateless tick: needs decay, memory fade, goal
        // urgency, then one Intent per mind. All of it on the game thread.
        Update(m_Registry, a_deltaSeconds);

        // The read + the table. "Already acting" is a future refinement —
        // every loaded settler is available for now.
        const auto plan = BuildPlan(
            m_Registry,
            [this](EntityId a_entity) { return IsActorLoaded(m_Translator, a_entity); },
            [this](EntityId a_entity) { return IsTargetLoaded(m_Translator, a_entity); },
            [](EntityId) { return true; });

        ExecutePlan(plan);
    }

    void Adapter::ExecutePlan(const std::vector<PlanEntry>& a_plan)
    {
        using namespace LCE::Simulation;

        for (const auto& entry : a_plan)
        {
            const auto actorFormId = m_Translator.FormFor(entry.Entity);
            const auto targetFormId = m_Translator.FormFor(entry.Intent.Target);

            // Refusal is the contract (the intent is a hint, not a command):
            // an unloaded actor, an unloaded target, or a busy actor. The
            // dropped intent is simply re-decided next tick — nothing queued.
            if (!entry.ActorLoaded)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    "settler " + FormatHex8(actorFormId) + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + FormatHex8(targetFormId) + " — refused: actor not loaded",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId, 1 });
                continue;
            }

            if (!entry.TargetLoaded)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    "settler " + FormatHex8(actorFormId) + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + FormatHex8(targetFormId) + " — refused: target not loaded",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId, 2 });
                continue;
            }

            if (!entry.Available)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    "settler " + FormatHex8(actorFormId) + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + FormatHex8(targetFormId) + " — refused: actor busy",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId, 3 });
                continue;
            }

            auto* actor = RE::TESForm::GetFormByID<RE::Actor>(actorFormId);

            if (actor == nullptr)
            {
                continue;   // defensive — ActorLoaded already checked
            }

            switch (entry.Intent.Action)
            {
            case ActionType::MoveTo:
            {
                const auto* target = RE::TESForm::GetFormByID<RE::TESObjectREFR>(targetFormId);

                if (target == nullptr)
                {
                    continue;   // defensive — TargetLoaded already checked
                }

                // The adapter walks the settler; the core never does
                // (ADR-0024). Refusal leaves the sim to re-decide.
                //
                // The walk session: while the Trade memory lasts the intent
                // stays MoveTo and would re-issue the planner every frame.
                // Issue each walk once per session; the game's planner keeps
                // walking the settler to the destination on its own.
                auto& session = m_Walks[entry.Entity];
                const auto now = std::chrono::steady_clock::now();

                bool walked = false;

                if (session.Target == targetFormId
                    && now - session.Issued < std::chrono::seconds(30))
                {
                    walked = true;   // already walking that way
                }
                else
                {
                    walked = Movement::WalkTo(actor, target->GetPosition());

                    if (walked)
                    {
                        session.Target = targetFormId;
                        session.Issued = now;
                    }
                    else
                    {
                        session = {};   // a refused walk ends the session
                    }
                }

                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                LogPlanEntry(
                    entry.Entity,
                    "settler " + FormatHex8(actorFormId) + " decides MoveTo -> " + FormatHex8(targetFormId)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId,
                        walked ? 0u : 4u });
            }
            break;

            case ActionType::Rest:
            case ActionType::Socialize:
            case ActionType::Explore:
            case ActionType::Work:
            case ActionType::Flee:
            {
                // A mind that decides something else is no longer walking
                // to market — its walk session ends.
                m_Walks.erase(entry.Entity);

                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                // Table slots: the loop is proven; the game behaviours are
                // the next stones' work.
                LogPlanEntry(
                    entry.Entity,
                    "settler " + FormatHex8(actorFormId) + " decides " + ActionName(entry.Intent.Action)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), 0, 0 });
            }
            break;
            }
        }
    }

    void Adapter::LogPlanEntry(
        LCE::Simulation::EntityId a_entity,
        std::string a_message,
        const LogKey& a_key)
    {
        const auto it = m_LastLogged.find(a_entity);

        if (it != m_LastLogged.end() && it->second == a_key)
        {
            return;   // unchanged — no per-frame spam
        }

        m_LastLogged[a_entity] = a_key;
        REX::INFO("{}", a_message);
    }
}
