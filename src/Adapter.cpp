//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   The game acts, the sim thinks, the adapter translates.                                      //
//                                                                             //
//=============================================================================//

#include "Adapter.h"

#include "Arcs.h"
#include "Behaviour.h"
#include "Birth.h"
#include "Components.h"
#include "Gossip.h"
#include "Fights.h"
#include "Rows.h"
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
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/S/Sky.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWorldSpace.h>
#include <RE/T/TESFormUtil.h>   // the header-only definition of TESForm::As<T>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESWeather.h>

#include <LCE/Logging/Logger.h>

#include "LCE/Simulation/Decision/Behaviour.h"
#include "LCE/Simulation/Society/Groups.h"
#include "LCE/Simulation/Decision/Legacy.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Relationships.h"
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

        // The species table's complement: every race a person can be.
        // ClassifySpecies defaults anything unknown to Human (a workshop
        // population is usually people — ADR-0024), and the device table
        // in SimRelevant.cpp catches what it knows. A Human-classified
        // mind whose race is neither here nor a known device is either a
        // modded race (fine — default Human is the point) or a prop that
        // slipped the table; the prune announces it once so the table can
        // grow. Races verified in xEdit 2026-08-10 from Fallout4.esm.
        bool IsKnownOrganicRace(const RE::TESRace* a_race)
        {
            if (a_race == nullptr)
            {
                return false;
            }

            switch (a_race->GetFormID())
            {
            // People, in every form the sim seeds as Human.
            case 0x00013746:   // HumanRace
            case 0x000EAFB6:   // GhoulRace
            case 0x0001A009:   // SuperMutantRace
            case 0x0001D31E:   // PowerArmorRace (an armored settler)
            case 0x000E8D09:   // SynthGen1Race
            case 0x0010BD65:   // SynthGen2Race
            case 0x002261A4:   // SynthGen2RaceValentine
            // Children and the animal table (species-tagged, never
            // classified Human — listed so the complement is total).
            case 0x0011D83F:   // HumanChildRace
            case 0x0011EB96:   // GhoulChildRace
            case 0x0001D698:   // DogmeatRace
            case 0x0001D810:   // MoleratRace
            case 0x0001DB4A:   // DeathclawRace
            case 0x0002047E:   // BrahminRace
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
            case 0x000D9804:   // GorillaRace
            case 0x000E12A6:   // MirelurkQueenRace
            case 0x00187AF9:   // RaiderDogRace
                return true;

            default:
                return false;
            }
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

        // (The walk cap itself is tuning, not a constant: sim.walk.cap
        // in the INI — see AdapterSettings::WalkCap. Arrival ends a
        // session and frees its slot immediately, so the default 16 is
        // generous; big saves can raise it without a rebuild.)

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

        // The identity stone's pool (0.7.0 Stone 1): the author's own
        // name lists — names.first.male / .female / .animal, names.last
        // — comma-separated, each overriding its default list. The
        // defaults are the world's fallback, never a broken line.
        m_Names = TLC::Names::PoolFrom(config);

        // The dialogue pools (0.7.1 Talk): the author's one-liners,
        // overridable per list in the INI (dialogue.* keys), defaults
        // otherwise — a missing or broken line never breaks the world.
        m_Dialogue = TLC::Dialogue::PoolFrom(config);

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

        // The seeded need rhythm — the five sim.*.decay rates. Printed
        // so the log always says which rhythm actually runs (the
        // 2026-08-11 hunt: a 0.1/s hunger INI that read as if it were
        // 0.002/s — the banner only showed market hours, drift, and rest).
        const auto needsDefaultsInjected =
            config.Get("sim.hunger.decay").empty()
            && config.Get("sim.fatigue.decay").empty()
            && config.Get("sim.safety.decay").empty()
            && config.Get("sim.social.decay").empty()
            && config.Get("sim.comfort.decay").empty();

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
        REX::INFO(
            "tuning: needs — hunger {:.3f}/s, fatigue {:.3f}/s, "
            "safety {:.3f}/s, social {:.3f}/s, comfort {:.3f}/s, "
            "walk cap {}{}.",
            m_Settings.Rates.Hunger,
            m_Settings.Rates.Fatigue,
            m_Settings.Rates.Safety,
            m_Settings.Rates.Social,
            m_Settings.Rates.Comfort,
            m_Settings.WalkCap,
            needsDefaultsInjected ? " (defaults)" : "");
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

    void Adapter::RunMediation()
    {
        AttemptMediation();
    }

    void Adapter::AttemptMediation()
    {
        // The feud pairs the settlement knows: every pair in the bond
        // book at the enemy line. A feud is a story the world heard —
        // strangers do not step in, but the settlement can try.
        std::vector<std::pair<LCE::Simulation::EntityId,
            LCE::Simulation::EntityId>> feuds;

        for (const auto& [key, bond] : m_Bonds)
        {
            if (bond.Kind != Bonds::BondKind::Enemy)
            {
                continue;
            }

            feuds.emplace_back(
                LCE::Simulation::EntityId{ key.first },
                LCE::Simulation::EntityId{ key.second });
        }

        if (feuds.empty())
        {
            return;
        }

        const auto attempts = Arcs::Mediate(m_Registry, feuds, &m_Rng);

        for (const auto& attempt : attempts)
        {
            const auto mediator = m_Translator.FormFor(attempt.Mediator);
            const auto enemyA = m_Translator.FormFor(attempt.EnemyA);
            const auto enemyB = m_Translator.FormFor(attempt.EnemyB);

            if (mediator == 0 || enemyA == 0 || enemyB == 0)
            {
                continue;
            }

            if (attempt.Cooled)
            {
                REX::INFO(
                    "arcs: {} cooled the feud between {} and {} "
                    "— the settlement pulls its own apart.",
                    MindLabelForm(mediator),
                    MindLabelForm(enemyA), MindLabelForm(enemyB));

                PushNews(
                    MindLabelForm(mediator) + " cooled the feud between "
                    + MindLabelForm(enemyA) + " and "
                    + MindLabelForm(enemyB) + ".");
            }
            else
            {
                REX::INFO(
                    "arcs: {} tried to cool the feud between {} "
                    "and {} — nobody listened.",
                    MindLabelForm(mediator),
                    MindLabelForm(enemyA), MindLabelForm(enemyB));
            }
        }
    }

    void Adapter::RunBirth()
    {
        // The eligible households: every spouse pair with two real
        // actors — a child is born to a couple that exists in the game.
        // The child itself is sim-only: no form, no game actor, just a
        // mind the household feeds.
        std::vector<std::pair<LCE::Simulation::EntityId,
            LCE::Simulation::EntityId>> couples;

        for (const auto& [key, bond] : m_Bonds)
        {
            if (bond.Kind != Bonds::BondKind::Spouse)
            {
                continue;
            }

            const auto a = LCE::Simulation::EntityId{ key.first };
            const auto b = LCE::Simulation::EntityId{ key.second };

            if (m_Translator.FormFor(a) != 0
                && m_Translator.FormFor(b) != 0)
            {
                couples.emplace_back(a, b);
            }
        }

        if (couples.empty())
        {
            return;
        }

        // One birth per day: the household the Rng draws. The household
        // already lives in the sim — the bond book and the shared pouch
        // know where its home settlement is, and the child eats there
        // for free.
        const auto index = m_Rng.Next() % couples.size();
        const auto& [parentA, parentB] = couples[index];

        const auto child = Birth::Create(
            m_Registry, parentA, parentB, m_Settings.Rates);

        if (!child.IsValid())
        {
            return;
        }

        // The identity stone (0.7.0 Stone 1): the child carries the
        // household's family name — a first name drawn deterministically
        // from the child's id, the family name from the parents' ("the
        // Lees" stay the Lees). A procedural parent falls back to a
        // plain procedural name.
        std::string family;

        if (const auto parentName =
                m_Registry.GetComponent<Name>(parentA))
        {
            family = std::string(TLC::Names::FamilyOf(parentName->Full));
        }

        const auto childGender = TLC::Names::GenderOf(child);

        const auto childName = family.empty()
            ? TLC::Names::GenerateUnique(
                m_UsedNames, child, m_Names, childGender)
            : TLC::Names::ChildName(
                family, child, m_Names, childGender);

        m_Registry.AddComponent<Name>(child, Name{ childName });
        m_UsedNames.insert(childName);

        // 0.7.0 Legacy (engine stone 11): the child inherits the
        // parents' memories of the people — the family's knowledge and
        // grudges, scaled and aged by the core. The world's predicate:
        // person-facts only (a valid Other — weather facts name no one
        // and stay behind). The feud travels on the memory channel, the
        // inherited cold shoulder on the group echo.
        const auto accept =
            [](const LCE::Simulation::MemoryEvent& a_event)
            {
                return a_event.Other.IsValid();
            };

        LCE::Simulation::InheritMemory(
            m_Registry, child, parentA, m_CoreTuning,
            LCE::Simulation::WorldTime{ CurrentDay() }, accept);
        LCE::Simulation::InheritMemory(
            m_Registry, child, parentB, m_CoreTuning,
            LCE::Simulation::WorldTime{ CurrentDay() }, accept);

        REX::INFO(
            "birth: a child is born to {} and {} — {}, a new mind, "
            "fed by the household.",
            MindLabel(parentA), MindLabel(parentB),
            MindLabel(child));

        PushNews(
            "a child was born to " + MindLabel(parentA) + " and "
            + MindLabel(parentB) + " — " + childName + ".");
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

        // The identity stone's voice (0.7.0 Stone 1): the world speaks
        // in people now — the name with its console hex beside it. The
        // species label stays for the household section below.
        const auto labelA = SpeciesLabel(
            m_Registry.GetComponent<SpeciesTag>(a_entityA).get());
        const auto labelB = SpeciesLabel(
            m_Registry.GetComponent<SpeciesTag>(a_entityB).get());
        const auto nameA = MindLabel(a_entityA);
        const auto nameB = MindLabel(a_entityB);

        // The world's voice: formation, change, and dissolution each get
        // their line. The feud line is Life.md's own: "X is feuding
        // with Y." — only the crossing into Enemy says it; the rest use
        // the pair's plural.
        if (a_new == Bonds::BondKind::None)
        {
            if (Bonds::IsNegative(a_old))
            {
                REX::INFO(
                    "bonds: {} and {} made peace.",
                    nameA, nameB);

                PushNews(nameA + " and " + nameB + " made peace.");
            }
            else
            {
                REX::INFO(
                    "bonds: {} and {} are no longer {}.",
                    nameA, nameB, Bonds::BondPlural(a_old));
            }
        }
        else if (a_new == Bonds::BondKind::Enemy)
        {
            // Any crossing into Enemy is a feud — whether it jumped the
            // whole book in one blow (None -> Enemy) or cooled down the
            // rivalry (Rival -> Enemy, the normal shut-stall path).
            REX::INFO(
                "bonds: {} is feuding with {}.",
                nameA, nameB);
        }
        else if (a_old == Bonds::BondKind::None)
        {
            REX::INFO(
                "bonds: {} and {} became {}.",
                nameA, nameB, Bonds::BondPlural(a_new));
        }
        else
        {
            REX::INFO(
                "bonds: {} and {} are now {}.",
                nameA, nameB, Bonds::BondPlural(a_new));
        }

        // The household follows the deepest bond (0.6.0 Stone 3): the
        // moment a pair becomes spouses, their pouches become one shared
        // wallet; when the marriage dissolves, the wallet splits. Formed
        // exactly once — the pair rests at Spouse afterwards, so neither
        // the event channel nor the 1-second pass says it again.
        //
        // The one-wallet-per-mind guard (the polygamy edge, 2026-08-11):
        // the bond layer may honestly read Spouse to two minds at once
        // (pure derived disposition — a beloved settler can cross +0.8
        // with two people), but a second merge would fold a third pouch
        // into the shared wallet and a later 2-way split would vanish
        // the third member's caps. The second marriage stands as a bond
        // (the family bench still feeds both spouses); the wallets stay
        // personal.
        if (a_new == Bonds::BondKind::Spouse
            && a_old != Bonds::BondKind::Spouse)
        {
            if (Households::InHousehold(
                    m_Registry, m_Bonds, a_entityA)
                || Households::InHousehold(
                    m_Registry, m_Bonds, a_entityB))
            {
                REX::INFO(
                    "households: {} and {} are spouses, but one "
                    "already shares a household — the first pouch stands; "
                    "their wallets stay personal.",
                    nameA, nameB);
            }
            else if (Households::FormHousehold(
                    m_Registry, a_entityA, a_entityB))
            {
                REX::INFO(
                    "households: {} and {} are now a household — one pouch, one bench.",
                    nameA, nameB);
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
                    "households: {} and {} are no longer a household — the pouch splits ({} / {} caps).",
                    nameA, nameB, holderShare, otherShare);
            }
        }

        // The settlement hears its own news (0.6.0 Stone 4 — gossip):
        // a bond crossing — friend, sweetheart, spouse, rival, enemy —
        // names both participants to every mind. Strangers and fresh
        // arrivals never hear it (gossip is written once, not replayed).
        // Deaths spread through the same channel in RemoveMind. The
        // player window (0.7.0 Stone 3) reads the same crossings as
        // news: formations are the world's headlines.
        if (a_new != Bonds::BondKind::None
            && a_old == Bonds::BondKind::None)
        {
            Gossip::SpreadBond(
                m_Registry, a_entityA, a_entityB,
                LCE::Simulation::InteractionKind::Social,
                CurrentDay());

            if (a_new == Bonds::BondKind::Rival)
            {
                PushNews(nameA + " and " + nameB + " became rivals.");
            }
            else if (a_new == Bonds::BondKind::Friend)
            {
                PushNews(nameA + " and " + nameB + " became friends.");
            }
            else if (a_new == Bonds::BondKind::Sweetheart)
            {
                PushNews(nameA + " and " + nameB + " are sweethearts.");
            }
            else if (a_new == Bonds::BondKind::Spouse)
            {
                PushNews(nameA + " and " + nameB + " are married.");
            }
        }

        // The feud headline (0.7.0 Stone 2): any crossing into Enemy
        // makes the papers, not just a direct jump from nothing. The
        // slow rival -> enemy path is how shut-stall feuds actually
        // arrive, and the world should hear them too.
        if (a_new == Bonds::BondKind::Enemy
            && a_old != Bonds::BondKind::Enemy)
        {
            PushNews(nameA + " is feuding with " + nameB + ".");

            // The settlement hears the feud (Gossip.h's intent: gossip
            // covers "a feud starts") — the formation gossip from the
            // rival stage has long faded by now (memory fade 0.2/s),
            // and without a fresh spread no third mind could ever know
            // the pair well enough to step in.
            Gossip::SpreadBond(
                m_Registry, a_entityA, a_entityB,
                LCE::Simulation::InteractionKind::Social,
                CurrentDay());

            // And the settlement tries, now — while the news is still
            // alive. The once-per-day pass alone can never reach a feud:
            // by the next day turn the gossip has faded to nothing and
            // no mediator can be found. A feud is mediated the moment it
            // breaks, while everyone still knows.
            AttemptMediation();
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

        // The arcs' day gates are session state — a restore must not
        // re-fire today's birth or mediation. The max sentinel says
        // "never ran", which is exactly wrong after a reload: the first
        // tick would birth a child and re-mediate every feud even
        // though the day hasn't turned (the 2026-08-11 restore-birth:
        // a child 1 s after every load). Seeded to today — the world
        // has already had its day's chances; tomorrow turns the gates
        // again.
        m_LastMediationDay = CurrentDay();
        m_LastBirthDay = CurrentDay();

        // Rebuild the edge's memory: which form is which entity, from the
        // restored FormRef components. The translator is adapter state,
        // not core state — it never rides inside the snapshot.
        m_Registry.ForEachWithComponent<FormRef>(
            [this](LCE::Simulation::EntityId a_entity, FormRef& a_formRef)
            {
                m_Translator.Add(a_formRef.FormId, a_entity);
            });

        // The device prune (0.7.2 fix): a polluted co-save holds the
        // workshop's props as minds. Runs now — the translator is up, but
        // pouches, names, and bonds are not rebuilt yet, so a pruned
        // prop never carries a wallet, a name, or a feud.
        PruneDeviceMinds();

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

        // The identity stone (0.7.0 Stone 1): a restored mind without a
        // name predates the stone — the record had no name to carry.
        // Back-fill a procedural name (the same deterministic draw the
        // seed uses, deduped against the world's live names) so a
        // pre-0.7 save wakes into a named world instead of a log of hex.
        // Children restored without a name get a plain procedural name;
        // children born after the stone carry their household's family
        // name (the RunBirth path).
        m_UsedNames.clear();

        m_Registry.ForEachWithComponent<Name>(
            [this](LCE::Simulation::EntityId, const Name& a_name)
            {
                m_UsedNames.insert(a_name.Full);
            });

        m_Registry.ForEachWithComponent<SpeciesTag>(
            [this](LCE::Simulation::EntityId a_entity, SpeciesTag& a_tag)
            {
                if (m_Registry.GetComponent<Name>(a_entity) != nullptr)
                {
                    return;
                }

                std::string backfilled;

                if (a_tag.Value == Species::Animal)
                {
                    // The naming rule (the owner stone): only an owned
                    // animal gets a name — a restored stray stays
                    // nameless, its label the species and hex. An owner
                    // is whoever the game assigns (the player, or a
                    // settler); a nameless restored pet is claimed here.
                    const auto formRef =
                        m_Registry.GetComponent<FormRef>(a_entity);
                    auto* actor = formRef
                        ? RE::TESForm::GetFormByID<RE::Actor>(formRef->FormId)
                        : nullptr;

                    if (actor == nullptr || actor->GetOwner() == nullptr)
                    {
                        return;
                    }

                    backfilled = TLC::Names::GenerateUniqueAnimal(
                        m_UsedNames, a_entity, m_Names);
                }
                else
                {
                    // A person's gender: the actor's sex when the form
                    // resolves (a restored world's actors load
                    // gradually), else the id's own draw.
                    auto gender = TLC::Names::GenderOf(a_entity);

                    if (const auto formRef =
                            m_Registry.GetComponent<FormRef>(a_entity))
                    {
                        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(
                            formRef->FormId);

                        if (actor != nullptr)
                        {
                            const auto sex = actor->GetSex();

                            if (sex == RE::SEX::kMale)
                            {
                                gender = TLC::Names::Gender::Male;
                            }
                            else if (sex == RE::SEX::kFemale)
                            {
                                gender = TLC::Names::Gender::Female;
                            }
                        }
                    }

                    backfilled = TLC::Names::GenerateUnique(
                        m_UsedNames, a_entity, m_Names, gender);
                }

                m_Registry.AddComponent<Name>(
                    a_entity, Name{ backfilled });
                m_UsedNames.insert(backfilled);
            });

        // Pet names are unique per world: an old co-save can hold
        // duplicates (the five "Bandit" dogs from earlier builds). The
        // first keeps its name; each later one is re-drawn
        // deterministically against the world's used set — self-heals
        // once, and every future session dedups as it names. The
        // corrected names persist on the next save.
        m_Registry.ForEachWithComponent<Name>(
            [this](LCE::Simulation::EntityId a_entity, Name& a_name)
            {
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value != Species::Animal)
                {
                    return;
                }

                if (!m_UsedNames.insert(a_name.Full).second)
                {
                    a_name.Full = TLC::Names::GenerateUniqueAnimal(
                        m_UsedNames, a_entity, m_Names);
                }
            });

        // The name's visible half after a restore: every mind whose
        // actor is loaded gets its name written onto the actor — the
        // same reconcile-aware rule as the per-second sweep (the base
        // form is the truth; stale generated stamps on game-named NPCs
        // are dropped). The sweep re-applies as the rest stream in.
        ApplyLoadedActorNames();

        // The market was saved with the world (it owns a FormRef); now
        // that the world is back, every mind must remember where to trade
        // again. The seed is a fading memory event (weight 1.0, forgotten
        // in seconds) — a mind saved long after its world woke has already
        // forgotten the market, and without the seed a restored world is
        // market-blind: starving minds Explore instead of walking. The
        // tick's periodic refresh keeps catching minds whose actors load
        // after this instant.
        SeedMarket(true);

        // The conflict source's settlement (0.7.0 Stone 2): every mind
        // with a restored market memory belongs to that settlement's
        // group — the engine's echo spreads a slight (or a warmth)
        // through it, and newcomers inherit the settlement's cold
        // shoulder toward a feud's villain. Derived from the restored
        // memories, never persisted itself.
        AssignSettlementGroups();

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

        const auto children = CountSimOnlyChildren();

        if (children > 0)
        {
            REX::INFO(
                "The Commonwealth wakes up: {} sim-only {} restored too — fed by their households.",
                children, children == 1 ? "child" : "children");
            LCE::Logging::Info(
                "The Commonwealth wakes up: " + std::to_string(children)
                + (children == 1 ? " sim-only child restored too — fed by its household."
                                 : " sim-only children restored too — fed by their households."));
        }

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

        // The identity stone (0.7.0 Stone 1): every mind is born with a
        // name. The game's own name wins — Sturges stays Sturges — and
        // a generic "Settler" gets a procedural Commonwealth name: the
        // actor's sex picks the first-name list (male or female; an
        // unset sex draws from the id), deduped against the world's live
        // names (no two "Vance" in the same room). An owned animal draws
        // from its own pool — a dog is "Rex", not "Rex Hart" — and a
        // stray with no owner stays nameless: the log labels it by
        // species and hex until someone claims it.
        std::string fullName;

        // The name must come from the BASE form, not the reference: the
        // reference's own full-name is the sparse map, which is empty
        // for most actors — reading the ref made EVERYONE look generic
        // and renamed Mama Murphy into "Milo Grey". The base form holds
        // the real name (Sturges, Mama Murphy, even "Dog").
        const auto gameName = a_actor->GetObjectReference()
            ? RE::TESFullName::GetFullName(*a_actor->GetObjectReference())
            : std::string_view{};

        // The role may live on the reference, not the base: the game
        // names a supply-line settler "Provisioner" itself, so the
        // base form is still the generic "Settler". Read the actor's
        // display name first (it falls back to the base form) and
        // prefer it for the role rule — real names and placeholders are
        // still decided by the base form exactly as before; the display
        // read is only ever consulted as a role candidate.
        const auto* shownName = const_cast<RE::Actor*>(a_actor)
            ->GetDisplayFullName();
        const auto displayName = shownName
            ? std::string_view(shownName)
            : std::string_view{};

        // A game name is the truth — unless it is a placeholder or a
        // role label. A placeholder ("Settler") gets a full procedural
        // name; a role label ("Provisioner", "Guard") keeps its
        // title and gains the person — "Provisioner Cole" — so memory
        // can tell two provisioners apart (0.7.3 Stone 1).
        auto role = species == Species::Human
            ? TLC::Names::IsRoleName(displayName)
            : std::string_view{};

        if (role.empty() && species == Species::Human)
        {
            role = TLC::Names::IsRoleName(gameName);
        }

        if (TLC::Names::IsGenericName(gameName, species) || !role.empty())
        {
            if (species == Species::Animal)
            {
                // GetOwner is non-const in the game API; the process
                // lists hand us a live, non-const actor — the cast is
                // contained to the read.
                if (const_cast<RE::Actor*>(a_actor)->GetOwner() != nullptr)
                {
                    fullName = TLC::Names::GenerateUniqueAnimal(
                        m_UsedNames, id, m_Names);
                }
            }
            else
            {
                auto gender = TLC::Names::GenderOf(id);
                const auto sex =
                    const_cast<RE::Actor*>(a_actor)->GetSex();

                if (sex == RE::SEX::kMale)
                {
                    gender = TLC::Names::Gender::Male;
                }
                else if (sex == RE::SEX::kFemale)
                {
                    gender = TLC::Names::Gender::Female;
                }

                fullName = role.empty()
                    ? TLC::Names::GenerateUnique(
                        m_UsedNames, id, m_Names, gender)
                    : TLC::Names::GenerateUniqueRole(
                        m_UsedNames, role, id, m_Names, gender);
            }

            // The name's visible half: write it onto the actor's extra
            // data so the game shows it — the pip-boy, the hover, the
            // workshop. Only for a name the sim generated here; a
            // game-named NPC (Sturges) is never touched.
            ApplyActorName(formId, fullName);
        }
        else
        {
            fullName = std::string(gameName);
        }

        // A stray stays unnamed — no Name component, so the label falls
        // back to species + hex until someone owns it.
        if (!fullName.empty())
        {
            m_Registry.AddComponent<Name>(id, Name{ fullName });
            m_UsedNames.insert(fullName);
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

    void Adapter::PruneDeviceMinds()
    {
        // A polluted co-save holds the workshop's props as minds (they
        // hold the settler faction, and before the exclusion they seeded
        // as Human — needs, walks, feuds). The prune runs on restore:
        // a fresh world never seeds them (IsSimRelevant now excludes
        // device/robot races), and the polluted world self-heals here —
        // quietly, one summary line, before pouches, names, or bonds
        // are rebuilt.
        std::vector<std::uint32_t> pruned;

        m_Registry.ForEachWithComponent<FormRef>(
            [&](LCE::Simulation::EntityId a_entity, FormRef& a_form)
            {
                // Targets (workshops) carry a FormRef but no SpeciesTag
                // — they are places to walk to, never minds.
                if (m_Registry.GetComponent<SpeciesTag>(a_entity) == nullptr)
                {
                    return;
                }

                auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(a_form.FormId);

                if (actor != nullptr && !IsSimRelevant(actor))
                {
                    pruned.push_back(a_form.FormId);
                }
            });

        for (const auto formId : pruned)
        {
            RemoveMind(formId, false, true);
        }

        if (!pruned.empty())
        {
            REX::INFO(
                "sim: {} workshop device{} pruned — turrets, spotlights, "
                "and robots are not minds.",
                pruned.size(), pruned.size() == 1 ? "" : "s");
        }

        // The table's blind spot (2026-08-12): the wall-mounted spotlight
        // slipped the device list — its race is neither a known device nor
        // a known organic race, so it survived the prune and became a
        // Human mind. Announce any such mind once per session, with its
        // race hex, so the device table can grow. A modded organic race
        // warns once and then stays a settler — default Human is the
        // design (ADR-0024); the line is how the table learns.
        std::unordered_set<std::uint32_t> announced;

        m_Registry.ForEachWithComponent<FormRef>(
            [&](LCE::Simulation::EntityId a_entity, FormRef& a_form)
            {
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value != Species::Human)
                {
                    return;
                }

                auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(a_form.FormId);

                if (actor == nullptr || actor->race == nullptr
                    || IsKnownOrganicRace(actor->race))
                {
                    return;
                }

                if (!announced.insert(actor->race->GetFormID()).second)
                {
                    return;
                }

                const auto* npc = actor->GetNPC();

                REX::WARN(
                    "sim: {:#x} ({}) is a Human mind with unknown race "
                    "{:#x} — if this is a device, its race belongs in the "
                    "device table (SimRelevant.cpp).",
                    a_form.FormId,
                    npc != nullptr
                        ? RE::TESFullName::GetFullName(*npc)
                        : "?",
                    actor->race->GetFormID());
            });
    }

    void Adapter::RemoveMind(
        std::uint32_t a_formId, bool a_isDeath, bool a_quiet)
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
        m_ArrivedAt.erase(entity);
        m_LastWander.erase(entity);
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
            // The death fact — the settlement's grief news (Stone 1,
            // spread through the gossip channel of Stone 4): every
            // surviving mind remembers who is gone — { the dead, Death,
            // weight, day }. A fact, never a door: Decide gates only
            // Trade and Social, so a death never blocks a walk or a
            // trade. Survivors carry it across the co-save; the dead
            // themselves are simply absent (they do not restore).
            // Grief reads it in Stone 5.
            const auto mourning = Gossip::Spread(
                m_Registry, entity,
                LCE::Simulation::InteractionKind::Death,
                CurrentDay());

            // The grief announce's memory (0.6.0 Stone 5): the dead is
            // about to leave the translator forever — FormFor can no
            // longer answer — but the grief arc needs the form to say
            // who is mourned. Recorded before the destroy, consulted by
            // the announce, cleared on EndWorld.
            m_RecentDeaths[entity.Value()] = a_formId;

            // 0.7.0 Legacy (engine stones 10–12): what the dead leaves
            // behind. The household's heir — the spouse — receives the
            // dead's memories at or above the bequest floor, scaled
            // (the story survives its maker, fainter). And the dead's
            // name becomes a registry-level legacy — the world's memory
            // of who they were, permanent until the world forgets it.
            // Both run before the destroy below: the dead must still be
            // alive for the core.
            if (spouse.IsValid())
            {
                LCE::Simulation::Bequeath(
                    m_Registry, entity,
                    std::span<const LCE::Simulation::EntityId>{ &spouse, 1 },
                    m_CoreTuning);
            }

            if (const auto name = m_Registry.GetComponent<Name>(entity))
            {
                m_Registry.LeaveLegacy(LCE::Simulation::LegacyFact{
                    entity, CurrentDay(), name->Full, 1.0f });
            }

            // The gossip stone's observable half: one line per death —
            // how many minds remember who is gone. The fact itself is
            // silent (a memory is not a door); this is the verify. The
            // name is the identity stone's voice; the hex rides along.
            REX::INFO(
                "gossip: {} {} remember {} is gone.",
                mourning, mourning == 1 ? "mind" : "minds",
                MindLabelForm(a_formId));

            PushNews(MindLabelForm(a_formId) + " died.");
        }

        // The label before the remove: the translator forgets the form
        // below, so the farewell line keeps the name it just lost.
        const auto goneLabel = MindLabelForm(a_formId);

        m_Registry.DestroyEntity(entity);
        m_Translator.Remove(a_formId);

        if (!a_quiet)
        {
            REX::INFO(
                "lifecycle: {} {} — the world keeps its books.",
                goneLabel, a_isDeath ? "died" : "left the settlement");
        }
    }

    std::uint64_t Adapter::CurrentDay() const
    {
        const auto* calendar = RE::Calendar::GetSingleton();

        return calendar != nullptr && calendar->gameDaysPassed != nullptr
            ? std::uint64_t(calendar->gameDaysPassed->value)
            : 0;
    }

    std::string Adapter::MindLabel(
        LCE::Simulation::EntityId a_entity) const
    {
        const auto formId = m_Translator.FormFor(a_entity);

        if (const auto name = m_Registry.GetComponent<Name>(a_entity))
        {
            return name->Full + " [" + FormatHex8(formId) + "]";
        }

        // No name — a stray animal (nobody claimed it yet), or a mind
        // predating the identity stone mid-world. The species label
        // keeps the log's voice: "animal [FF0197BF]", not a bare hex.
        return std::string(SpeciesLabel(
                   m_Registry.GetComponent<SpeciesTag>(a_entity).get()))
            + " [" + FormatHex8(formId) + "]";
    }

    std::string Adapter::MindLabelForm(std::uint32_t a_formId) const
    {
        const auto entity = m_Translator.EntityFor(a_formId);

        if (entity.IsValid())
        {
            return MindLabel(entity);
        }

        // No entity — a workshop target, or a form the sim does not
        // know. A workshop is a market; name it from its base form.
        return MarketLabel(a_formId);
    }

    std::string Adapter::MarketLabel(std::uint32_t a_formId) const
    {
        const auto* form = RE::TESForm::GetFormByID(a_formId);

        if (form != nullptr)
        {
            const auto name = RE::TESFullName::GetFullName(*form);

            if (!name.empty())
            {
                return std::string(name) + " [" + FormatHex8(a_formId) + "]";
            }
        }

        return FormatHex8(a_formId);
    }

    void Adapter::ApplyActorName(
        std::uint32_t a_formId, const std::string& a_name) const
    {
        if (a_name.empty())
        {
            return;
        }

        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(a_formId);

        if (actor == nullptr || actor->extraList == nullptr)
        {
            // Not loaded (a restored world's actors stream in slowly) —
            // the restore pass re-applies names as actors load, and the
            // actor's own extra data persists it once written.
            return;
        }

        actor->extraList->SetOverrideName(a_name.c_str());
    }

    void Adapter::ApplyLoadedActorNames()
    {
        ForEachLoadedActor(
            [this](const RE::Actor* a_actor)
            {
                const auto formId = a_actor->GetFormID();
                const auto entity = m_Translator.EntityFor(formId);

                if (!entity.IsValid())
                {
                    return;
                }

                const auto name = m_Registry.GetComponent<Name>(entity);

                if (name == nullptr || name->Full.empty())
                {
                    return;
                }

                auto* actor = const_cast<RE::Actor*>(a_actor);

                if (actor->extraList == nullptr)
                {
                    return;
                }

                const auto tag = m_Registry.GetComponent<SpeciesTag>(entity);
                const auto species = tag ? tag->Value : Species::Human;

                // The base form is the eternal truth: if the game gave
                // this NPC a real name, the mind must carry it — and an
                // earlier build's stale generated stamp ("Milo Grey" on
                // Mama Murphy) is dropped so the real name shows again.
                // The co-save holds the corrected name from the next
                // save onward.
                const auto baseName = actor->GetObjectReference()
                    ? RE::TESFullName::GetFullName(
                        *actor->GetObjectReference())
                    : std::string_view{};

                // A role label is a title, not a name (0.7.3 Stone 1):
                // the mind wears "Provisioner Cole", never the bare
                // "Provisioner" — memory must tell two provisioners
                // apart. The role may live on the reference rather than
                // the base (the game names supply-line settlers
                // "Provisioner" itself), so the display name is read
                // first. The base never overrides the role name (the
                // converge below would stamp the bare role word back
                // on), and a mind still wearing a pre-0.7.3 name (the
                // bare role word, or a restore-time full name) converges
                // to its role name here.
                if (species == Species::Human)
                {
                    const auto* shown = actor->GetDisplayFullName();
                    const auto displayName = shown
                        ? std::string_view(shown)
                        : std::string_view{};

                    auto role = TLC::Names::IsRoleName(displayName);

                    if (role.empty())
                    {
                        role = TLC::Names::IsRoleName(baseName);
                    }

                    if (!role.empty())
                    {
                        auto& mind = name->Full;

                        if (mind.empty()
                            || TLC::Names::IsGenericName(
                                mind, Species::Human)
                            || !TLC::Names::HasRolePrefix(mind, role))
                        {
                            auto gender = TLC::Names::GenderOf(entity);
                            const auto sex = actor->GetSex();

                            if (sex == RE::SEX::kMale)
                            {
                                gender = TLC::Names::Gender::Male;
                            }
                            else if (sex == RE::SEX::kFemale)
                            {
                                gender = TLC::Names::Gender::Female;
                            }

                            mind = TLC::Names::GenerateUniqueRole(
                                m_UsedNames, role, entity,
                                m_Names, gender);
                        }

                        // The game names supply-line settlers itself:
                        // the actor can already wear the bare role word
                        // as a text-display override, which would
                        // swallow the sim's name. Write through when
                        // the actor shows nothing or the bare role — a
                        // different deliberate name (a player rename)
                        // is respected. Verified in-game: the write is
                        // a no-op against the game's own mask while the
                        // actor is assigned to a supply line (the label
                        // is re-derived from the assignment, ahead of
                        // any extra-data override — a universal FO4
                        // limitation). It still lands the moment the
                        // provisioner is reassigned to another role, so
                        // the write stays.
                        if (shown == nullptr
                            || TLC::Names::EqualsFold(
                                displayName, role))
                        {
                            actor->extraList->SetOverrideName(
                                mind.c_str());
                        }

                        return;
                    }
                }

                if (!TLC::Names::IsGenericName(baseName, species))
                {
                    if (name->Full != baseName)
                    {
                        m_Registry.GetComponent<Name>(entity)->Full =
                            std::string(baseName);

                        if (actor->extraList->HasType(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData))
                        {
                            actor->extraList->RemoveExtra(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData);
                        }
                    }

                    return;
                }

                // A generic base ("Settler", "Dog"): write the sim's
                // name once — an actor already showing it is left alone.
                // An animal whose mind still holds the raw species word
                // (a kept game name from before the naming rule) or a
                // stale generated name converges to the rule: owned → a
                // real name from the pool; a stray → nameless, its
                // stamp dropped.
                if (species == Species::Animal
                    && TLC::Names::IsGenericName(
                        name->Full, Species::Animal))
                {
                    if (actor->GetOwner() != nullptr)
                    {
                        m_Registry.GetComponent<Name>(entity)->Full =
                            TLC::Names::GenerateUniqueAnimal(
                                m_UsedNames, entity, m_Names);
                    }
                    else
                    {
                        m_Registry.RemoveComponent<Name>(entity);

                        if (actor->extraList->HasType(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData))
                        {
                            actor->extraList->RemoveExtra(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData);
                        }

                        return;
                    }
                }

                if (!actor->extraList->HasType(
                        RE::EXTRA_DATA_TYPE::kTextDisplayData))
                {
                    actor->extraList->SetOverrideName(name->Full.c_str());
                }
            });
    }

    void Adapter::AssignSettlementGroups()
    {
        using namespace LCE::Simulation;

        // No census, no settlements — the group is the world, and there
        // is nothing to derive membership from (the fallback market is
        // a workshop entity, so it appears in m_Workshops once the
        // census finds nothing and EnsureWorkshop pins it — this guard
        // only skips a truly empty world).
        if (m_Workshops.empty())
        {
            return;
        }

        m_Registry.ForEachWithComponent<Memory>(
            [this](EntityId a_entity, Memory& a_memory)
            {
                if (m_Registry.GetComponent<Groups>(a_entity) != nullptr)
                {
                    return;   // already has a home
                }

                // The settlement is the market the mind remembers — a
                // Trade-kind event whose Other is a workshop form (not a
                // person; the remembered merchant is an actor, and a
                // mind's market is the bench). The group id is the
                // market's form id: deterministic, session-stable, and
                // the same for every mind of the settlement.
                for (const auto& event : a_memory.Events)
                {
                    if (event.Kind != InteractionKind::Trade
                        || !event.Other.IsValid())
                    {
                        continue;
                    }

                    const auto formId = m_Translator.FormFor(event.Other);
                    const auto isWorkshop = formId != 0
                        && std::find_if(
                               m_Workshops.begin(), m_Workshops.end(),
                               [formId](const WorkshopPosition& a_workshop)
                               {
                                   return a_workshop.FormId == formId;
                               })
                            != m_Workshops.end();

                    if (isWorkshop)
                    {
                        m_Registry.AddComponent<Groups>(
                            a_entity,
                            Groups{ { GroupId{ formId } } });
                        return;
                    }
                }
            });
    }

    void Adapter::PushNews(const std::string& a_line)
    {
        m_News.Add(a_line);

        // The on-screen window (0.7.0 Stone 3), throttled: a world of
        // news is still a flood if every line pops at once, so events
        // queue into the feed and the screen shows at most one per
        // sim.news.cooldown seconds. The feed is the radio's story; the
        // log's own line stays the verify channel.
        if (!m_Settings.NewsEnabled)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (now - m_LastNews >= std::chrono::duration<float>(
                m_Settings.NewsCooldown))
        {
            m_LastNews = now;
            // The HUD diagnostic (0.8.1 verification): the on-screen pop is
            // the one thing the log cannot see — a stall inside
            // ShowHUDMessage would show as a gap after this line. Timestamp
            // each pop so the next session can prove (or clear) the
            // message-path hang.
            REX::DEBUG("hud: pop '{}'", a_line);
            RE::SendHUDMessage::ShowHUDMessage(
                a_line.c_str(), "", false, false);
        }
    }

    void Adapter::Say(
        LCE::Simulation::EntityId a_speaker,
        LCE::Simulation::EntityId a_listener,
        Dialogue::Pool a_pool)
    {
        // One line, deterministic per mind and day — the same mind says
        // the same line all day and a different one tomorrow (Pick). An
        // empty pool says nothing: silence is a safe default.
        const auto line =
            Dialogue::Pick(m_Dialogue, a_pool, a_speaker, CurrentDay());

        if (line.empty())
        {
            return;
        }

        const auto speakerForm = m_Translator.FormFor(a_speaker);
        const auto listenerForm = m_Translator.FormFor(a_listener);

        // The verify channel: the log reads as dialogue — who said what
        // to whom.
        REX::INFO(
            "LCE: {} to {}: \"{}\"",
            MindLabelForm(speakerForm), MindLabelForm(listenerForm), line);

        // The radio channel: the same feed the settlement radio reads as
        // on-screen captions, so a conversation line pops while the
        // player is near. (PushNews would also pop a HUD notification;
        // speech is quieter — the feed alone is the radio's story.)
        m_News.Add(MindLabelForm(speakerForm) + ": \"" + line + "\"");
    }

    void Adapter::EscalateToFight(
        LCE::Simulation::EntityId a_aggressor,
        LCE::Simulation::EntityId a_victim,
        std::uint64_t a_day)
    {
        // Three gates, all must pass (Fights::RollFight): the pair is
        // an enemy feud (rivals stay verbal — the verbal-first rule),
        // the aggressor's temper is at or above sim.fight.temper (the
        // churlish throw the punch), and the world's coin lands under
        // sim.fight.chance (1.0 forces every eligible escalation — the
        // test knob). The once-per-day gate lives in BookFight (a
        // Combat memory stamped today, co-saved).
        const auto kind = Bonds::CurrentKind(m_Bonds, a_aggressor, a_victim);
        const float roll = m_Rng.NextFloat(0.0f, 1.0f);

        if (!Fights::RollFight(
                kind, TemperOf(a_aggressor),
                m_Settings.FightTemper, m_Settings.FightChance, roll))
        {
            return;
        }

        if (!Fights::BookFight(
                m_Registry, m_Bonds, a_aggressor, a_victim,
                a_day, m_CoreTuning, &m_Bus))
        {
            return;
        }

        // The words before the blows (0.7.1 Talk's fight pool — "Come
        // on then!", "Put 'em up"): speech rides the news feed, so the
        // radio reads the fight as a caption too.
        Say(a_aggressor, a_victim, Dialogue::Pool::Fight);

        const auto a = MindLabelForm(m_Translator.FormFor(a_aggressor));
        const auto b = MindLabelForm(m_Translator.FormFor(a_victim));

        PushNews(a + " and " + b
            + " come to blows — the feud turns physical.");

        REX::INFO(
            "LCE: {} and {} come to blows — the feud turns physical.",
            a, b);
    }

    void Adapter::RadioCaptions()
    {
        if (!m_Settings.NewsEnabled)
        {
            return;
        }

        // A settlement radio the player built: while one is near, the
        // settlement tells its story — the news feed as on-screen
        // captions, one per sim.radio.caption.every seconds. The base
        // form is radio.base.formid (default the workshop "Radio" — a
        // hardcoded FormID, flagged for xEdit verification; the key
        // exists so a wrong pin is a config line, never a rebuild).
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (player == nullptr)
        {
            return;
        }

        auto* cell = player->GetParentCell();

        if (cell == nullptr)
        {
            return;
        }

        bool radioNearby = false;

        cell->ForEachReferenceInRange(
            player->GetPosition(), m_Settings.RadioRadius,
            [&](RE::TESObjectREFR* a_ref) -> RE::BSContainer::ForEachResult
            {
                if (a_ref == nullptr)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto* base = a_ref->GetObjectReference();

                if (base != nullptr
                    && base->GetFormID() == m_Settings.RadioBaseFormId)
                {
                    radioNearby = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

        if (!radioNearby)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (now - m_LastRadioCaption < std::chrono::duration<float>(
                m_Settings.RadioCaptionEvery))
        {
            return;
        }

        m_LastRadioCaption = now;

        const auto line = m_News.NextLine();

        if (!line.empty())
        {
            // Same HUD diagnostic as PushNews: prove or clear the
            // message-path stall (0.8.1 verification).
            REX::DEBUG("hud: radio '{}'", line);
            RE::SendHUDMessage::ShowHUDMessage(
                line.c_str(), "", false, false);
        }
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

        // The sellers (0.7.4 Trade with anyone): who sells in the loaded
        // world, remembered by the minds near them — a person who sells
        // out-scores the bench while both are fresh (the seed weight is
        // a hair above the market's), so a hungry mind trades with the
        // trader on the road or at the market stall, not only the bench.
        SeedVendors(true);

        // The conflict source's settlement (0.7.0 Stone 2): every mind
        // with a market memory belongs to its settlement's group — the
        // engine's echo then spreads a slight (or a warmth) through the
        // group, and InheritGroupAttitudes gives a newcomer the
        // settlement's inherited feelings. Derived from the seeded
        // memories, never persisted.
        AssignSettlementGroups();

        REX::INFO("The Commonwealth wakes up: {} settlers became minds.", count);
        LCE::Logging::Info(
            "The Commonwealth wakes up: " + std::to_string(count) + " settlers became minds.");

        const auto children = CountSimOnlyChildren();

        if (children > 0)
        {
            REX::INFO(
                "The Commonwealth wakes up: {} sim-only {} born to their households.",
                children, children == 1 ? "child" : "children");
            LCE::Logging::Info(
                "The Commonwealth wakes up: " + std::to_string(children)
                + (children == 1 ? " sim-only child born to its household."
                                 : " sim-only children born to their households."));
        }

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
        m_ArrivedAt.clear();
        m_MarketAttendance.clear();
        m_LastWander.clear();
        m_RecentDeaths.clear();
        m_GriefAnnounced.clear();
        m_PendingDeaths.clear();
        m_SeenAlive.clear();
        m_UsedNames.clear();
        m_News.Clear();
        m_TickCalled = false;
        m_FirstPassLogged = false;
        m_Started = false;
    }

    std::size_t Adapter::CountSimOnlyChildren()
    {
        std::size_t count = 0;

        m_Registry.ForEachWithComponent<SpeciesTag>(
            [&](LCE::Simulation::EntityId a_entity,
                const SpeciesTag& a_tag)
            {
                if (a_tag.Value == Species::Child
                    && m_Registry.GetComponent<FormRef>(a_entity) == nullptr)
                {
                    ++count;
                }
            });

        return count;
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
        LCE::Simulation::EntityId familySpouse{};   // who the family meal warms
        std::uint32_t marketFormId = a_targetFormId;

        // The walk target is a bench when it carries no species tag —
        // the market itself. (A person target resolves to a merchant or
        // a remembered trader — a mind, not a place.) The Rows crossing
        // scan runs only at benches: the feud's geography (Identity.md)
        // is the stall.
        const auto targetTag =
            m_Registry.GetComponent<SpeciesTag>(target);
        const bool atBench = targetTag == nullptr;

        if (species == Species::Human)
        {
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
                    familySpouse = spouse;
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

        // The row (0.7.2 Rows): rivals and enemies who cross paths at
        // the same bench have words — the feud's audible half. The
        // attendance book is who walked here today (ephemeral, pruned
        // to the day); the scan finds a feud partner already here, and
        // Rows::Exchange books the wrong on both sides (engine Wronged,
        // −0.25 — an unprompted wrong, not the −0.1 executed let-down
        // of a shut stall), gossips the shouting to the settlement, and
        // publishes on the bus — so a crossing that pushes the pair
        // over the enemy line fires OnBondChange the instant it
        // happens. Words once a day per pair: the memory gate is
        // co-saved, so save/load never double-rows.
        if (atBench)
        {
            const auto day = CurrentDay();
            auto& attendees = m_MarketAttendance[marketFormId];

            attendees.erase(
                std::remove_if(
                    attendees.begin(), attendees.end(),
                    [day](const auto& entry)
                    {
                        return entry.second != day;
                    }),
                attendees.end());

            // The keeper stands at the bench — the feud's geography is
            // the stall. The attendance book only sees walkers ("who
            // walked here today"), so a keeper planted or restored at
            // her own bench never enters it: the very mind every
            // shut-stall slight is aimed at could never row back. Scan
            // her directly alongside today's walkers; the once-per-day
            // Wronged gate makes a keeper who did arrive today a
            // harmless double-scan.
            const auto keeperIt = m_StallKeepers.find(target);
            const auto keeper = keeperIt != m_StallKeepers.end()
                ? keeperIt->second
                : LCE::Simulation::EntityId{};

            const auto cross = [&](LCE::Simulation::EntityId a_other)
            {
                if (!a_other.IsValid() || a_other == a_entity)
                {
                    return;
                }

                if (Rows::Exchange(
                        m_Registry, m_Bonds,
                        a_entity, a_other, day, m_CoreTuning, &m_Bus))
                {
                    // The exchange itself: each says a line to the
                    // other — two voices, the shouting the settlement
                    // hears. Speech rides the news feed, so the radio
                    // reads the row as a caption.
                    Say(a_entity, a_other, Dialogue::Pool::Row);
                    Say(a_other, a_entity, Dialogue::Pool::Row);

                    REX::INFO(
                        "LCE: {} and {} row at the market — words first.",
                        MindLabelForm(formId),
                        MindLabelForm(m_Translator.FormFor(a_other)));

                    // The physical escalation (0.7.5 Fights): a row
                    // between enemies can turn to blows — the temper
                    // and chance rolls decide; the punch lands (Combat
                    // both ways), the feud deepens, and the victim
                    // carries a threat. Rivals stay verbal.
                    EscalateToFight(a_entity, a_other, day);
                }
            };

            for (const auto& [other, otherDay] : attendees)
            {
                if (otherDay == day)
                {
                    cross(other);
                }
            }

            cross(keeper);

            attendees.emplace_back(a_entity, day);
        }

        // The conflict source (0.7.0 Stone 2): a hungry human arrived
        // and the stall is shut. The world-facts gate stops new walks
        // after hours, but a walk already in flight when the hour turns
        // still arrives. No trade, no food — and the mind remembers the
        // let-down. The engine's decided channel for an executed
        // interaction that went badly: ReportOutcome({ keeper, Social,
        // Failure }) cools the disposition 0.1 toward the stall-keeper,
        // and the settlement echo spreads the chill through the group
        // (Simulation.cpp — the sign lives in the kind and the result,
        // never on the weight). A forgiving mind (temper below
        // sim.slight.temper) blames no one — a world outcome, memory
        // only: the stall was just shut. The feud's fuel: two or three
        // shut-stall let-downs cross the rival line, and the 0.6.0 feud
        // arc — gossip, mediation, the settlement's inherited cold
        // shoulder — takes it from there.
        //
        // Bench only (0.7.4 fix): the stall is the bench's geography.
        // A human who walked to a PERSON — the remembered seller — is
        // at the seller's market, not the settlement's; the seller is
        // there, so the trade lands whatever the hour. (The world-facts
        // gate still stops new walks after hours; only walks already in
        // flight arrive, so night trades are the desperate ones.)
        const bool closed = WorldFacts::IsMarketClosed(
            CurrentGameHour(),
            m_Settings.MarketOpenHour, m_Settings.MarketCloseHour);

        if (closed && species == Species::Human
            && atBench && !keeperHome && !familyHome)
        {
            const auto keeperIterator = m_StallKeepers.find(target);
            const auto keeper = keeperIterator != m_StallKeepers.end()
                ? keeperIterator->second
                : EntityId{};

            const bool slighted =
                TemperOf(a_entity) >= m_Settings.SlightTemper;

            ReportOutcome(
                m_Registry, a_entity,
                Outcome{
                    (slighted && keeper.IsValid()) ? keeper : EntityId{},
                    InteractionKind::Social, OutcomeResult::Failure,
                    1.0f },
                m_CoreTuning, &m_Bus, WorldTime{ CurrentDay() });

            REX::INFO(
                "LCE: the stall at {} is shut — {} went hungry{}.",
                MarketLabel(marketFormId), MindLabelForm(formId),
                slighted ? " and blames the keeper" : "");

            // The first words of a feud (0.7.1 Talk, feeding 0.7.2
            // Rows): a slighted mind does not just blame silently — it
            // says something to the keeper. The row pool's early lines
            // ("You ripped me off") are exactly this moment. When the
            // keeper is a feud partner who already rowed here this
            // arrival (the crossing above), the words are spoken — the
            // slight's own Say would repeat the same line, so it stays
            // silent.
            if (slighted && keeper.IsValid()
                && !Rows::AlreadyRowedToday(
                    m_Registry, a_entity, keeper, CurrentDay()))
            {
                Say(a_entity, keeper, Dialogue::Pool::Row);
            }

            // The feud turns physical (0.7.5 Fights): a slighted mind
            // facing an enemy keeper throws the punch instead of just
            // the words — the reliable test path (force the market
            // shut, the slight fires, the enemy keeper catches a fist).
            if (slighted && keeper.IsValid())
            {
                EscalateToFight(a_entity, keeper, CurrentDay());
            }

            return;
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
                        "LCE: {} trades with {} at market {} — fed, {} caps change hands ({}{} left, {} now).",
                        MindLabelForm(formId), MindLabelForm(traderFormId),
                        MarketLabel(marketFormId),
                        paid,
                        paidFromHousehold ? "household; " : "",
                        buyerCaps, sellerCaps);

                    // The market's words (0.7.1 Talk): a paid meal is a
                    // conversation — the buyer says something to the
                    // keeper while the caps change hands. Speech rides
                    // the news feed, so the settlement radio reads it.
                    Say(a_entity, counterparty, Dialogue::Pool::Trade);
                }
                else
                {
                    REX::INFO(
                        "LCE: {} trades with {} at market {} — fed on the settlement's credit (no caps).",
                        MindLabelForm(formId), MindLabelForm(traderFormId),
                        MarketLabel(marketFormId));
                }
            }
            else if (familyHome)
            {
                // The family bench is the marriage's heartbeat: a shared
                // meal at home warms both ways — the same Social warmth
                // a bench-sale carries. Without this the couple's
                // dispositions erode by drift (they stopped trading the
                // moment they married — free meals), and a marriage
                // quietly dies in about an hour; grief for a spouse then
                // finds the love gone (the 2026-08-11 grief test).
                if (familySpouse.IsValid() && familySpouse != a_entity)
                {
                    Remember(
                        m_Registry, a_entity,
                        MemoryEvent{
                            familySpouse, InteractionKind::Social,
                            WorldFacts::kFactWeight },
                        m_CoreTuning,
                        WorldTime{ CurrentDay() },
                        &m_Bus);

                    auto spouseMemory =
                        m_Registry.GetComponent<Memory>(familySpouse);
                    auto spouseRelationships =
                        m_Registry.GetComponent<Relationships>(familySpouse);

                    if (spouseMemory && spouseRelationships)
                    {
                        RecordSale(
                            *spouseMemory, *spouseRelationships,
                            a_entity, m_Settings.SaleWarmth);
                    }
                }

                // The family's words (0.7.1 Talk): a meal at home is the
                // warmest conversation there is.
                if (familySpouse.IsValid() && familySpouse != a_entity)
                {
                    Say(a_entity, familySpouse, Dialogue::Pool::Family);
                }

                REX::INFO(
                    "LCE: {} is at the family stall at market {} — fed from the household's meal.",
                    MindLabelForm(formId), MarketLabel(marketFormId));
            }
            else if (keeperHome)
            {
                REX::INFO(
                    "LCE: {} is at their own stall at market {} — no customers yet.",
                    MindLabelForm(formId), MarketLabel(marketFormId));
            }
            else
            {
                REX::INFO(
                    "LCE: {} sets up the stall at market {} — trade begins when customers come.",
                    MindLabelForm(formId), MarketLabel(marketFormId));
            }
        }
        else
        {
            REX::INFO(
                "LCE: {} arrived — fed, gives nothing in return (Aid, Success).",
                MindLabelForm(formId));
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
                    "LCE: {} fed: Hunger {:.2f} -> 1.00",
                    MindLabelForm(formId), previous);
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
                            REX::INFO("The market is open: every mind remembers where to trade (the Sanctuary workshop — 000250FE).");
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
                    "The market is open: every mind remembers where to trade ({} workshops).",
                    m_Workshops.size());
            }
        }
    }

    void Adapter::SeedVendors(bool a_announce)
    {
        using namespace LCE::Simulation;

        // The vendor census: who sells in the loaded world right now
        // (0.7.4 Trade with anyone). A seller is an actor with a
        // merchant container — SimRelevant::IsVendor, the same gate
        // that admits them as minds. Spatial on purpose: a mind only
        // remembers a seller within walking distance, so the census is
        // the loaded lists, not the whole Commonwealth.
        std::vector<TLC::VendorPosition> vendors;

        ForEachLoadedActor(
            [this, &vendors](const RE::Actor* a_actor)
            {
                if (!IsVendor(a_actor))
                {
                    return;
                }

                const auto pos = a_actor->GetPosition();
                vendors.push_back(TLC::VendorPosition{
                    a_actor->GetFormID(), pos.x, pos.y });
            });

        if (a_announce)
        {
            REX::INFO(
                "sim: {} sellers loaded — trade resolves to a person when one is remembered.",
                vendors.size());
        }

        if (vendors.empty())
        {
            return;
        }

        // The loaded sellers by form, for the idempotency check: a mind
        // that already remembers a seller it can still see keeps its
        // memory — the seed only ever adds the fact back to minds that
        // lost it, exactly like the market seed.
        std::unordered_set<std::uint32_t> vendorForms;

        for (const auto& vendor : vendors)
        {
            vendorForms.insert(vendor.FormId);
        }

        m_Registry.ForEachWithComponent<Memory>(
            [this, &vendors, &vendorForms](
                EntityId a_entity, Memory& a_memory)
            {
                // Only people trade. A child or an animal is fed — its
                // food source stays the owner or the settlement, never
                // a seller.
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value != Species::Human)
                {
                    return;
                }

                const auto formId = m_Translator.FormFor(a_entity);
                const auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(formId);

                if (actor == nullptr)
                {
                    return;   // not loaded — the refresh catches it
                }

                const auto pos = actor->GetPosition();
                auto nearest =
                    TLC::NearestVendor(pos.x, pos.y, vendors, kMarketRadius);

                // A seller's own stall is not a customer: a vendor mind
                // is always the nearest vendor to itself (distance 0), so
                // when the nearest is the mind's own form, find the next
                // nearest instead.
                if (nearest == formId)
                {
                    nearest = 0;
                    float bestSq = kMarketRadius * kMarketRadius;

                    for (const auto& vendor : vendors)
                    {
                        if (vendor.FormId == formId)
                        {
                            continue;
                        }

                        const auto dx = vendor.X - pos.x;
                        const auto dy = vendor.Y - pos.y;
                        const auto distSq = dx * dx + dy * dy;

                        if (distSq < bestSq)
                        {
                            bestSq = distSq;
                            nearest = vendor.FormId;
                        }
                    }
                }

                if (nearest == 0)
                {
                    return;   // no seller in walking distance
                }

                const auto seller = m_Translator.EntityFor(nearest);

                if (!seller.IsValid() || seller == a_entity)
                {
                    return;   // no entity yet, or the seller is me
                }

                for (const auto& event : a_memory.Events)
                {
                    if (event.Kind != InteractionKind::Trade
                        || !event.Other.IsValid())
                    {
                        continue;
                    }

                    const auto known = m_Translator.FormFor(event.Other);

                    if (known != 0 && vendorForms.contains(known))
                    {
                        return;   // already remembers a seller here
                    }
                }

                a_memory.Events.push_back(MemoryEvent{
                    seller, InteractionKind::Trade, kVendorSeedWeight });
            });
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
        // log in facts. The player window (0.7.0 Stone 3) turns each
        // transition into a headline.
        if (closed != m_MarketClosed)
        {
            m_MarketClosed = closed;

            if (closed)
            {
                REX::INFO(
                    "world fact: the market is closed ({}) — trade unavailable until {:02.0f}:00.",
                    FormatGameHour(hour), m_Settings.MarketOpenHour);

                PushNews("the market closed for the night.");
            }
            else
            {
                REX::INFO(
                    "world fact: the market is open ({}) — trade available.",
                    FormatGameHour(hour));

                PushNews("the market opened — the Commonwealth trades.");
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

            // 0.7.0 Stone 1's visible tail: a restored world's actors
            // stream in gradually — this names a mind's actor the first
            // time it appears (idempotent; fresh arrivals were already
            // named at seed).
            ApplyLoadedActorNames();

            // 0.6.0 Stone 2 — bonds: the 1-second dissolve net. The
            // event channel is instant; this pass is complete — quiet
            // drift (the core never publishes a dissolve), restores,
            // and anything the bus missed all surface here.
            ReconcileBonds();

            SeedMarket(false);

            // 0.7.4 Trade with anyone: the sellers refresh on the same
            // cadence — vendors stream in and out of the loaded area as
            // they travel, so the who-sells memory re-points at whoever
            // is nearest now (idempotent; silent).
            SeedVendors(false);

            // The conflict source's settlement (0.7.0 Stone 2): late-
            // loading minds join their settlement's group once their
            // market memory lands. Idempotent — a mind with a group
            // keeps it.
            AssignSettlementGroups();

            // The player window (0.7.0 Stone 3): the settlement radio
            // speaks the news while one is near the player.
            RadioCaptions();

            // The world's doors: the market's trading hours and the
            // weather. Pushed on the same cadence as the seed — silent
            // unless a door changes.
            PushWorldFacts();

            // 0.6.0 Stone 5 — the arcs, on a day cadence: mediation for
            // every feud the settlement has heard of, once per day; and
            // birth, at most once per day, when enabled (Stone 6).
            const auto day = CurrentDay();

            if (m_Settings.MediationEnabled
                && day != m_LastMediationDay)
            {
                m_LastMediationDay = day;
                RunMediation();
            }

            if (m_Settings.BirthEnabled && day != m_LastBirthDay)
            {
                m_LastBirthDay = day;
                RunBirth();
            }
        }

        // The grief arc (0.6.0 Stone 5) runs every tick: a grieving mind
        // — a recent death of someone it loved — drains Social faster
        // and seeks company. Derived from persisted components, so no
        // record of its own; the line announces each fresh bereavement
        // once (the memory fades and the line stops on its own).
        {
            const auto grieving = Arcs::ApplyGrief(
                m_Registry, CurrentDay(),
                m_Settings.GriefDecay,
                static_cast<float>(a_deltaSeconds));

            for (const auto& [mind, dead] : grieving)
            {
                // The dead's form is gone from the translator (RemoveMind
                // destroyed it) — the announce reads the recent-deaths
                // map recorded at booking time. Without it the line was
                // dead code (FormFor(dead) was always 0).
                const auto formId = m_Translator.FormFor(mind);
                const auto deadIt = m_RecentDeaths.find(dead.Value());

                if (formId == 0 || deadIt == m_RecentDeaths.end())
                {
                    continue;
                }

                // Once per bereavement, not every frame: the fresh
                // window (weight ≥ 0.9) is ~0.5 s of frames, and the
                // first version announced in all of them (34 lines in
                // half a second, 2026-08-11).
                const auto key = std::make_pair(mind.Value(), dead.Value());

                if (!m_GriefAnnounced.insert(key).second)
                {
                    continue;
                }

                REX::INFO(
                    "arcs: settler {:#x} grieves for {:#x} — they seek company.",
                    formId, deadIt->second);
            }
        }

        // The child's life (0.6.0 Stone 6): sim-only children are fed by
        // their household — no walk, no market, just a full bowl.
        Birth::FeedChildren(
            m_Registry, static_cast<float>(a_deltaSeconds));

        // The sleep cycle (0.6.0): a mind rests while its last decision
        // was Rest — or while the engine silenced it because the need it
        // most urgently feels is one rest fixes. The recovery restores
        // Fatigue, Safety, and Comfort at sim.rest.recovery per second
        // (default 0.2/s, a full nap in ~5 s); the engine's need loop
        // only decays. Keyed on the needs, not just the intent, because
        // a Safety-drained mind with no remembered threat makes the
        // engine's Decide return nullopt (nothing to flee) — the intent
        // is removed and no intent-keyed pass could ever see the mind
        // again (the parked-world discovery, 2026-08-11). Restored
        // before Update so this tick's decisions see the rested mind.
        m_Registry.ForEachWithComponent<Needs>(
            [&](EntityId a_entity, Needs& a_needs)
            {
                const auto intent =
                    m_Registry.GetComponent<LCE::Simulation::Intent>(a_entity);

                bool resting = intent != nullptr
                    && intent->Action == LCE::Simulation::ActionType::Rest;

                if (!resting && intent == nullptr)
                {
                    // No intent: the engine parked the mind (nullopt).
                    // The only need that silences Decide is Safety with
                    // no threat; Fatigue and Safety are the rest-fixable
                    // urgencies — either way, it is time to rest.
                    const auto urgent = MostUrgentNeed(a_needs);

                    resting = urgent.has_value()
                        && (*urgent == LCE::Simulation::NeedType::Fatigue
                            || *urgent == LCE::Simulation::NeedType::Safety);
                }

                if (!resting)
                {
                    return;
                }

                // The recovery value is a defensive marker (-1 when a
                // mind somehow lacks Fatigue); a resting mind with one
                // is recovered, nothing to branch.
                (void)RestRecovery(
                    a_needs,
                    m_Settings.RestRecovery,
                    static_cast<float>(a_deltaSeconds));
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

        // How many MoveTo intents the walk cap refused this pass. Logged
        // once per pass (rate-limited below) so a starved world is
        // visible without the per-frame flood the first attempt caused
        // (5.1M lines in five minutes — every deferred mind logged every
        // frame of the 600-mind revival flood).
        std::size_t deferred = 0;

        // The hold clock (the meal-cadence stone): the same `now` the
        // Rest/Explore branch rate-limits its commanded holds against.
        const auto now = std::chrono::steady_clock::now();

        for (const auto& entry : a_plan)
        {
            const auto actorFormId = m_Translator.FormFor(entry.Entity);
            const auto targetFormId = m_Translator.FormFor(entry.Intent.Target);

            // The identity stone's voice (0.7.0 Stone 1): decisions speak
            // in names — "Vera Hart [00048B77] decides MoveTo -> Sanctuary
            // workshop [000250FE]" — with the console hex beside each.
            const auto actorLabel = MindLabelForm(actorFormId);

            // Refusal is the contract (the intent is a hint, not a command):
            // an unloaded actor, an unloaded target, or a busy actor. The
            // dropped intent is simply re-decided next tick — nothing queued.
            if (!entry.ActorLoaded)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + MindLabelForm(targetFormId) + " — refused: actor not loaded",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId, 1 });
                continue;
            }

            if (!entry.TargetLoaded)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + MindLabelForm(targetFormId) + " — refused: target not loaded",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId, 2 });
                continue;
            }

            if (!entry.Available)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + MindLabelForm(targetFormId) + " — refused: actor busy",
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

                // The arrival-cooldown guard: a mind that arrived at
                // this target within the cooldown is already where it
                // wanted to be — its MoveTo is satisfied without a new
                // walk. Without it, a fed mind standing at its market is
                // always most-urgent-hungry (fast decay rates), so it
                // re-decides MoveTo every frame, re-arrives instantly,
                // and the arrival → feed loop floods the log (the 0.3/s
                // test: 18k animal-fed lines in under a minute). The
                // session was erased on arrival (the slot frees at
                // once), so the walk table alone cannot answer "was this
                // mind just here?" — this map can.
                const auto arrivedIt = m_ArrivedAt.find(entry.Entity);

                const bool justArrived = arrivedIt != m_ArrivedAt.end()
                    && arrivedIt->second.first == targetFormId
                    && now - arrivedIt->second.second
                        < std::chrono::seconds(10);

                if (justArrived
                    || (!session.Reached
                        && session.Target == targetFormId
                        && now - session.Issued < std::chrono::seconds(120)))
                {
                    walked = true;   // already walking / just arrived
                }
                else
                {
                    // Per-market budget: each market's walkers share its
                    // own slice of the cap, so one settlement's hunger
                    // cannot be starved by the Commonwealth-wide flood
                    // (the revival census — 600+ minds all deciding
                    // MoveTo at once — saturates a single global cap and
                    // no market ever trades; the 2026-08-11 test).
                    const auto atThisMarket = std::count_if(
                        m_Walks.begin(), m_Walks.end(),
                        [targetFormId](const auto& a_walk)
                        {
                            return a_walk.second.Target == targetFormId;
                        });

                    if (atThisMarket >= m_Settings.WalkCap)
                    {
                        // Walk cap: erase the session so a refused walk
                        // never lingers (a zombie session — Issued at
                        // the epoch — made ProbeWalks log an instant
                        // "ended" line every frame for every refused
                        // walk; that flood preceded the crash). The mind
                        // re-decides next tick.
                        ++deferred;
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
                            // A refused walk ends the session — erase,
                            // don't reset (see the cap branch: a reset
                            // leaves a zombie that ProbeWalks logs as
                            // instantly ended).
                            m_Walks.erase(entry.Entity);
                        }
                    }
                }

                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides MoveTo -> " + MindLabelForm(targetFormId)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId,
                        walked ? 0u : 4u });
            }
            break;

            case ActionType::Rest:
            case ActionType::Explore:
            {
                // The meal-cadence stone: Rest and Explore execute as a
                // bounded wander — a real nearby reference in the
                // actor's own cell (Movement::WanderNear), furniture
                // preferred. The first hold implementation parked the
                // actor in place and worked (meals collapsed from
                // minutes to ~10 s), but it froze the settlement:
                // everyone stood still at the bench. The wander keeps
                // the cadence — the sandbox cannot drift the actor away
                // before the next command — while the world looks
                // alive, and the game plays its own idles between
                // commands (it may even sit a settler at the bench it
                // walked to). Rate-limited to one wander per mind per
                // sim.wander.cooldown (default 30 s — re-issuing
                // mid-walk would yank the actor to a new target),
                // ranging sim.wander.radius (default 4000 units).
                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), 0, 0 });

                const auto wanderIt = m_LastWander.find(entry.Entity);

                if (wanderIt == m_LastWander.end()
                    || now - wanderIt->second
                        >= std::chrono::duration<float>(m_Settings.WanderCooldown))
                {
                    m_LastWander[entry.Entity] = now;
                    Movement::WanderNear(actor, m_Settings.WanderRadius);
                }
            }
            break;

            case ActionType::Socialize:
            case ActionType::Work:
            case ActionType::Flee:
            {
                // Table slots still: socializing is the future stone,
                // work and fleeing are unbuilt. The loop is proven;
                // these execute nothing in-game yet.
                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), 0, 0 });
            }
            break;
            }
        }

        // One line per pass, rate-limited to every few seconds: a starved
        // world stays visible without the per-frame flood (the first
        // attempt — a per-entity DEBUG line — wrote 5.1M lines in five
        // minutes of the 600-mind revival flood).
        if (deferred > 0
            && (m_LastCapLog.time_since_epoch().count() == 0
                || now - m_LastCapLog >= std::chrono::seconds(5)))
        {
            m_LastCapLog = now;
            REX::DEBUG(
                "LCE: walk cap — {} MoveTo(s) deferred this pass ({} active walks, per-market cap {}).",
                deferred, m_Walks.size(), m_Settings.WalkCap);
        }
    }

    void Adapter::LogPlanEntry(
        LCE::Simulation::EntityId a_entity,
        std::string a_message,
        const LogKey& a_key)
    {
        // One line per mind per LogDecisionEvery seconds, at most — and a
        // mind whose intent is unchanged stays quiet after its first line
        // (the pre-0.7.4 key dedupe). Both rules exist because of the
        // 600-mind restored world, not the 11-mind test:
        //   1. A mind flip-flopping between near-tied intents (Rest/Explore
        //      at the same confidence: two needs both ~full, 'most urgent'
        //      swaps as they decay) would write one line per frame per
        //      mind — 22k lines in under three minutes of synchronous
        //      file I/O on the game thread, the drag behind the growing
        //      frame hang. The change-cap holds it to one line per second.
        //   2. A stable mind printing once per second is 600 lines per
        //      second in a restored world — the 0.8.1 field finding that
        //      turned 'little hangs' back on. Same key = not news: print
        //      once, then silence until the intent actually changes.
        const auto now = std::chrono::steady_clock::now();
        const auto it = m_LastLogged.find(a_entity);

        if (it != m_LastLogged.end()
            && (it->second.first == a_key
                || now - it->second.second
                    < std::chrono::duration<float>(m_Settings.LogDecisionEvery)))
        {
            return;   // same intent (quiet) or a recent line (capped)
        }

        m_LastLogged[a_entity] = { a_key, now };
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
                        "LCE: walk session {} ended (closest approach {:.1f} u).",
                        MindLabelForm(m_Translator.FormFor(it->first)),
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

                // Remember the arrival — the guard the MoveTo branch
                // reads: a mind that just got here has its next MoveTo
                // to the same place treated as satisfied, so a fed mind
                // standing at its market cannot loop MoveTo → instant
                // arrival → feed every frame (the 0.3/s hunger test:
                // at fast decay a full mind is always most-urgent-
                // hungry and the walk layer was re-issuing the trip it
                // just completed).
                m_ArrivedAt[it->first] = { session.Target, now };

                ReportArrival(it->first, session.Target);
                REX::INFO(
                    "LCE: {} arrived (d = {:.1f} u).",
                    MindLabelForm(actorFormId), d);

                // The trip is done — end the session now so the walk
                // slot frees immediately (the sleep-cycle discovery:
                // the 120 s timeout kept a completed trip occupying one
                // of the 16 walk slots, and the "already walking"
                // short-circuit then swallowed the fed mind's next
                // MoveTo for the whole 120 s — no new walk, no second
                // meal, no repeat pair, no bond).
                it = m_Walks.erase(it);
                continue;
            }
            else if (now - session.LastProbe >= std::chrono::seconds(2)
                && (session.LastDistance < 0.0f
                    || std::fabs(d - session.LastDistance) >= 1.0f))
            {
                session.LastProbe = now;
                session.LastDistance = d;
                REX::DEBUG(
                    "LCE: walk probe {} -> {} d = {:.1f} u (min {:.1f} u).",
                    MindLabelForm(actorFormId), MindLabelForm(session.Target), d,
                    session.MinDistance);
            }

            ++it;
        }
    }
}
