//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   The game acts, the sim thinks, the adapter translates.                                      //
//                                                                             //
//=============================================================================//

#include "Adapter.h"

#include "Behaviour.h"
#include "Components.h"
#include "Market.h"
#include "Tuning.h"
#include "WorldFacts.h"
#include "Movement.h"
#include "Serialization.h"
#include "SimRelevant.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/Actor.h>
#include <RE/C/Calendar.h>
#include <RE/N/NiAVObject.h>
#include <RE/P/ProcessLists.h>
#include <RE/S/Sky.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESFormUtil.h>   // the header-only definition of TESForm::As<T>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESWeather.h>

#include <LCE/Logging/Logger.h>

#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Relationships.h"
#include "LCE/Simulation/Simulation.h"

#include <REX/LOG.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Windows.h LAST: its macros (min/max, MEM_*, ...) collide with
// commonlibf4's tokens (REX::W32::MEM_RELEASE, std::numeric_limits::max)
// if it is included before them. NOMINMAX alone is not enough — the
// MEM_* collisions are the reason for the ordering.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>   // GetModuleHandleExW/GetModuleFileNameW (config path)

namespace TLC
{
    namespace
    {
        //-------------------------------------------------------------------------
        // Species classification (ADR-0024: game knowledge at the edge). The
        // core never knows a race; the adapter decides which minds trade,
        // which are fed, and which are children. Race FormIDs verified in
        // xEdit 2026-08-10 from Fallout4.esm. Anything outside the lists
        // defaults to Human — a workshop population is usually people, and a
        // misclassified mind only behaves human until the table grows
        // (Behaviour.h).
        //
        // Enemies (feral ghouls, super mutants, ...) never reach this
        // table: sim-relevance is WorkshopNPCFaction membership, and
        // hostiles do not hold it. The wild-animal entries below are
        // future-proofing — if a mod ever makes one a settler, it is fed,
        // not trading. Robots and synths are deliberately absent: a synth
        // settler is a person (Human is right); a robot is its own species
        // for a later stone (no biological needs to seed).
        //-------------------------------------------------------------------------
        Species ClassifySpecies(const RE::TESRace* a_race)
        {
            if (a_race == nullptr)
            {
                return Species::Human;
            }

            switch (a_race->GetFormID())
            {
            // Children — they play and talk; they don't run stalls.
            case 0x0011D83F:   // HumanChildRace
            case 0x0011EB96:   // GhoulChildRace
                return Species::Child;

            // Animals — fed at the settlement, never bartering.
            case 0x0001D698:   // DogmeatRace (junkyard dog)
            case 0x0001D810:   // MoleratRace
            case 0x0001DB4A:   // DeathclawRace
            case 0x0002047E:   // BrahminRace (pack brahmin)
            case 0x00023FFC:   // MirelurkRace
            case 0x0002456D:   // BloodbugRace
            case 0x00029463:   // BloatflyRace
            case 0x0003578A:   // ViciousDogRace
            case 0x0004716C:   // RadRoachRace
            case 0x0005FBB1:   // StingwingRace
            case 0x000636AB:   // RadScorpionRace
            case 0x00064C60:   // MirelurkHunterRace
            case 0x0006B4EC:   // FeralGhoulRace
            case 0x0007ED1D:   // RadStagRace
            case 0x00090C33:   // FEVHoundRace
            case 0x000A0F2F:   // YaoGuaiRace
            case 0x000A563A:   // EyeBotRace
            case 0x000A96BF:   // FeralGhoulGlowingRace
            case 0x000B7F91:   // MirelurkKingRace
            case 0x000C9ACF:   // CatRace
            case 0x000D77E3:   // VertibirdRace
            case 0x000D9804:   // GorillaRace (settlement gorillas)
            case 0x000E12A6:   // MirelurkQueenRace
            case 0x00187AF9:   // RaiderDogRace
                return Species::Animal;

            default:
                break;
            }

            return Species::Human;
        }

        //-------------------------------------------------------------------------
        // The translation itself (ADR-0024: at the edge). Every loaded
        // actor that is sim-relevant becomes an entity: a FormRef so the
        // entity knows its game form, a SpeciesTag so the sim knows what
        // kind of mind it is, and a fresh mind (satisfied needs seeded for
        // the species, empty memory, empty relationships). The game is
        // read once and never written — the write-through belongs to the
        // executor stone.
        //-------------------------------------------------------------------------
        std::size_t TranslateLoadedActors(
            LCE::Simulation::EntityRegistry& registry,
            Translator& translator,
            const NeedRates& rates)
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

                    const auto species = ClassifySpecies(actor->race);
                    const auto id = registry.CreateEntity();

                    // The desync stone: every mind's needs are born
                    // slightly different (VaryNeeds — deterministic per
                    // entity id), so hunger arrives at different times and
                    // the settlement doesn't march to the market in
                    // lockstep.
                    auto needs = SeededNeeds(species, rates);
                    VaryNeeds(needs, id);

