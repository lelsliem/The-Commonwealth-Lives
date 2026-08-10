//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   One world, two brains — this is the bridge.                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "CoSave.h"
#include "Executor.h"
#include "Lifecycle.h"
#include "Market.h"
#include "Translator.h"
#include "Tuning.h"
#include "WorldFacts.h"

#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/RegistrySnapshot.h"
#include "LCE/Simulation/Rng.h"
#include "LCE/Simulation/Simulation.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace RE
{
    class Actor;
    class TESWeather;
}

namespace TLC
{
    //-------------------------------------------------------------------------
    // The world-level randomness seed (the decay-jitter wiring, engine
    // stone 07). A fixed constant: the adapter's own stream starts
    // identically every session, and the co-save's v2 record persists
    // rng.State() so a restored world resumes the exact same stream.
    // (The parent only feeds Derive — per-entity jitter never advances it
    // — so the persisted state matters the moment a world-level draw
    // exists.)
    //-------------------------------------------------------------------------
    inline constexpr std::uint64_t kRngSeed = 0x4C43455700000001ull;   // 'LCEW' + 1

    //-------------------------------------------------------------------------
    // Adapter — the plugin's one world object (owned by main.cpp, not a
    // hidden global). Holds the core's registry and the translator, and
    // maps the game's life events onto the simulation:
    //
    //   GameLoaded  -> EndWorld + StartWorld : every completed load is a
    //                   fresh world (settlers become minds)
    //   PreLoadGame -> EndWorld, and arm the abort recovery: the game
    //                   sometimes starts a load that never completes (the
    //                   exit-save reload aborts ~9s in, every session) —
    //                   if no GameLoaded follows within 12s, the world
    //                   revives itself in Tick
    //   DeleteGame  -> EndWorld, no recovery (nothing is loading)
    //
    // Serializers are registered in the constructor — once, before any
    // world exists (core 0.4.0 contract: they survive Clear()).
    //-------------------------------------------------------------------------
    class Adapter
    {
    public:
        Adapter();

        void GameLoaded();
        void PreLoadGame();
        void DeleteGame();
        void StartWorld();
        void EndWorld();

        // 0.6.0 Stone 1 — the world keeps its books. The per-second
        // edge read: new loaded settlers become minds (arrivals), a
        // known mind whose actor is dead is removed with a death fact
        // (every survivor remembers who is gone), and a known mind whose
        // actor left the settler faction is removed with a goodbye.
        void KeepBooks();

        // Creates a mind for a loaded, sim-relevant actor: the entity,
        // the species split, seeded needs (with the per-mind desync),
        // goals, empty memory and relationships, and the human's pouch.
        // Silent — the caller logs; idempotent (an already-known form is
        // skipped). The wake seed and the bookkeeping arrival share it.
        void SeedMind(const RE::Actor* a_actor);

        // The wake seed (StartWorld): every loaded sim-relevant actor
        // becomes a mind. Returns the count the banner logs.
        std::size_t SeedLoadedActors();

        // Removes a mind and its book entries: the walk session, the
        // last-log key, the feeder line, a stall the mind kept (the
        // market re-derives its keeper on the next arrival), the entity,
        // and the translation. A death (a_isDeath) writes the death fact
        // — every surviving mind remembers { the dead, Death, weight,
        // day } — the groundwork grief reads in Stone 2; a departure is
        // a goodbye line, no fact (they chose to go).
        void RemoveMind(std::uint32_t a_formId, bool a_isDeath);

        // The world day (the weather stone reads it inline; this is the
        // same read, named for the bookkeeping). 0 before the calendar
        // is up.
        [[nodiscard]] std::uint64_t CurrentDay() const;

        // Co-save (0.4.0) — the world rides inside the save file. The
        // F4SE serialization callbacks (main.cpp) call these; the adapter
        // stays game-free, so they are testable without the game.
        //
        //   PreSaveGame -> CaptureWorld(): the whole registry as data.
        //   load        -> QueueRestore(snapshot): stash what the co-save
        //                  held; GameLoaded applies it (or starts fresh
        //                  when none is pending).
        [[nodiscard]] LCE::Simulation::RegistrySnapshot CaptureWorld() const;

