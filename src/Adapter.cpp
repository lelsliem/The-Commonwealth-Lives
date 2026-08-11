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
#include <RE/B/BSContainer.h>
#include <RE/C/Calendar.h>
#include <RE/N/NiAVObject.h>
#include <RE/P/ProcessLists.h>
#include <RE/S/Sky.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWorldSpace.h>
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
#include <typeindex>
#include <typeinfo>

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

        // The world's voice for a mind — the same labels the arrival
        // logging uses, read from the mind's tag (a missing tag reads as
        // a settler, the workshop default).
        const char* SpeciesLabel(
            const SpeciesTag* a_tag)
        {
            if (a_tag == nullptr)
            {
                return "settler";
            }

            switch (a_tag->Value)
            {
            case Species::Child:
                return "child";
            case Species::Animal:
                return "animal";
            default:
                return "settler";
            }
        }

        //-------------------------------------------------------------------------
        // The per-second census sweep (0.6.0 Stone 1). ForEachLoadedActor
        // visits every actor the game is simulating near the player — the
        // same four process lists the wake seed reads; the classification
        // (arrival / death / departure) is Lifecycle::Diff, pure.
        //-------------------------------------------------------------------------
        template <class Fn>
        void ForEachLoadedActor(Fn&& a_fn)
        {
            const auto* processLists = RE::ProcessLists::GetSingleton();

            if (!processLists)
            {
                return;
            }

            for (const auto* list : processLists->allProcesss)
            {
                if (!list)
                {
                    continue;
                }

                for (const auto& handle : *list)
                {
                    // handle.get() is a NiPointer<Actor>; .get() is the
                    // raw pointer the callers want.
                    auto* actor = handle.get().get();

                    if (actor == nullptr || actor->IsPlayerRef())
                    {
                        continue;
                    }

                    a_fn(actor);
                }
            }
        }

        // IsActorDead — the game's own death markers, read only on a
        // fully streamed-in actor. A killer handle is set the moment an
        // actor is killed, a corpse-cleanup timer runs while the body
        // awaits cleanup, and a deleted ref is removed from the world;
        // any of the three means the mind is gone. The 3D gate matters:
        // after a load, actors enter the process lists BEFORE their
        // members are initialized — reading myKiller / the corpse timer /
        // the form flags on a partially-initialized actor is garbage, and
        // in-game that read 11 false deaths in one frame, 3s after a
        // 665-mind restore (2026-08-10). A fully-loaded actor has a 3D
        // node, and a corpse keeps it — so the raw reads only happen once
        // the actor is really there.
        inline bool IsActorDead(RE::Actor* a_actor)
        {
            if (a_actor == nullptr)
            {
                return true;
            }

            // Not fully streamed in yet — treat as alive; the census
            // re-reads next second, when the members are real.
            if (a_actor->Get3D() == nullptr)
            {
                return false;
            }

            return a_actor->IsDeleted()
                || a_actor->myKiller.get().get() != nullptr
                || a_actor->checkMyDeadBodyTimer > 0.0f;
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

        // The observation bus (Request A — stone 08): the core publishes
        // RelationshipChangedEvent when a configured disposition line is
        // crossed. Subscribed once for the adapter's lifetime; the
        // handler reads the live relationships and applies the same
        // derivation the 1-second pass uses, so the two channels never
        // disagree. No logging here — the constructor runs before the
        // logger attaches.
        m_Bus.Subscribe(
            std::type_index(typeid(LCE::Simulation::RelationshipChangedEvent)),
            [this](const LCE::Events::Event& a_event)
            {
                OnRelationshipChanged(
                    static_cast<const LCE::Simulation::RelationshipChangedEvent&>(
                        a_event));
            });

        // Tuning is loaded from GameLoaded (not here): the constructor
        // runs before the logger attaches, so its confirmation lines
        // would be silently dropped — and the modder should see them.
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

        // The feeling rhythm (0.6.0 Stone 2): the core's drift default
        // (0.05/s — half-life ~14 s) was tuned for a fast demo and
        // erases a shared meal's warmth between meals (minutes apart),
        // so no relationship can ever accumulate. The adapter's world
        // runs the same slow clock the shipped INI sets (sim.drift.rate)
        // when the config names none — the adapter's defaults ARE the
        // living-world defaults, exactly like the bond lines below.
        const auto driftInjected = config.Get("sim.drift.rate").empty();

        if (driftInjected)
        {
            m_CoreTuning.DriftRate = Tuning::kLivingDriftRate;
        }

        // Bonds (0.6.0 Stone 2): the core's watch-list is empty unless
        // the world names its lines — the adapter names them here, so a
        // world without a config file still bonds (friend +0.3 through
        // enemy −0.6). The INI's sim.bond.threshold.<name> keys override
        // whatever they set; the same values drive the core's events and
        // the adapter's derivation.
        const auto bondDefaultsInjected =
            m_CoreTuning.BondThresholds.empty();

        if (bondDefaultsInjected)
        {
            m_CoreTuning.BondThresholds = Bonds::DefaultBondThresholds();
        }

        // The sleep cycle (0.6.0): the recovery rate a resting mind
        // refills Fatigue at. The adapter's own default (0.2/s — a full
        // nap in ~5 s) applies when the config names none, so a world
        // without the key still sleeps.
        const auto restInjected =
            config.Get("sim.rest.recovery").empty();

        m_BondThresholds =
            Bonds::ParseBondThresholds(m_CoreTuning.BondThresholds);

        REX::INFO(
            "tuning: loaded {} — market {:02.0f}:00–{:02.0f}:00, drift {}{}, rest {:.3f}/s{}.",
            ini.string(),
            m_Settings.MarketOpenHour,
            m_Settings.MarketCloseHour,
            m_CoreTuning.DriftRate,
            driftInjected ? " (defaults)" : "",
            m_Settings.RestRecovery,
            restInjected ? " (defaults)" : "");
        REX::INFO(
            "bonds: friend {:+.2f}, sweetheart {:+.2f}, spouse {:+.2f}, "
            "rival {:+.2f}, enemy {:+.2f}{}.",
            m_BondThresholds.Friend,
            m_BondThresholds.Sweetheart,
            m_BondThresholds.Spouse,
            m_BondThresholds.Rival,
            m_BondThresholds.Enemy,
            bondDefaultsInjected ? " (defaults)" : "");
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

        // The modder's knob, loaded once per session — from here, after
        // the logger is attached, so the tuning confirmation lines
        // actually appear. Before the first world: StartWorld and
        // ApplyRestore below both consume m_Settings/m_CoreTuning.
        if (!m_TuningLoaded)
        {
            LoadConfiguration();
            m_TuningLoaded = true;
        }

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

    std::uint64_t Adapter::RngState() const noexcept
    {
        return m_Rng.State();
    }

    void Adapter::QueueRestore(
        LCE::Simulation::RegistrySnapshot a_snapshot,
        std::uint64_t a_rngState,
        std::vector<TLC::CoSave::StallKeeperPair> a_stallKeepers,
        std::vector<TLC::CoSave::BondPair> a_bonds)
    {
        m_PendingRestore = std::move(a_snapshot);
        m_PendingRngState = a_rngState;
        m_PendingStallKeepers = std::move(a_stallKeepers);
        m_PendingBonds = std::move(a_bonds);
    }

    std::vector<TLC::CoSave::StallKeeperPair> Adapter::StallKeepersForSave() const
    {
        // Entity ids are session-local; the durable form is form ids.
        // A keeper or market whose form the translator cannot resolve is
        // skipped — it was never a real entity this world.
        std::vector<TLC::CoSave::StallKeeperPair> result;
        result.reserve(m_StallKeepers.size());

        for (const auto& [market, keeper] : m_StallKeepers)
        {
            const auto marketFormId = m_Translator.FormFor(market);
            const auto keeperFormId = m_Translator.FormFor(keeper);

            if (marketFormId == 0 || keeperFormId == 0)
            {
                continue;
            }

            result.emplace_back(marketFormId, keeperFormId);
        }

        return result;
    }

    void Adapter::RestoreStallKeepers(
        const std::vector<TLC::CoSave::StallKeeperPair>& a_stallKeepers)
    {
        for (const auto& [marketFormId, keeperFormId] : a_stallKeepers)
        {
            const auto market = m_Translator.EntityFor(marketFormId);
            const auto keeper = m_Translator.EntityFor(keeperFormId);

            // Both ride the snapshot (the market owns a FormRef, the
            // keeper is a mind) — if either is missing, that market's
            // stall re-derives on the first arrival, like a fresh world.
            if (!market.IsValid() || !keeper.IsValid())
            {
                continue;
            }

            m_StallKeepers[market] = keeper;

            REX::INFO(
                "LCE: stall restored — market {:#x} reopens under keeper {:#x}.",
                marketFormId, keeperFormId);
        }
    }

    std::vector<TLC::CoSave::BondPair> Adapter::BondsForSave() const
    {
        // Entity ids are session-local; the durable form is form ids.
        // A pair whose form the translator cannot resolve is skipped — it
        // was never a real pair this world. A resting (None) row is never
        // written.
        std::vector<TLC::CoSave::BondPair> result;
        result.reserve(m_Bonds.size());

        for (const auto& [key, bond] : m_Bonds)
        {
            if (bond.Kind == Bonds::BondKind::None)
            {
                continue;
            }

            const auto formA = m_Translator.FormFor(
                LCE::Simulation::EntityId{ key.first });
            const auto formB = m_Translator.FormFor(
                LCE::Simulation::EntityId{ key.second });

            if (formA == 0 || formB == 0)
            {
                continue;
            }

            result.push_back(TLC::CoSave::BondPair{
                formA, formB,
                static_cast<std::uint32_t>(bond.Kind),
                bond.SinceDay });
        }

        return result;
    }

    void Adapter::RestoreBonds(
        const std::vector<TLC::CoSave::BondPair>& a_bonds)
    {
        m_Bonds.clear();

        for (const auto& bond : a_bonds)
        {
            const auto a = m_Translator.EntityFor(bond.FormA);
            const auto b = m_Translator.EntityFor(bond.FormB);

            // Both ride the snapshot (both are minds with FormRefs) — a
            // pair whose actor is missing is simply absent, and the 1s
            // reconcile pass re-derives what it can from the restored
            // relationships.
            if (!a.IsValid() || !b.IsValid())
            {
                continue;
            }

            m_Bonds[Bonds::PairKey(a, b)] =
                Bonds::PairBond{
                    static_cast<Bonds::BondKind>(bond.Kind),
                    bond.SinceDay };
        }

        if (!m_Bonds.empty())
        {
            REX::INFO(
                "bonds: {} bond{} restored from the co-save.",
                m_Bonds.size(), m_Bonds.size() == 1 ? "" : "s");
        }
    }

    void Adapter::ReconcileBonds()
    {
        // The 1-second pass (the dissolve net): re-derive every pair
        // from the live relationships. The event channel is instant; this
        // is complete — quiet drift, restores, and anything the bus
        // missed all surface here. OnBondChange fires only on change,
        // so a pair the event already settled is silent here.
        Bonds::Reconcile(
            m_Registry,
            m_BondThresholds,
            m_Bonds,
            CurrentDay(),
            [this](
                LCE::Simulation::EntityId a_entityA,
                LCE::Simulation::EntityId a_entityB,
                Bonds::BondKind a_old,
                Bonds::BondKind a_new,
                std::uint64_t a_sinceDay)
            {
                OnBondChange(a_entityA, a_entityB, a_old, a_new, a_sinceDay);
            });

        // The household invariant (0.6.0 Stone 3): one pouch per married
        // pair, one per unmarried human. The loud events fire in
        // OnBondChange; this is the silent repair — a restored marriage,
        // a defensive seed, anything the events missed.
        Households::Enforce(m_Registry, m_Bonds);
    }

    void Adapter::OnRelationshipChanged(
        const LCE::Simulation::RelationshipChangedEvent& a_event)
    {
        // The immediate channel (Request A — stone 08): the core crossed
        // a bond line mid-mutation. Re-derive that pair now, the same
        // rule the 1-second pass applies — the pass then finds the pair
        // resting and stays silent. The event's day is the crossing day,
        // the honest birthdate of a fresh bond.
        if (!a_event.Subject.IsValid() || !a_event.Other.IsValid())
        {
            return;
        }

        // Both must be minds — a workshop is a target, never a bond
        // partner (the same gate the reconcile pass applies).
        if (!m_Registry.GetComponent<SpeciesTag>(a_event.Subject)
            || !m_Registry.GetComponent<SpeciesTag>(a_event.Other))
        {
            return;
        }

        const auto dToOther = DispositionOf(a_event.Subject, a_event.Other);
        const auto dOtherToMe = DispositionOf(a_event.Other, a_event.Subject);

        Bonds::ApplyPair(
            m_Bonds,
            Bonds::PairKey(a_event.Subject, a_event.Other),
            dToOther, dOtherToMe,
            m_BondThresholds,
            a_event.Day,
            [this](
                LCE::Simulation::EntityId a_entityA,
                LCE::Simulation::EntityId a_entityB,
                Bonds::BondKind a_old,
                Bonds::BondKind a_new,
                std::uint64_t a_sinceDay)
            {
                OnBondChange(a_entityA, a_entityB, a_old, a_new, a_sinceDay);
            });
    }

    float Adapter::DispositionOf(
        LCE::Simulation::EntityId a_from,
        LCE::Simulation::EntityId a_to)
    {
        const auto relationships =
            m_Registry.GetComponent<LCE::Simulation::Relationships>(a_from);

        if (!relationships)
        {
            return 0.0f;
        }

        const auto iterator = relationships->ByEntity.find(a_to);

        return iterator != relationships->ByEntity.end()
            ? iterator->second.Disposition
            : 0.0f;
    }

    void Adapter::OnBondChange(
        LCE::Simulation::EntityId a_entityA,
        LCE::Simulation::EntityId a_entityB,
        Bonds::BondKind a_old,
        Bonds::BondKind a_new,
        std::uint64_t)
    {
        const auto formA = m_Translator.FormFor(a_entityA);
        const auto formB = m_Translator.FormFor(a_entityB);

        if (formA == 0 || formB == 0)
        {
            return;   // defensive — a translated pair names two minds
        }

        const auto labelA = SpeciesLabel(
            m_Registry.GetComponent<SpeciesTag>(a_entityA).get());
        const auto labelB = SpeciesLabel(
            m_Registry.GetComponent<SpeciesTag>(a_entityB).get());

        // The world's voice: formation, change, and dissolution each get
        // their line. The feud line is Life.md's own: "X is feuding
        // with Y." — only the crossing into Enemy says it; the rest use
        // the pair's plural.
        if (a_new == Bonds::BondKind::None)
        {
            if (Bonds::IsNegative(a_old))
            {
                REX::INFO(
                    "bonds: {} {:#x} and {} {:#x} made peace.",
                    labelA, formA, labelB, formB);
            }
            else
            {
                REX::INFO(
                    "bonds: {} {:#x} and {} {:#x} are no longer {}.",
                    labelA, formA, labelB, formB,
                    Bonds::BondPlural(a_old));
            }
        }
        else if (a_old == Bonds::BondKind::None
            && a_new == Bonds::BondKind::Enemy)
        {
            REX::INFO(
                "bonds: {} {:#x} is feuding with {} {:#x}.",
                labelA, formA, labelB, formB);
        }
        else if (a_old == Bonds::BondKind::None)
        {
            REX::INFO(
                "bonds: {} {:#x} and {} {:#x} became {}.",
                labelA, formA, labelB, formB,
                Bonds::BondPlural(a_new));
        }
        else
        {
            REX::INFO(
                "bonds: {} {:#x} and {} {:#x} are now {}.",
                labelA, formA, labelB, formB,
                Bonds::BondPlural(a_new));
        }

        // The household follows the deepest bond (0.6.0 Stone 3): the
        // moment a pair becomes spouses, their pouches become one shared
        // wallet; when the marriage dissolves, the wallet splits. Formed
        // exactly once — the pair rests at Spouse afterwards, so neither
        // the event channel nor the 1-second pass says it again.
        if (a_new == Bonds::BondKind::Spouse
            && a_old != Bonds::BondKind::Spouse)
        {
            if (Households::FormHousehold(m_Registry, a_entityA, a_entityB))
            {
                REX::INFO(
                    "households: {} {:#x} and {} {:#x} are now a household — one pouch, one bench.",
                    labelA, formA, labelB, formB);
            }
        }

        if (a_old == Bonds::BondKind::Spouse
            && a_new != Bonds::BondKind::Spouse)
        {
            std::uint32_t holderShare = 0;
            std::uint32_t otherShare = 0;

            if (Households::DissolveHousehold(
                    m_Registry, a_entityA, a_entityB,
                    holderShare, otherShare))
            {
                REX::INFO(
                    "households: {} {:#x} and {} {:#x} are no longer a household — the pouch splits ({} / {} caps).",
                    labelA, formA, labelB, formB,
                    holderShare, otherShare);
            }
        }
    }

    void Adapter::ApplyRestore(LCE::Simulation::RegistrySnapshot a_snapshot)
    {
        // End whatever was running (the pre-load already did, defensively)
        // — Clear keeps the serializers, registered once at init.
        EndWorld();

        m_Registry.Restore(a_snapshot);

        // The decay-jitter wiring (engine stone 07): resume the saved
        // world's randomness — its stream, exactly where it left off.
        // (For a v1 record the decode left the default seed untouched,
        // which is honest: that world never had a saved stream.)
        m_Rng.SetState(m_PendingRngState);

        // Rebuild the edge's memory: which form is which entity, from the
        // restored FormRef components. The translator is adapter state,
        // not core state — it never rides inside the snapshot.
        m_Registry.ForEachWithComponent<FormRef>(
            [this](LCE::Simulation::EntityId a_entity, FormRef& a_formRef)
            {
                m_Translator.Add(a_formRef.FormId, a_entity);
            });

        // The economy stone: a restored human without a pouch predates
        // the economy — the record had no caps to carry. Back-fill the
        // seed so a pre-economy save wakes into a living market instead of
        // a world where everyone is broke (a human mind always has a
        // pouch; children and animals never do).
        m_Registry.ForEachWithComponent<SpeciesTag>(
            [this](LCE::Simulation::EntityId a_entity, SpeciesTag& a_tag)
            {
                if (a_tag.Value == Species::Human
                    && !m_Registry.GetComponent<CapPouch>(a_entity))
                {
                    m_Registry.AddComponent<CapPouch>(
                        a_entity, CapPouch{ SeedPouch(a_entity) });
                }
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

        // The stall-keepers stone (v3): a restored market reopens under
        // its saved keeper — the same face behind the bench — instead of
        // whoever happens to arrive first. Runs after the translator
        // rebuild above, so both the market and the keeper resolve.
        RestoreStallKeepers(m_PendingStallKeepers);

        // The bonds stone (v5): a spouse is still a spouse after reload.
        // The book restores by form ids; the 1-second reconcile pass
        // then re-derives — a bond whose relationship drifted below its
        // dissolve line while the game was away dissolves, everything
        // else stands.
        RestoreBonds(m_PendingBonds);

        // The household stone (v3? no — derived, ADR-0013): a restored
        // marriage re-establishes its shared pouch silently. The loud
        // events fired in OnBondChange when the pair crossed the line in
        // life; here, the invariant is just repaired — one pouch per
        // married pair, one per unmarried human.
        Households::Enforce(m_Registry, m_Bonds);

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
        // A save FILE was deleted — not a world teardown. The sim must
        // keep running: in-game, an autosave rotation deleted a save
        // mid-world and EndWorld killed the sim with no pending load to
        // revive it (m_AwaitingLoad was false, so the 60s abort-revival
        // never fired) — every later save wrote 0 entities and the world
        // stayed dead until a full restart. A new game or a load goes
        // through PreLoadGame/GameLoaded, which own the world's life.
    }

    void Adapter::SeedMind(const RE::Actor* a_actor)
    {
        using namespace LCE::Simulation;

        if (a_actor == nullptr || !IsSimRelevant(a_actor))
        {
            return;
        }

        const auto formId = a_actor->GetFormID();

        // Already a mind — the wake seed and the per-second bookkeeping
        // share this path, and a form id never names both a mind and a
        // workshop (targets carry a FormRef but no SpeciesTag).
        if (m_Translator.EntityFor(formId).IsValid())
        {
            return;
        }

        const auto species = ClassifySpecies(a_actor->race);
        const auto id = m_Registry.CreateEntity();

        // The desync stone: every mind's needs are born slightly different
        // (VaryNeeds — deterministic per entity id), so hunger arrives at
        // different times and the settlement doesn't march to the market
        // in lockstep.
        auto needs = SeededNeeds(species, m_Settings.Rates);
        VaryNeeds(needs, id);

        m_Registry.AddComponent<FormRef>(id, FormRef{ formId });
        m_Registry.AddComponent<SpeciesTag>(id, SpeciesTag{ species });
        m_Registry.AddComponent<Needs>(id, std::move(needs));
        m_Registry.AddComponent<Goals>(id, SeededGoals(species));
        m_Registry.AddComponent<Memory>(id, Memory{});
        m_Registry.AddComponent<Relationships>(id, Relationships{});

        // The economy stone: a human is born with a small pouch
        // (deterministic per entity id — a saved purse restores exactly).
        // Children and animals never carry one: they never barter.
        if (species == Species::Human)
        {
            m_Registry.AddComponent<CapPouch>(
                id, CapPouch{ SeedPouch(id) });
        }

        m_Translator.Add(formId, id);
    }

    std::size_t Adapter::SeedLoadedActors()
    {
        std::size_t count = 0;

        ForEachLoadedActor(
            [this, &count](const RE::Actor* a_actor)
            {
                if (!IsSimRelevant(a_actor)
                    || m_Translator.EntityFor(a_actor->GetFormID()).IsValid())
                {
                    return;
                }

                SeedMind(a_actor);
                ++count;
            });

        return count;
    }

    void Adapter::KeepBooks()
    {
        using namespace LCE::Simulation;

        // The known minds: every live entity with a SpeciesTag. Workshops
        // carry a FormRef but no tag — they are targets, never minds.
        std::unordered_set<std::uint32_t> known;

        m_Registry.ForEachWithComponent<SpeciesTag>(
            [&](EntityId a_entity, const SpeciesTag&)
            {
                if (const auto formId = m_Translator.FormFor(a_entity); formId != 0)
                {
                    known.insert(formId);
                }
            });

        // The loaded-actor census: one read of the game's truth this pass.
        std::vector<Lifecycle::Scan> scans;

        ForEachLoadedActor(
            [&scans](RE::Actor* a_actor)
            {
                scans.push_back(Lifecycle::Scan{
                    a_actor->GetFormID(),
                    IsSimRelevant(a_actor),
                    IsActorDead(a_actor) });
            });

        // Two-pass death confirmation: park a form id read dead this
        // pass; book the death only when the next pass still reads it
        // dead. Stream-in artifacts read dead once — cleared the moment
        // the actor reads alive or leaves the lists. Real corpses stay
        // dead for minutes; two passes a second apart never miss one.
        // The seen-alive rule rides along: an actor that has never read
        // alive cannot die (a death is a transition) — the spawn burst
        // after a big load reads the same actors dead on first sight for
        // ~2s, and only an alive reading un-parks them.
        std::unordered_set<std::uint32_t> deadThisPass;

        for (const auto& scan : scans)
        {
            if (scan.Dead)
            {
                deadThisPass.insert(scan.FormId);
            }
            else
            {
                m_SeenAlive.insert(scan.FormId);
            }
        }

        for (auto it = m_PendingDeaths.begin(); it != m_PendingDeaths.end();)
        {
            if (!deadThisPass.contains(it->first))
            {
                it = m_PendingDeaths.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (const auto& event : Lifecycle::Diff(known, scans))
        {
            switch (event.Kind)
            {
            case Lifecycle::EventKind::Arrival:
            {
                // The scan's actor, re-resolved: the diff is pure, so the
                // actor pointer comes back through the form id. A loaded
                // arrival always resolves.
                const auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(event.FormId);

                if (actor == nullptr)
                {
                    break;
                }

                SeedMind(actor);

                if (m_Translator.EntityFor(event.FormId).IsValid())
                {
                    REX::INFO(
                        "lifecycle: settler {:#x} arrives — a new mind wakes.",
                        event.FormId);
                }

                break;
            }
            case Lifecycle::EventKind::Death:
                if (!m_SeenAlive.contains(event.FormId))
                {
                    // Never read alive — the spawn-burst artifact (or a
                    // corpse that was already gone before the world
                    // woke). A death is a transition; this is not one.
                    // Parked forever, cleared the moment the actor reads
                    // alive; never booked.
                    m_PendingDeaths[event.FormId] =
                        std::chrono::steady_clock::now();

                    REX::DEBUG(
                        "lifecycle: settler {:#x} reads dead before ever "
                        "being seen alive — parked, never booked.",
                        event.FormId);
                    break;
                }

                if (m_PendingDeaths.contains(event.FormId))
                {
                    // Second consecutive dead read — the death is real.
                    m_PendingDeaths.erase(event.FormId);
                    RemoveMind(event.FormId, true);
                }
                else
                {
                    // First dead read — an artifact passes next pass, a
                    // corpse confirms. Nothing booked yet; the debug line
                    // makes the confirmation visible in the log.
                    m_PendingDeaths[event.FormId] =
                        std::chrono::steady_clock::now();

                    REX::DEBUG(
                        "lifecycle: settler {:#x} reads dead — first pass, "
                        "not booked (confirming).",
                        event.FormId);
                }
                break;
            case Lifecycle::EventKind::Departure:
                RemoveMind(event.FormId, false);
                break;
            }
        }
    }

    void Adapter::RemoveMind(std::uint32_t a_formId, bool a_isDeath)
    {
        using namespace LCE::Simulation;

        const auto entity = m_Translator.EntityFor(a_formId);

        if (!entity.IsValid())
        {
            return;
        }

        // The book's other pages: no walk, no last-log, no feeder line,
        // no stall — a keeper's market re-derives its keeper on the next
        // arrival.
        m_Walks.erase(entity);
        m_LastLogged.erase(entity);
        m_FeederLogged.erase(entity);

        for (auto it = m_StallKeepers.begin(); it != m_StallKeepers.end();)
        {
            if (it->second == entity)
            {
                it = m_StallKeepers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // The household (0.6.0 Stone 3): if the dead held the family
        // pouch, it passes to the widow(er) — the wallet is the
        // household's, not the holder's. Runs before the bond erase
        // below, so the spouse still resolves.
        const auto spouse = Households::SpouseOf(m_Bonds, entity);

        if (spouse.IsValid()
            && m_Registry.GetComponent<CapPouch>(entity)
            && !m_Registry.GetComponent<CapPouch>(spouse))
        {
            const auto pouch = m_Registry.GetComponent<CapPouch>(entity);

            m_Registry.AddComponent<CapPouch>(
                spouse, CapPouch{ pouch->Caps });
        }

        // The bond book closes with the mind: every pair it belonged to
        // dissolves (the survivors' relationship rows still hold the
        // stale id, but the id is dead — the reconcile pass skips pairs
        // whose members are not minds, so no ghost bond lingers).
        for (auto it = m_Bonds.begin(); it != m_Bonds.end();)
        {
            if (it->first.first == entity.Value()
                || it->first.second == entity.Value())
            {
                it = m_Bonds.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (a_isDeath)
        {
            // The death fact: every surviving mind remembers who is gone —
            // { the dead, Death, weight, day }. A fact, never a door:
            // Decide gates only Trade and Social, so a death never blocks
            // a walk or a trade. Survivors carry it across the co-save;
            // the dead themselves are simply absent (they do not restore).
            // Grief reads it in Stone 2.
            const auto day = CurrentDay();

            m_Registry.ForEachWithComponent<Memory>(
                [&](EntityId a_survivor, Memory& a_memory)
                {
                    if (a_survivor == entity)
                    {
                        return;
                    }

                    a_memory.Events.push_back(MemoryEvent{
                        entity, InteractionKind::Death,
                        WorldFacts::kFactWeight, day });
                });
        }

        m_Registry.DestroyEntity(entity);
        m_Translator.Remove(a_formId);

        REX::INFO(
            "lifecycle: settler {:#x} {} — the world keeps its books.",
            a_formId, a_isDeath ? "died" : "left the settlement");
    }

    std::uint64_t Adapter::CurrentDay() const
    {
        const auto* calendar = RE::Calendar::GetSingleton();

        return calendar != nullptr && calendar->gameDaysPassed != nullptr
            ? std::uint64_t(calendar->gameDaysPassed->value)
            : 0;
    }

    void Adapter::StartWorld()
    {
        if (m_Started)
        {
            return;
        }

        const auto count = SeedLoadedActors();

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
        m_Bonds.clear();
        m_Walks.clear();
        m_PendingDeaths.clear();
        m_SeenAlive.clear();
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
        // Throttled retry: the census can run before the game's REFR
        // data is fully available, so an empty result must never be
        // pinned — a false 0 would lock the whole session into the
        // single-bench fallback. Once a non-empty list is found it is
        // final (static per load order); the seed cycle re-scans an
        // empty one at most every few seconds.
        const auto now = std::chrono::steady_clock::now();

        if (m_LastCensus.time_since_epoch().count() != 0
            && now - m_LastCensus < std::chrono::seconds(5))
        {
            return;
        }

        m_LastCensus = now;
        m_Workshops.clear();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();

        if (dataHandler == nullptr)
        {
            return;
        }

        // FO4 does not store REFRs in the data handler's flat form
        // array — GetFormArray<TESObjectREFR> is a Skyrim-ism that is
        // always empty here (verified in-game: 0 REFRs across retries).
        // Settlement workbenches are all *persistent* refs, and every
        // persistent ref lives in its worldspace's persistent cell,
        // which is loaded at world start — so one pass over the
        // worldspaces' persistent cells finds every settlement market
        // with valid positions, without loading a single cell.
        const auto& worldspaces =
            dataHandler->GetFormArray<RE::TESWorldSpace>();

        std::size_t probed = 0;  // diagnostic refs logged (once)

        for (const auto* world : worldspaces)
        {
            if (world == nullptr)
            {
                continue;
            }

            auto* cell = world->persistentCell;

            if (cell == nullptr)
            {
                continue;
            }

            cell->ForEachReference(
                [&](RE::TESObjectREFR* a_ref)
                {
                    const auto* base = a_ref->GetObjectReference();

                    if (base == nullptr)
                    {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    // Diagnostic (once per session): the first bases in
                    // the persistent cells, so a 0-result census says
                    // what the filter saw, not just that it saw nothing.
                    if (!m_CensusDiagnosed && probed < 3)
                    {
                        REX::INFO(
                            "census probe: world {:#x} persistent REFR {:#x} base {:#x}.",
                            world->GetFormID(), a_ref->GetFormID(),
                            base->GetFormID());
                        ++probed;
                    }

                    if (base->GetFormID() != kWorkshopBaseFormId)
                    {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    const auto pos = a_ref->GetPosition();

                    m_Workshops.push_back(WorkshopPosition{
                        a_ref->GetFormID(), pos.x, pos.y });

                    return RE::BSContainer::ForEachResult::kContinue;
                });
        }

        if (probed > 0)
        {
            m_CensusDiagnosed = true;
        }

        // A found list is final; an empty one is not (see the throttle
        // above) — the legacy single-bench fallback covers the world
        // while the census keeps looking.
        if (!m_Workshops.empty())
        {
            m_WorkshopsReady = true;
        }

        REX::INFO(
            "settlement census: {} worldspaces, {} workshops known (base {:#x}){}.",
            worldspaces.size(), m_Workshops.size(), kWorkshopBaseFormId,
            m_Workshops.empty() ? " — will retry" : " — markets are per settlement");
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
        bool keeperHome = false;   // the stall-keeper at their own bench
        bool familyHome = false;   // the spouse of the keeper — the family bench
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

                const auto spouse = Households::SpouseOf(m_Bonds, a_entity);

                if (stall.IsValid() && spouse.IsValid() && stall == spouse)
                {
                    // The family bench (0.6.0 Stone 3): my spouse keeps
                    // this stall — a meal at home. No exchange: the
                    // household pouch does not pay itself.
                    counterparty = target;
                    traded = false;
                    familyHome = true;
                }
                else if (stall.IsValid() && stall != a_entity)
                {
                    counterparty = stall;
                    traded = true;
                }
                else if (stall.IsValid())
                {
                    // The keeper themselves at their own bench (this world
                    // or restored from the co-save) — no customers yet.
                    // Not a re-claim: the stall stands, nothing changes
                    // hands, and the map keeps the same keeper.
                    counterparty = target;
                    traded = false;
                    keeperHome = true;
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

        // The outcome lands on the bus (0.6.0 stone 08): a sale that
        // warms the buyer past the friend line publishes
        // RelationshipChangedEvent — the instant bond channel. The
        // world day rides along so the event (and the memory it stamps)
        // is anchored to the calendar.
        ReportOutcome(
            m_Registry, a_entity, outcome, m_CoreTuning,
            &m_Bus, LCE::Simulation::WorldTime{ CurrentDay() });

        if (species == Species::Human)
        {
            if (traded)
            {
                // The buyer's half of the exchange: a meal at the bench
                // is company. ReportOutcome above built trust (Trade is
                // the core's reliability channel); Remember(Social)
                // warms the disposition — the same +0.1 the keeper's
                // RecordSale warms them — and publishes the crossing on
                // the bus, so a warm enough buyer crosses the friend
                // line the instant the meal lands. This is the
                // courtship's raw material (Life.md): repeated trading
                // at the same bench is how two settlers become friends.
                Remember(
                    m_Registry, a_entity,
                    MemoryEvent{
                        counterparty, InteractionKind::Social,
                        WorldFacts::kFactWeight },
                    m_CoreTuning,
                    WorldTime{ CurrentDay() },
                    &m_Bus);

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

                // The physical exchange (the economy stone): the buyer
                // pays what they can afford up to the meal's price and
                // the seller's pouch grows. A broke buyer pays nothing
                // and is still fed — the settlement covers the meal.
                // Price is whole caps, minimum 1 (a free meal is not a
                // market).
                std::uint32_t paid = 0;
                std::uint32_t buyerCaps = 0;
                std::uint32_t sellerCaps = 0;

                // The shared wallet (0.6.0 Stone 3): a married member
                // trades with the household's pouch — their own, or the
                // spouse's — on both sides of the bench. PouchOf resolves
                // it either way, so one wallet round-trips.
                auto buyerPouch =
                    Households::PouchOf(m_Registry, m_Bonds, a_entity);
                auto sellerPouch =
                    Households::PouchOf(m_Registry, m_Bonds, counterparty);
                const auto paidFromHousehold =
                    buyerPouch != nullptr
                    && m_Registry.GetComponent<CapPouch>(a_entity) == nullptr;

                if (buyerPouch && sellerPouch)
                {
                    const auto price = static_cast<std::uint32_t>(
                        m_Settings.MealPrice > 1.0f
                            ? m_Settings.MealPrice
                            : 1.0f);

                    paid = PayForMeal(*buyerPouch, *sellerPouch, price);
                    buyerCaps = buyerPouch->Caps;
                    sellerCaps = sellerPouch->Caps;
                }

                const auto traderFormId = m_Translator.FormFor(counterparty);

                if (paid > 0)
                {
                    REX::INFO(
                        "LCE: settler {:#x} trades with settler {:#x} at market {:#x} — fed, {} caps change hands ({}{} left, {} now).",
                        formId, traderFormId, marketFormId,
                        paid,
                        paidFromHousehold ? "household; " : "",
                        buyerCaps, sellerCaps);
                }
                else
                {
                    REX::INFO(
                        "LCE: settler {:#x} trades with settler {:#x} at market {:#x} — fed on the settlement's credit (no caps).",
                        formId, traderFormId, marketFormId);
                }
            }
            else if (familyHome)
            {
                REX::INFO(
                    "LCE: settler {:#x} is at the family stall at market {:#x} — fed from the household's meal.",
                    formId, marketFormId);
            }
            else if (keeperHome)
            {
                REX::INFO(
                    "LCE: settler {:#x} is at their own stall at market {:#x} — no customers yet.",
                    formId, marketFormId);
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
        // The census: one scan over the REFR form array. Static per load
        // order once it finds workshops; an empty result is retried (the
        // array may not be populated yet) while the fallback covers the
        // world — see RefreshWorkshops.
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

            // 0.6.0 Stone 1 — the world keeps its books: new settlers
            // become minds, deaths and departures leave the book. Runs
            // before the seed so this tick's Update sees consistent state.
            KeepBooks();

            // 0.6.0 Stone 2 — bonds: the 1-second dissolve net. The
            // event channel is instant; this pass is complete — quiet
            // drift (the core never publishes a dissolve), restores,
            // and anything the bus missed all surface here.
            ReconcileBonds();

            SeedMarket(false);

            // The world's doors: the market's trading hours and the
            // weather. Pushed on the same cadence as the seed — silent
            // unless a door changes.
            PushWorldFacts();
        }

        // The sleep cycle (0.6.0): a mind whose last intent was Rest is
        // resting — its Fatigue recovers at sim.rest.recovery per second
        // (default 0.2/s, a full nap in ~5 s). The engine's need loop
        // only decays; without this, a fed mind with drained Fatigue
        // parks in Rest forever (the sleep-cycle discovery: only Hunger
        // is ever restored, and only on the meal). Restored before
        // Update so this tick's decisions see the rested mind.
        m_Registry.ForEachWithComponent<LCE::Simulation::Intent>(
            [&](EntityId a_entity, const LCE::Simulation::Intent& a_intent)
            {
                if (a_intent.Action == LCE::Simulation::ActionType::Rest)
                {
                    auto needs =
                        m_Registry.GetComponent<Needs>(a_entity);

                    if (needs)
                    {
                        // The recovery value is a defensive marker (-1
                        // when a mind somehow lacks Fatigue); a resting
                        // mind with one is recovered, nothing to branch.
                        (void)RestRecovery(
                            *needs,
                            m_Settings.RestRecovery,
                            static_cast<float>(a_deltaSeconds));
                    }
                }
            });

        // The core's stateless tick: needs decay, memory fade, goal
        // urgency, then one Intent per mind. All of it on the game thread,
        // with the modder's tuning (the config file) when present. The
        // observation bus rides along so the sim's changes flow out —
        // bond crossings surface as RelationshipChangedEvent (0.6.0
        // stone 08).
        Update(m_Registry, a_deltaSeconds, m_CoreTuning, &m_Bus, &m_Rng);

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
                    && now - session.Issued < std::chrono::seconds(120))
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

            // A session ends 120s after issue — the walk is issued once
            // and the game's planner carries it from there. 120s covers
            // the market radius (≈10,000 units ≈ 140 m) even at a slow
            // walk; the old 60s killed slow walkers mid-path (the radius
            // needs 60-100s at walk speed, and pathing detours stretch
            // it further). The sim re-decides on its own, so a live
            // session outliving its intent is harmless.
            if (now - session.Issued >= std::chrono::seconds(120))
            {
                if (!session.Reached)
                {
                    REX::DEBUG(
                        "LCE: walk session settler {} ended (closest approach {:.1f} u).",
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

            // Distance (units) from the actor's DATA position to the
            // destination. The 3D node's world transform was the old
            // source, and it lied: after a fast travel, streaming actors
            // reported positions 120,000+ units from where they stood
            // (a walker ~700 units from the bench "moved" 1.7 km in a
            // frame), so arrival never registered. The data position
            // tracks the actor while loaded and stays sane (last known)
            // when the cell unloads.
            const auto from = actor->GetPosition();
            const auto to = target->GetPosition();
            const auto dx = from.x - to.x;
            const auto dy = from.y - to.y;
            const auto d = std::sqrt(dx * dx + dy * dy);

            // The first reading is the baseline. A walker cannot plausibly
            // stray beyond ~8× its starting distance plus a margin — a
            // reading that absurd is a stream artifact (cell teardown),
            // not progress: skip it without touching the minimum or the
            // arrival check, so a blip cannot corrupt a healthy walk.
            if (session.StartDistance <= 0.0f)
            {
                session.StartDistance = d;
            }
            else if (d > session.StartDistance * 8.0f + 5000.0f)
            {
                ++it;
                continue;
            }

            if (d < session.MinDistance)
            {
                session.MinDistance = d;
            }

            if (d < kArrivalRadius)
            {
                session.Reached = true;
                ReportArrival(it->first, session.Target);
                REX::INFO(
                    "LCE: settler {} arrived (d = {:.1f} u).",
                    FormatHex8(actorFormId), d);
            }
            else if (now - session.LastProbe >= std::chrono::seconds(2)
                && (session.LastDistance < 0.0f
                    || std::fabs(d - session.LastDistance) >= 1.0f))
            {
                session.LastProbe = now;
                session.LastDistance = d;
                REX::DEBUG(
                    "LCE: walk probe settler {} -> {} d = {:.1f} u (min {:.1f} u).",
                    FormatHex8(actorFormId), FormatHex8(session.Target), d,
                    session.MinDistance);
            }

            ++it;
        }
    }
}