                    registry.AddComponent<FormRef>(id, FormRef{ formId });
                    registry.AddComponent<SpeciesTag>(id, SpeciesTag{ species });
                    registry.AddComponent<Needs>(id, std::move(needs));
                    registry.AddComponent<Goals>(id, SeededGoals(species));
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

        // A settler reaching the market counts as arrived. The game's own
        // command-mode arrival stop leaves walkers ~1 m short of the
        // marker (probes bottom out around 50 units ≈ 0.7 m), so the
        // radius must cover the stop distance — 200 units ≈ 2.8 m — not
        // hug the marker (4 units ≈ 6 cm never fired: walkers stood at
        // the bench outside a 6 cm circle).
        constexpr float kArrivalRadius = 200.0f;

        // How many settlers may walk at once. The command-mode travel
        // package flags each walker as commanded; hundreds at once (the
        // revival world after an aborted load can seed 600+ settler-faction
        // actors near the market) risks a flood — cap the issued walks.
        // The rest are refused this tick and re-decide next.
        constexpr std::size_t kMaxWalks = 16;

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

        //-------------------------------------------------------------------------
        // The config file's anchor: its own code address locates this
        // module (the DLL) so the INI is found next to it, regardless of
        // how F4SE loaded us. A free function — a member function pointer
        // cannot cast to LPCWSTR.
        void ModuleAnchor() {}

        //-------------------------------------------------------------------------
        // The game clock, read once at the edge (the world-facts stone).
        // Sky::currentGameHour is the running hour 0–24 the game itself
        // shows in the HUD clock. Formatted as HH:MM for the log lines
        // the player reads.
        //-------------------------------------------------------------------------
        float CurrentGameHour()
        {
            const auto* sky = RE::Sky::GetSingleton();
            return sky != nullptr ? sky->currentGameHour : 0.0f;
        }