        // The Rng's whole state — one number the co-save persists (v2) so
        // a restored world resumes the same randomness (engine Seeded RNG
        // contract).
        [[nodiscard]] std::uint64_t RngState() const noexcept;

        void QueueRestore(
            LCE::Simulation::RegistrySnapshot a_snapshot,
            std::uint64_t a_rngState,
            std::vector<TLC::CoSave::StallKeeperPair> a_stallKeepers);

        // The stall-keepers in durable form — (market FormID, keeper
        // FormID) pairs, translated from the session-local entity ids via
        // the translator. The co-save persists this (v3) so a restored
        // market reopens under the same keeper instead of whoever happens
        // to arrive first.
        [[nodiscard]] std::vector<TLC::CoSave::StallKeeperPair>
        StallKeepersForSave() const;

        // The per-frame heartbeat of the simulation: decay, remember,
        // decide, then execute. Called by the Tick hook on the game thread.
        void Tick(double a_deltaSeconds);

    private:
        // What an entity last did — so intents are logged when they change,
        // not every frame.
        struct LogKey
        {
            std::uint32_t Action = 0;
            std::uint32_t Target = 0;
            std::uint8_t Reason = 0;
            bool operator==(const LogKey&) const = default;
        };

        // A walk in progress: which target the entity was told to walk to,
        // and when. The Trade memory outlives one tick, so a MoveTo intent
        // would otherwise re-issue the walk every frame; the session makes
        // each walk issued once. It outlives the intent — ProbeWalks keeps
        // measuring distance (and logs arrival) until the walk is refused,
        // arrival is logged, or 60s pass (a stuck walk then re-issues).
        struct WalkSession
        {
            std::uint32_t Target = 0;
            std::chrono::steady_clock::time_point Issued{};

            // The walk probe (verification instrument): distance to the
            // destination (in game units) as the settler moves (≥1 u of
            // progress, 2s apart — silence means standing still), the
            // closest approach so far, and whether arrival was ever
            // logged. The log, not the player's eyes, proves whether the
            // walk happened.
            std::chrono::steady_clock::time_point LastProbe{};
            float LastDistance = -1.0f;   // -1 = nothing logged yet
            float MinDistance = std::numeric_limits<float>::max();

            // The distance at the first probe (units) — the sanity gate
            // for the probe: a reading absurdly far beyond it is a
            // stream artifact (cell teardown after a fast travel), not
            // progress, and is skipped.
            float StartDistance = 0.0f;

            bool Reached = false;
        };

        void ExecutePlan(const std::vector<PlanEntry>& a_plan);

        // Measures every active walk session as the settler moves: distance
        // to the target (≥1 m of progress, 2s apart), closest approach, and
        // a one-time "reached" line. Silence means the settler isn't moving
        // — the sandbox-override verdict. Runs independent of the current
        // intent, because the Trade memory fades (~4.5s) long before the
        // walk completes.
        void ProbeWalks();

        void LogPlanEntry(
            LCE::Simulation::EntityId a_entity,
            std::string a_message,
            const LogKey& a_key);

        // Tuning (0.5.0): one text file next to the DLL (the core's
        // Configuration service + the adapter's own keys — market hours).
        // Loaded once, before any world; the file is the modder's knob,
        // never written by the plugin.
        void LoadConfiguration();

        LCE::Simulation::SimulationTuning m_CoreTuning;
        Tuning::AdapterSettings m_Settings;

        // The settlement census (per-settlement markets, 0.5.x): one
        // pass over the game's REFR form array, keeping every placed ref
        // whose base is the vanilla workshop workbench (000C1AEB). Each
        // is a settlement's market, with a world position the seed can
        // reason over. Static per load order, so the scan runs once (on
        // the first seed) and the list survives worlds.
        void RefreshWorkshops();

        // A workshop form becomes a market entity (a FormRef only — a
        // target, never a mind), so minds can remember it as where to
        // trade and the walk can resolve its position. Idempotent per
        // form; the census calls it for every known workshop, and the
        // legacy pin path calls it for the fallback market.
        void EnsureWorkshop(std::uint32_t a_formId);

        // The market is open: the census, then every mind remembers where
        // its own settlement trades — the nearest workshop within range
        // of where it stands, not a single global bench. Runs on every
        // world start — fresh translations and co-save restores alike,
        // because the seed is a fading memory event and a saved mind may
        // have forgotten it. a_announce logs the "market is open" line;
        // the tick's periodic refresh passes false (idempotent, silent).
        void SeedMarket(bool a_announce);