        std::string FormatGameHour(float a_hour)
        {
            const auto whole = static_cast<int>(a_hour);
            const auto minutes = static_cast<int>((a_hour - static_cast<float>(whole)) * 60.0f);

            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "%02d:%02d", whole, minutes);
            return buffer;
        }
    }

    Adapter::Adapter()
    {
        // Once, before any world exists; survives Clear() so Restore (the
        // co-save stone) always has its serializers.
        RegisterAllSerializers(m_Registry);

        // The modder's knob (0.5.0): the config file next to the DLL.
        // Loaded once, before the first world; missing file or broken
        // lines keep the defaults (the core's rule: a broken line must
        // never break the world).
        LoadConfiguration();
    }

    void Adapter::LoadConfiguration()
    {
        // The file lives next to the DLL — Data\F4SE\Plugins\
        // TheLivingCommonwealth.ini. Located via the module's own path so
        // it works regardless of how F4SE loaded us.
        wchar_t modulePath[MAX_PATH]{};
        HMODULE module = nullptr;

        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(&ModuleAnchor),
                &module)
            || GetModuleFileNameW(module, modulePath, MAX_PATH) == 0)
        {
            REX::WARN("tuning: could not locate the plugin module — defaults.");
            return;
        }

        std::filesystem::path ini{ modulePath };
        ini.replace_extension(".ini");

        std::error_code error;

        if (!std::filesystem::exists(ini, error))
        {
            REX::INFO(
                "tuning: no config file ({} expected) — defaults. "
                "Create it to change the sim's feel.",
                ini.string());
            return;
        }

        std::ifstream stream(ini);
        std::stringstream buffer;
        buffer << stream.rdbuf();

        const auto config = Tuning::ParseConfig(buffer.str());

        m_CoreTuning =
            LCE::Simulation::SimulationTuning::FromConfiguration(config);
        m_Settings = Tuning::AdapterSettingsFrom(config);

        REX::INFO(
            "tuning: loaded {} — market {:02.0f}:00–{:02.0f}:00.",
            ini.string(),
            m_Settings.MarketOpenHour,
            m_Settings.MarketCloseHour);
    }

    void Adapter::GameLoaded()
    {
        // One load can complete with both kPostLoadGame and kGameLoaded —
        // the first completion applies the restore; the second must not
        // wipe it with a fresh start. Reset by PreLoadGame; the startup
        // wake (no preceding load) also starts false and is handled once.
        if (m_LoadCompleted)
        {
            return;
        }

        m_LoadCompleted = true;

        // Every completed load is a fresh world — but if the co-save held
        // a world for this save, restore it instead of translating anew:
        // the sim remembers (0.4.0). An empty pending restore (a save made
        // while the sim was not running) falls through to a fresh world.
        if (m_PendingRestore && !m_PendingRestore->Entities.empty())
        {
            ApplyRestore(std::move(*m_PendingRestore));
        }
        else
        {
            EndWorld();
            StartWorld();
        }

        m_PendingRestore.reset();
        m_AwaitingLoad = false;
    }

    LCE::Simulation::RegistrySnapshot Adapter::CaptureWorld() const
    {
        return m_Registry.Capture();
    }

    void Adapter::QueueRestore(LCE::Simulation::RegistrySnapshot a_snapshot)
    {
        m_PendingRestore = std::move(a_snapshot);
    }

    void Adapter::ApplyRestore(LCE::Simulation::RegistrySnapshot a_snapshot)
    {
        // End whatever was running (the pre-load already did, defensively)
        // — Clear keeps the serializers, registered once at init.
        EndWorld();

        m_Registry.Restore(a_snapshot);

        // Rebuild the edge's memory: which form is which entity, from the
        // restored FormRef components. The translator is adapter state,
        // not core state — it never rides inside the snapshot.
        m_Registry.ForEachWithComponent<FormRef>(
            [this](LCE::Simulation::EntityId a_entity, FormRef& a_formRef)
            {
                m_Translator.Add(a_formRef.FormId, a_entity);
            });

        // The market was saved with the world (it owns a FormRef); now
        // that the world is back, every mind must remember where to trade
        // again. The seed is a fading memory event (weight 1.0, forgotten
        // in seconds) — a mind saved long after its world woke has already
        // forgotten the market, and without the seed a restored world is
        // market-blind: starving minds Explore instead of walking. The
        // tick's periodic refresh keeps catching minds whose actors load
        // after this instant.
        SeedMarket(true);

        REX::INFO(
            "The Commonwealth wakes up: {} minds restored from the co-save.",
            a_snapshot.Entities.size());
        LCE::Logging::Info(
            "The Commonwealth wakes up: " + std::to_string(a_snapshot.Entities.size())
            + " minds restored from the co-save.");
        LCE::Logging::Flush();

        m_Started = true;
    }

    void Adapter::PreLoadGame()
    {
        EndWorld();
        m_AwaitingLoad = true;
        m_WorldEndedAt = std::chrono::steady_clock::now();

        // A load is starting — its completion event has not been handled.
        m_LoadCompleted = false;
    }

    void Adapter::DeleteGame()
    {
        EndWorld();
        m_AwaitingLoad = false;
    }

    void Adapter::StartWorld()
    {
        if (m_Started)
        {
            return;
        }

        const auto count =
            TranslateLoadedActors(m_Registry, m_Translator, m_Settings.Rates);

        // The market: if the workshop form is loaded it becomes an entity,
        // and every mind remembers where to trade (ADR-0024 — the adapter
        // reports events; the simulation gives them meaning). A mind that
        // knows the market can decide MoveTo; one that doesn't explores.
        SeedMarket(true);

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
        m_FeederLogged.clear();
        m_StallKeepers.clear();
        m_Walks.clear();
        m_TickCalled = false;
        m_FirstPassLogged = false;
        m_Started = false;
    }

    void Adapter::EnsureWorkshop(std::uint32_t a_formId)
    {
        // Already known — the workshop entity survives within one world.
        if (m_Translator.EntityFor(a_formId).IsValid())
        {
            return;
        }

        // The workshop must be a loaded reference to be walked to.
        if (RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_formId) == nullptr)
        {
            return;
        }

        const auto id = m_Registry.CreateEntity();

        m_Registry.AddComponent<FormRef>(id, FormRef{ a_formId });
        m_Translator.Add(a_formId, id);
    }

    void Adapter::RefreshWorkshops()
    {
        m_Workshops.clear();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();

        if (dataHandler == nullptr)
        {
            return;
        }

        // One pass over the game's REFR form array: every placed ref
        // whose base is the vanilla workshop workbench is a settlement
        // market. Position comes from the ref's data record — valid even
        // for refs in unloaded cells, so the census is complete without
        // loading a single cell. Static per load order, so this runs
        // once and the list survives worlds.
        for (const auto* ref : dataHandler->GetFormArray<RE::TESObjectREFR>())
        {
            if (ref == nullptr)
            {
                continue;
            }

            const auto* base = ref->GetObjectReference();

            if (base == nullptr
                || base->GetFormID() != kWorkshopBaseFormId)
            {
                continue;
            }

            const auto pos = ref->GetPosition();

            m_Workshops.push_back(WorkshopPosition{
                ref->GetFormID(), pos.x, pos.y });
        }

        m_WorkshopsReady = true;

        REX::INFO(
            "settlement census: {} workshops known — markets are per settlement.",
            m_Workshops.size());
    }

    LCE::Simulation::EntityId Adapter::OwnerEntityFor(
        LCE::Simulation::EntityId a_entity)
    {
        const auto formId = m_Translator.FormFor(a_entity);
        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(formId);

        if (actor == nullptr)
        {
            return {};
        }

        const auto* owner = actor->GetOwner();
        LCE::Simulation::EntityId ownerEntity;

        if (owner != nullptr)
        {
            ownerEntity = m_Translator.EntityFor(owner->GetFormID());
        }

        // One line per animal per world — the readout that proves the
        // ownership resolution in-game (the junkyard dog's owner is
        // likely the player, which is no entity — the dog comes home to
        // be fed).
        if (m_FeederLogged.insert(a_entity).second)
        {
            if (owner == nullptr)
            {
                REX::INFO(
                    "LCE: animal {:#x} has no owner — fed by the settlement.",
                    formId);
            }
            else if (ownerEntity.IsValid())
            {
                REX::INFO(
                    "LCE: animal {:#x} is fed by its owner {:#x} (a settler).",
                    formId, owner->GetFormID());
            }
            else
            {
                REX::INFO(
                    "LCE: animal {:#x} has no sim owner ({:#x}) — fed by the settlement.",
                    formId, owner->GetFormID());
            }
        }

        return ownerEntity;
    }

    void Adapter::ReportArrival(
        LCE::Simulation::EntityId a_entity, std::uint32_t a_targetFormId)
    {
        using namespace LCE::Simulation;

        const auto tag = m_Registry.GetComponent<SpeciesTag>(a_entity);
        const auto species = tag != nullptr ? tag->Value : Species::Human;

        const auto target = m_Translator.EntityFor(a_targetFormId);

        if (!target.IsValid())
        {
            return;   // defensive — the walk target is a translated form
        }

        const auto formId = m_Translator.FormFor(a_entity);
        const auto* label = species == Species::Human
            ? "settler"
            : (species == Species::Child ? "child" : "animal");

        // The trade stone: who did this mind meet? The walk target is
        // either the market (a workshop entity — FormRef only, no
        // SpeciesTag) or a person (a translated mind, remembered as a
        // merchant from a previous trade). A human trades with a person;
        // the first human at a market sets up its stall; a child or an
        // animal is fed by whoever resolved as its feeder (the owner, or
        // the settlement).
        LCE::Simulation::EntityId counterparty = target;
        bool traded = false;
        std::uint32_t marketFormId = a_targetFormId;

        if (species == Species::Human)
        {
            const auto targetTag =
                m_Registry.GetComponent<SpeciesTag>(target);

            if (targetTag == nullptr)
            {
                // The bench: resolve the stall-keeper for this market.
                marketFormId = a_targetFormId;

                const auto iterator = m_StallKeepers.find(target);
                const auto stall =
                    iterator != m_StallKeepers.end() ? iterator->second
                                                     : EntityId{};

                if (stall.IsValid() && stall != a_entity)
                {
                    counterparty = stall;
                    traded = true;
                }
                else
                {
                    // No stall yet — this mind sets it up. Honest Partial:
                    // arrived, no customers, nothing changed hands.
                    m_StallKeepers[target] = a_entity;
                    counterparty = target;
                    traded = false;
                }
            }
            else if (targetTag->Value == Species::Human && target != a_entity)
            {
                // The walk resolved to a person — the remembered merchant
                // (the core's ChooseTarget prefers the trader over the
                // bench once a trade exists). Trade with them directly.
                counterparty = target;
                traded = true;
            }
            else
            {
                // Defensive: a non-human mind as a trade target cannot
                // happen (children and animals never enter Trade memory).
                // No trade — the honest Partial.
                counterparty = target;
                traded = false;
            }
        }

        const auto outcome = ArrivalOutcome(species, counterparty, traded);

        ReportOutcome(m_Registry, a_entity, outcome, m_CoreTuning);

        if (species == Species::Human)
        {
            if (traded)
            {
                // The trader's half of the exchange: they remember the
                // sale and warm toward the customer. The buyer's side —
                // trust earned, the goal served — is the core's work via
                // ReportOutcome above.
                auto traderMemory = m_Registry.GetComponent<Memory>(counterparty);
                auto traderRelationships =
                    m_Registry.GetComponent<Relationships>(counterparty);

                if (traderMemory && traderRelationships)
                {
                    RecordSale(
                        *traderMemory, *traderRelationships,
                        a_entity, m_Settings.SaleWarmth);
                }

                const auto traderFormId = m_Translator.FormFor(counterparty);

                REX::INFO(
                    "LCE: settler {:#x} trades with settler {:#x} at market {:#x} — fed, trust earned.",
                    formId, traderFormId, marketFormId);
            }
            else
            {
                REX::INFO(
                    "LCE: settler {:#x} sets up the stall at market {:#x} — trade begins when customers come.",
                    formId, marketFormId);
            }
        }
        else
        {
            REX::INFO(
                "LCE: {} {:#x} arrived — fed, gives nothing in return (Aid, Success).",
                label, formId);
        }

        // The trip pays off (the real test): arriving at the market means
        // food — the settlement's stores feed arrivals, human and animal
        // alike. The need is restored, the loop closes, and the log shows
        // the payoff the sim previously hid.
        auto needs = m_Registry.GetComponent<Needs>(a_entity);

        if (needs)
        {
            const auto previous = RestoreHunger(*needs);

            if (previous >= 0.0f)
            {
                REX::INFO(
                    "LCE: {} {:#x} fed: Hunger {:.2f} -> 1.00",
                    label, formId, previous);
            }
        }
    }

    void Adapter::SeedMarket(bool a_announce)
    {
        // The census: one scan over the REFR form array, once per world.
        // Static per load order, so the result survives worlds.
        if (!m_WorkshopsReady)
        {
            RefreshWorkshops();
        }

        if (m_Workshops.empty())
        {
            // Fallback — no census (no REFRs: an interior, a bare world).
            // A lone known market beats no market: the sim degrades to
            // the walking stone's single-bench behavior rather than
            // forgetting to eat.
            EnsureWorkshop(kMarketFormId);

            const auto market = m_Translator.EntityFor(kMarketFormId);

            if (market.IsValid())
            {
                const auto* marketRef =
                    RE::TESForm::GetFormByID<RE::TESObjectREFR>(kMarketFormId);

                // Only minds whose settler is within walking distance of
                // the market remember it (the probe proved why: the
                // process lists carry settler-faction actors from
                // settlements kilometers away, and every one of them was
                // issued a walk to the Sanctuary bench).
                if (marketRef != nullptr)
                {
                    const auto marketPos = marketRef->GetPosition();

                    SeedMarketMemory(
                        m_Registry, market,
                        [this, market](LCE::Simulation::EntityId a_entity) {
                            // Settlers trade at the market. A child or an
                            // animal is fed — by its owner when the game
                            // assigns one and the owner is a sim entity,
                            // else by the settlement.
                            const auto tag =
                                m_Registry.GetComponent<SpeciesTag>(a_entity);

                            if (tag == nullptr
                                || tag->Value == Species::Human)
                            {
                                return market;
                            }

                            const auto owner = OwnerEntityFor(a_entity);

                            return owner.IsValid() ? owner : market;
                        },
                        [this, marketPos](LCE::Simulation::EntityId a_entity) {
                            const auto formId = m_Translator.FormFor(a_entity);
                            const auto* actor =
                                RE::TESForm::GetFormByID<RE::Actor>(formId);

                            if (actor == nullptr)
                            {
                                return false;
                            }

                            const auto pos = actor->GetPosition();
                            const auto dx = pos.x - marketPos.x;
                            const auto dy = pos.y - marketPos.y;

                            return std::sqrt(dx * dx + dy * dy) < kMarketRadius;
                        });

                    if (a_announce)
                    {
                        const auto hour = CurrentGameHour();

                        if (WorldFacts::IsMarketClosed(
                                hour, m_Settings.MarketOpenHour,
                                m_Settings.MarketCloseHour))
                        {
                            REX::INFO(
                                "The market is remembered but closed ({}): trade resumes at {:02.0f}:00.",
                                FormatGameHour(hour), m_Settings.MarketOpenHour);
                        }
                        else
                        {
                            REX::INFO("The market is open: every nearby mind remembers where to trade (000250FE — the Sanctuary workshop).");
                        }
                    }
                }
            }
            else if (a_announce)
            {
                REX::INFO("The market is not loaded — settlers explore until it is.");
            }

            return;
        }

        // Per-settlement markets: every workshop is a market entity, and
        // every mind remembers its own — the nearest workshop within
        // range of where it stands. A settler in Sanctuary knows the
        // Sanctuary bench; a settler at Warwick knows Warwick's; a mind
        // in the wastes knows none and explores until it finds one.
        for (const auto& workshop : m_Workshops)
        {
            EnsureWorkshop(workshop.FormId);
        }

        SeedMarketMemory(
            m_Registry, {},
            [this](LCE::Simulation::EntityId a_entity) {
                // Where is this mind? Its actor must be loaded to know —
                // restored minds load gradually, and the periodic seed
                // catches them a second later.
                const auto formId = m_Translator.FormFor(a_entity);
                const auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(formId);

                if (actor == nullptr)
                {
                    return LCE::Simulation::EntityId{};
                }

                const auto pos = actor->GetPosition();
                const auto nearest = NearestWorkshop(
                    pos.x, pos.y, m_Workshops, kMarketRadius);

                if (nearest == 0)
                {
                    return LCE::Simulation::EntityId{};   // in the wastes
                }

                const auto market = m_Translator.EntityFor(nearest);

                if (!market.IsValid())
                {
                    return LCE::Simulation::EntityId{};
                }

                // Settlers trade at their settlement's market. A child or
                // an animal is fed — by its owner when the game assigns
                // one and the owner is a sim entity, else by the
                // settlement (the player is no entity — a player-owned
                // dog comes home to be fed).
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value == Species::Human)
                {
                    return market;
                }

                const auto owner = OwnerEntityFor(a_entity);

                return owner.IsValid() ? owner : market;
            });

        if (a_announce)
        {
            // Hour-aware since the world-facts stone: the classic line
            // only when the market is actually open. At night the seed
            // still plants *where* the market is (the location memory is
            // independent of the hours gate) — the announce just tells
            // the truth about what the world is doing now.
            const auto hour = CurrentGameHour();

            if (WorldFacts::IsMarketClosed(
                    hour, m_Settings.MarketOpenHour,
                    m_Settings.MarketCloseHour))
            {
                REX::INFO(
                    "The market is remembered but closed ({}): trade resumes at {:02.0f}:00.",
                    FormatGameHour(hour), m_Settings.MarketOpenHour);
            }
            else
            {
                REX::INFO(
                    "The market is open: every mind remembers where its own settlement trades ({} workshops).",
                    m_Workshops.size());
            }
        }
    }

    bool Adapter::IsRadstorm(const RE::TESWeather* a_weather) const
    {
        // Radstorms shut the gatherings: a { invalid, Social } world fact
        // — nobody meets in the green air. Verified against the full xEdit
        // weather list 2026-08-10: only CommonwealthGSRadstorm (001C3D5E)
        // is the live radstorm. The other GS radstorm forms were reviewed
        // and deliberately excluded — Old/Backup (00222394/002392A3) are
        // editor records the game never sets, and NoHazard (0024A3C0)
        // removes the hazard by design: the gate is the green air, not
        // the colour of the sky.
        if (a_weather != nullptr)
        {
            switch (a_weather->GetFormID())
            {
            case 0x001C3D5E:   // CommonwealthGSRadstorm
                return true;
            default:
                break;
            }
        }

        return false;
    }

    void Adapter::PushWorldFacts()
    {
        using namespace LCE::Simulation;

        const auto hour = CurrentGameHour();
        const bool closed = WorldFacts::IsMarketClosed(
            hour, m_Settings.MarketOpenHour, m_Settings.MarketCloseHour);

        const auto* sky = RE::Sky::GetSingleton();
        const bool radstorm =
            IsRadstorm(sky != nullptr ? sky->currentWeather : nullptr);

        // Transitions only — these are the lines the player reads. The
        // push below is silent; announcing every second would drown the
        // log in facts.
        if (closed != m_MarketClosed)
        {
            m_MarketClosed = closed;

            if (closed)
            {
                REX::INFO(
                    "world fact: the market is closed ({}) — trade unavailable until {:02.0f}:00.",
                    FormatGameHour(hour), m_Settings.MarketOpenHour);
            }
            else
            {
                REX::INFO(
                    "world fact: the market is open ({}) — trade available.",
                    FormatGameHour(hour));
            }
        }

        if (radstorm != m_Radstorm)
        {
            m_Radstorm = radstorm;

            if (radstorm)
            {
                REX::INFO("world fact: a radstorm rolls in — no one gathers while it lasts.");
            }
            else
            {
                REX::INFO("world fact: the radstorm passes — gatherings resume.");
            }
        }

        // --- The day's weather: a memory, not a door. --------------------
        // The sky is classified into a category and today's categories are
        // pushed as day-stamped world facts ({ invalid, WeatherRain, 1.0,
        // day } — "day 12 was rainy"). These kinds are never gated, so
        // rain never closes the market; they are pure labels the sim
        // remembers. Re-derived at the edge: after a load the re-push
        // re-seeds today's sky within a second, so weather never needs
        // the co-save. Weather is global knowledge — every mind shares
        // the same sky — so, like the gates, it reaches all minds.
        const auto* calendar = RE::Calendar::GetSingleton();
        const auto day = std::uint64_t(
            calendar != nullptr && calendar->gameDaysPassed != nullptr
                ? calendar->gameDaysPassed->value
                : 0.0f);

        const auto weather = WorldFacts::ClassifyWeather(
            sky != nullptr && sky->currentWeather != nullptr
                ? sky->currentWeather->GetFormID()
                : 0);

        if (day != m_WeatherDay)
        {
            // The world turned: yesterday's categories are left to fade
            // and today's start empty. The turn gets a line — it proves
            // the day tracking in the log.
            m_WeatherDay = day;
            m_WeatherSeen = 0;

            REX::INFO(
                "world fact: the world turns — day {} begins, the sky {}.",
                day, WorldFacts::WeatherLabel(weather));
        }
        else if (weather != m_Weather)
        {
            // A sky change within the day — transitions only.
            m_Weather = weather;

            if (weather != WorldFacts::WeatherKind::Unknown)
            {
                REX::INFO(
                    "world fact: the sky turns {} (day {}) — the day's weather is remembered.",
                    WorldFacts::WeatherLabel(weather), day);
            }
            else
            {
                REX::INFO("world fact: the sky is unclassified — no weather memory.");
            }
        }

        if (weather != WorldFacts::WeatherKind::Unknown)
        {
            // The seen-set is a bitmask over WeatherKind (Clear=1..Radstorm=6).
            m_WeatherSeen |= 1u << (static_cast<unsigned>(weather) - 1u);
        }

        // Refresh every category seen today, stamped with today — "it
        // rained this morning" stays remembered until the world turns.
        // Yesterday's categories are not refreshed, and the tick fades
        // them out: the designed forget.
        if (m_WeatherSeen != 0)
        {
            m_Registry.ForEachWithComponent<Memory>(
                [this, day](EntityId, Memory& a_memory)
                {
                    for (unsigned i = 0; i < 6; ++i)
                    {
                        if ((m_WeatherSeen & (1u << i)) == 0)
                        {
                            continue;
                        }

                        const auto factKind = WorldFacts::WeatherFactKind(
                            static_cast<WorldFacts::WeatherKind>(i + 1));

                        if (factKind)
                        {
                            WorldFacts::ApplyFact(
                                a_memory, *factKind, true, day);
                        }
                    }
                });
        }

        if (!closed && !radstorm)
        {
            return;   // no doors shut — nothing to push
        }

        // The refresh pattern, one door at a time (WorldFacts::ApplyFact):
        // while a door is shut, top the fact back to full weight every
        // second so the tick's fade never erases it — no flicker, no
        // memory growth. When the condition flips, refreshing stops and
        // the tick fades the fact out in ~4.5 s: the close-down the
        // transition line promised. World facts are global knowledge —
        // every mind hears the market shut, loaded or not, near or far
        // (unlike the market-location seed, which is radius-filtered).
        m_Registry.ForEachWithComponent<Memory>(
            [closed, radstorm](EntityId, Memory& a_memory)
            {
                WorldFacts::ApplyFact(
                    a_memory, InteractionKind::Trade, closed);
                WorldFacts::ApplyFact(
                    a_memory, InteractionKind::Social, radstorm);
            });
    }

    void Adapter::Tick(double a_deltaSeconds)
    {
        if (!m_Started)
        {
            // Abort recovery: a PreLoadGame armed a pending load. Two
            // cases, and the window separates them:
            //   - a REAL load is slow: a 600-actor co-save world can take
            //     far longer than 12s to finish. The old 12s window fired
            //     mid-load, revived a world while the game was still
            //     loading, and that revival killed the load — every
            //     in-game load "aborted" at exactly 12s, every session.
            //   - a true phantom load (the pre-DisableExitSave exit-save
            //     reload) never completes at all.
            // The window is the patience: 60s lets real loads finish; a
            // phantom past that is dead, and the sim revives to survive.
            if (m_AwaitingLoad
                && std::chrono::steady_clock::now() - m_WorldEndedAt
                    > std::chrono::seconds(60))
            {
                m_AwaitingLoad = false;

                // The aborted load's co-save snapshot (if one was read
                // before the abort) belongs to a world that never
                // existed — discard it, or a later, unrelated GameLoaded
                // would restore a stale world over a fresh one.
                if (m_PendingRestore)
                {
                    REX::INFO(
                        "lifecycle: the aborted load's co-save is discarded — "
                        "the world revives fresh.");
                    m_PendingRestore.reset();
                }

                REX::INFO("lifecycle: the pending load aborted — reviving the world.");
                StartWorld();
                return;
            }

            // One-time proof the tick hook fires even before a world
            // exists (the hooks install at Load; worlds start on
            // GameLoaded). If neither this nor the first-pass line ever
            // appears, the game is not simulating — a paused, unfocused,
            // or occluded window throttles the per-frame VM ticks.
            if (!m_TickCalled)
            {
                m_TickCalled = true;
                REX::INFO("Tick: called before the world started (once).");
            }
            return;
        }

        using namespace LCE::Simulation;

        // The market stays open: re-push the fact every second so minds
        // whose actors load after the world started (a restore brings 637
        // entities back, but their actors load gradually) learn where to
        // trade. Idempotent — SeedMarketMemory skips minds that already
        // remember — so this only ever adds to the truly forgotten.
        // Silent: the world-start call announced it.
        if (std::chrono::steady_clock::now() - m_LastMarketSeed
            > std::chrono::seconds(1))
        {
            m_LastMarketSeed = std::chrono::steady_clock::now();
            SeedMarket(false);

            // The world's doors: the market's trading hours and the
            // weather. Pushed on the same cadence as the seed — silent
            // unless a door changes.
            PushWorldFacts();
        }

        // The core's stateless tick: needs decay, memory fade, goal
        // urgency, then one Intent per mind. All of it on the game thread,
        // with the modder's tuning (the config file) when present.
        Update(m_Registry, a_deltaSeconds, m_CoreTuning);

        // The read + the table. "Already acting" is a future refinement —
        // every loaded settler is available for now.
        const auto plan = BuildPlan(
            m_Registry,
            [this](EntityId a_entity) { return IsActorLoaded(m_Translator, a_entity); },
            [this](EntityId a_entity) { return IsTargetLoaded(m_Translator, a_entity); },
            [](EntityId) { return true; });

        ExecutePlan(plan);
        ProbeWalks();

        // One-time proof the whole first pass completed. If the intent
        // lines printed but this is missing, the pass is stuck in
        // ProbeWalks; if neither printed, the hooks didn't fire.
        if (!m_FirstPassLogged)
        {
            m_FirstPassLogged = true;
            REX::INFO(
                "Tick: first pass complete — {} intents, {} active walks.",
                plan.size(), m_Walks.size());
        }
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
                auto* target = RE::TESForm::GetFormByID<RE::TESObjectREFR>(targetFormId);

                if (target == nullptr)
                {
                    continue;   // defensive — TargetLoaded already checked
                }

                // The adapter walks the settler; the core never does
                // (ADR-0024). Refusal leaves the sim to re-decide.
                //
                // The walk session: while the Trade memory lasts the intent
                // stays MoveTo and would re-issue the planner every frame.
                // Issue each walk once per session; the game's command
                // package keeps walking the settler to the destination on
                // its own. The session outlives the intent (ProbeWalks
                // measures it) because the memory fades long before the
                // walk completes.
                auto& session = m_Walks[entry.Entity];
                const auto now = std::chrono::steady_clock::now();

                bool walked = false;

                if (session.Target == targetFormId
                    && now - session.Issued < std::chrono::seconds(60))
                {
                    walked = true;   // already walking that way
                }
                else if (m_Walks.size() >= kMaxWalks)
                {
                    // Walk cap: erase the session so a refused walk never
                    // lingers (a zombie session — Issued at the epoch —
                    // made ProbeWalks log an instant "ended" line every
                    // frame for every refused walk; that flood preceded
                    // the crash). The mind re-decides next tick.
                    m_Walks.erase(entry.Entity);
                }
                else
                {
                    walked = Movement::WalkTo(actor, target);

                    if (walked)
                    {
                        session.Target = targetFormId;
                        session.Issued = now;
                    }
                    else
                    {
                        // A refused walk ends the session — erase, don't
                        // reset (see the cap branch: a reset leaves a
                        // zombie that ProbeWalks logs as instantly ended).
                        m_Walks.erase(entry.Entity);
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
                // The session is deliberately kept: the walk was issued and
                // the game's planner carries it, so ProbeWalks still
                // measures progress (and arrival) after the memory fades.

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

    void Adapter::ProbeWalks()
    {
        using namespace LCE::Simulation;

        const auto now = std::chrono::steady_clock::now();

        for (auto it = m_Walks.begin(); it != m_Walks.end();)
        {
            auto& session = it->second;

            // A never-issued session (default Issued) is a zombie — drop it
            // silently rather than log it as an instant "ended" (the flood
            // that preceded the crash). ExecutePlan now erases on refusal,
            // so this is defense in depth.
            if (session.Issued == std::chrono::steady_clock::time_point{})
            {
                it = m_Walks.erase(it);
                continue;
            }

            // A session ends 60s after issue — the walk is issued once and
            // the game's planner carries it from there; 60s covers the
            // market radius (≈140 m) even at a slow walk.
            if (now - session.Issued >= std::chrono::seconds(60))
            {
                if (!session.Reached)
                {
                    REX::DEBUG(
                        "LCE: walk session settler {} ended (closest approach {:.1f} m).",
                        FormatHex8(m_Translator.FormFor(it->first)),
                        session.MinDistance);
                }

                it = m_Walks.erase(it);
                continue;
            }

            if (session.Reached)
            {
                ++it;
                continue;
            }

            // Resolve the pair: the walker and the destination it was told
            // to walk to.
            const auto actorFormId = m_Translator.FormFor(it->first);
            const auto* actor = RE::TESForm::GetFormByID<RE::Actor>(actorFormId);
            const auto* target =
                RE::TESForm::GetFormByID<RE::TESObjectREFR>(session.Target);

            if (actor == nullptr || target == nullptr)
            {
                ++it;   // unloaded — nothing to measure this tick
                continue;
            }

            // Live position: prefer the 3D node (the render position —
            // always current); some actors report no 3D from Get3D(), so
            // try GetFullyLoaded3D() too, then fall back to the stored
            // position (a hard skip once silenced the probe entirely).
            // The reading is tagged so the source is interpretable.
            const auto* node = actor->Get3D();

            if (node == nullptr)
            {
                node = actor->GetFullyLoaded3D();
            }

            const auto from = node != nullptr
                ? node->GetWorldTransform().translate
                : actor->GetPosition();
            const bool live = node != nullptr;
            const auto to = target->GetPosition();
            const auto dx = from.x - to.x;
            const auto dy = from.y - to.y;
            const auto d = std::sqrt(dx * dx + dy * dy);

            if (d < session.MinDistance)
            {
                session.MinDistance = d;
            }

            if (d < kArrivalRadius)
            {
                session.Reached = true;
                ReportArrival(it->first, session.Target);
                REX::INFO(
                    "LCE: settler {} arrived (d = {:.1f} m).",
                    FormatHex8(actorFormId), d);
            }
            else if (now - session.LastProbe >= std::chrono::seconds(2)
                && (session.LastDistance < 0.0f
                    || std::fabs(d - session.LastDistance) >= 1.0f))
            {
                session.LastProbe = now;
                session.LastDistance = d;
                REX::DEBUG(
                    "LCE: walk probe settler {} -> {} d = {:.1f} m (min {:.1f} m){}.",
                    FormatHex8(actorFormId), FormatHex8(session.Target), d,
                    session.MinDistance, live ? " [live]" : " [stored]");
            }

            ++it;
        }
    }
}