        // Rebuilds the world from a co-save snapshot: Restore the registry
        // (identities preserved), rebuild the translator from the restored
        // FormRef components (the edge's memory is adapter state, never
        // part of the snapshot), re-seed the market, and resume.
        void ApplyRestore(LCE::Simulation::RegistrySnapshot a_snapshot);

        LCE::Simulation::EntityRegistry m_Registry;
        Translator m_Translator;
        bool m_Started = false;

        // The census result: every known settlement market, by form and
        // world position. m_WorkshopsReady is set only when a non-empty
        // list is found (static per load order) — an empty result is
        // retried on the seed cycle, throttled by m_LastCensus, because
        // the REFR array may not be populated when the world wakes. The
        // legacy single-bench fallback covers the world meanwhile.
        std::vector<WorkshopPosition> m_Workshops;
        bool m_WorkshopsReady = false;
        std::chrono::steady_clock::time_point m_LastCensus{};

        // The one-time census diagnostic: the first base forms actually
        // in the REFR array, so a 0-result census says what the filter
        // saw (empty array vs. a different workbench base) instead of
        // just that it saw nothing.
        bool m_CensusDiagnosed = false;

        // Tuning is loaded once per session, from GameLoaded (the
        // constructor runs before the logger attaches, so its
        // confirmation lines would be dropped).
        bool m_TuningLoaded = false;

        // First-pass instrumentation (the walking stone's verification):
        // one-time lines that prove the tick hook fires and that a full
        // pass (Update → plan → execute → probe) completed.
        bool m_TickCalled = false;
        bool m_FirstPassLogged = false;

        // Abort recovery: armed by PreLoadGame, cleared by GameLoaded or
        // the recovery itself. The game's exit-save reload starts a load
        // ~0.1s after the world wakes and aborts it ~9s later without a
        // GameLoaded — without this, the sim dies every session.
        bool m_AwaitingLoad = false;
        std::chrono::steady_clock::time_point m_WorldEndedAt{};

        // The market fact is re-pushed every second while the world runs
        // (idempotent — SeedMarketMemory skips minds that remember) so
        // minds whose actors load after the world starts — a restore
        // brings 637 entities back, but their actors load gradually —
        // still learn where to trade.
        std::chrono::steady_clock::time_point m_LastMarketSeed{};

        // World facts (0.5.0): the doors the world shuts. The market
        // closes outside its trading hours — a remembered { invalid,
        // Trade } fact blocks the hungry walk until it fades — and a
        // radstorm shuts the gatherings ( { invalid, Social } ). The
        // adapter reads the game clock and the sky once per second,
        // pushes the active facts (refresh-in-place so they never die
        // while the door is shut), and logs on transition only. Pure
        // logic lives in WorldFacts.h; this is the edge read.
        void PushWorldFacts();

        // Whether the radstorm fact is active — a FormID table in
        // Adapter.cpp, pinned from the xEdit weather dump 2026-08-10:
        // only CommonwealthGSRadstorm (001C3D5E) matches (see
        // Docs/WeatherForms.md for the exclusion decisions).
        bool IsRadstorm(const RE::TESWeather* a_weather) const;

        bool m_MarketClosed = false;
        bool m_Radstorm = false;

        // Weather memory (0.5.x): the day's sky. m_Weather is the current
        // classification (transition logs); m_WeatherDay/m_WeatherSeen
        // track which categories today's sky has shown, so "it rained
        // this morning" stays remembered until the world turns. Session
        // state, never persisted — weather is re-derived from the live
        // sky on every start.
        WorldFacts::WeatherKind m_Weather =
            WorldFacts::WeatherKind::Unknown;
        std::uint64_t m_WeatherDay = 0;
        std::uint8_t m_WeatherSeen = 0;

        // The animal's feeder: its owner when the game assigns one and
        // the owner is a sim entity, else the settlement. Resolved per
        // mind at seed time (the 0.5.0 food-source resolver).
        LCE::Simulation::EntityId OwnerEntityFor(
            LCE::Simulation::EntityId a_entity);

        // A walk reached its food source: report the per-species outcome.
        // A human trades for real when a trader resolves (the stall-keeper
        // of the market they reached, or the person the walk itself
        // resolved to — the remembered merchant); Trade, Success, the
        // exchange lands. The first human at a market sets up its stall
        // (Trade, Partial — no customers yet). A child or animal is fed —
        // Aid, Success, nothing in return.
        void ReportArrival(
            LCE::Simulation::EntityId a_entity, std::uint32_t a_targetFormId);

        // Who runs each market's stall this world (market entity →
        // stall-keeper mind). Set by the first human arrival at that
        // market; every later bench-arrival trades with them. Persisted
        // in the co-save (v3, as FormID pairs) and rebuilt on restore —
        // a market reopens under the same keeper, not whoever arrives
        // first. Cleared on EndWorld.
        std::unordered_map<
            LCE::Simulation::EntityId, LCE::Simulation::EntityId>
            m_StallKeepers;

        // Which animals already got their feeder announced this world
        // (one line per animal, cleared on EndWorld).
        std::unordered_set<LCE::Simulation::EntityId> m_FeederLogged;

        // Two-pass death confirmation (Stone 1 hardening): a form id read
        // dead by the census is parked here; if the very next pass still
        // reads it dead, the death is real and booked. Actors streaming
        // in after a load can read as dead once (garbage members — the
        // 3D gate is not enough; two persistent Sanctuary settlers
        // "died" on both the first and second in-game runs, same 3s
        // window, nobody near them), and a transient read must never
        // book a death. Cleared the moment the actor reads alive or
        // stops being scanned, and on EndWorld.
        std::unordered_map<
            std::uint32_t, std::chrono::steady_clock::time_point>
            m_PendingDeaths;

        // Seen-alive (Stone 1 hardening, round two): form ids that have
        // read ALIVE at least once since seeding. A death is a transition
        // — alive, then dead — so a mind must be seen alive before it can
        // be booked dead. The spawn burst after a big load reads certain
        // actors dead on their very FIRST sighting for ~2s (deterministic:
        // the same two persistent Sanctuary settlers every time, three
        // builds in a row), and a corpse or artifact that was never seen
        // alive must never book. Parked forever until the actor reads
        // alive; cleared on EndWorld.
        std::unordered_set<std::uint32_t> m_SeenAlive;

        std::unordered_map<LCE::Simulation::EntityId, LogKey> m_LastLogged;
        std::unordered_map<LCE::Simulation::EntityId, WalkSession> m_Walks;

        // The world the co-save held for this save. Set by QueueRestore
        // during the load, consumed by GameLoaded; absent or empty means
        // this session starts fresh (a new game, or a save made while the
        // sim was not running). m_PendingRngState rides with it: the
        // stream the saved world was using, resumed on ApplyRestore.
        std::optional<LCE::Simulation::RegistrySnapshot> m_PendingRestore;
        std::uint64_t m_PendingRngState = kRngSeed;

        // The stall-keepers the co-save held for this save, riding with
        // m_PendingRestore; consumed by ApplyRestore (which translates
        // the FormID pairs back into this world's entity ids).
        std::vector<TLC::CoSave::StallKeeperPair> m_PendingStallKeepers;

        // Rebuilds m_StallKeepers from durable (market, keeper) FormID
        // pairs after a restore — the market entity and the keeper entity
        // both resolve via the rebuilt translator (their FormRefs rode
        // the snapshot), so this works even for actors not yet loaded.
        void RestoreStallKeepers(
            const std::vector<TLC::CoSave::StallKeeperPair>& a_stallKeepers);

        // The world-level randomness (engine stone 07): passed to Update
        // so every entity's needs decay at its own per-tick rate
        // (DecayRate * Derive(id).NextFloat(1 ± sim.jitter)). Session
        // state — never reset by EndWorld; the co-save resumes it.
        LCE::Simulation::Rng m_Rng{ kRngSeed };

        // Whether the current load's completion event was already handled.
        // F4SE can fire both kPostLoadGame and kGameLoaded for one load —
        // the first applies the restore, the second must not wipe it.
        // Reset by PreLoadGame; the startup wake (no preceding load) also
        // starts false and is handled once.
        bool m_LoadCompleted = false;
    };
}
