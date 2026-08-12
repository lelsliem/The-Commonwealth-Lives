//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   One world, two brains — this is the bridge.                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "Bonds.h"
#include "CoSave.h"
#include "ConflictGates.h"
#include "Kin.h"
#include "Dialogue.h"
#include "Executor.h"
#include "Households.h"
#include "Lifecycle.h"
#include "Market.h"
#include "News.h"
#include "Translator.h"
#include "Tuning.h"
#include "WorldFacts.h"

#include "LCE/Events/EventBus.h"
#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Entity/RegistrySnapshot.h"
#include "LCE/Simulation/Substrate/Rng.h"
#include "LCE/Simulation/Simulation.h"
#include "LCE/Simulation/SimulationEvents.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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
        // a goodbye line, no fact (they chose to go). The quiet flag
        // (the device prune) suppresses the per-mind farewell line —
        // dozens of turrets "leaving" at restore is noise; the prune
        // says it once.
        void RemoveMind(
            std::uint32_t a_formId, bool a_isDeath,
            bool a_quiet = false);

        // The device prune (0.7.2 fix): a polluted co-save holds the
        // workshop's props as minds (they hold the settler faction, and
        // before the exclusion they seeded as Human). A fresh world
        // never seeds them (IsSimRelevant); a restored one self-heals
        // here — quietly, one summary line, before pouches, names, or
        // bonds are rebuilt, so a prop never carries a wallet, a name,
        // or a feud. Actors still streaming in are caught by the census
        // as departures the moment they load.
        void PruneDeviceMinds();

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
            std::vector<TLC::CoSave::StallKeeperPair> a_stallKeepers,
            std::vector<TLC::CoSave::BondPair> a_bonds,
            std::vector<TLC::CoSave::ConflictGatePair> a_gates);

        // The stall-keepers in durable form — (market FormID, keeper
        // FormID) pairs, translated from the session-local entity ids via
        // the translator. The co-save persists this (v3) so a restored
        // market reopens under the same keeper instead of whoever happens
        // to arrive first.
        [[nodiscard]] std::vector<TLC::CoSave::StallKeeperPair>
        StallKeepersForSave() const;

        // The bonds in durable form — (form A, form B, kind, since-day),
        // translated from the session-local entity ids via the
        // translator. The co-save persists this (v5) so a spouse is still
        // a spouse after reload — the bond map is adapter state, never
        // part of the core's snapshot.
        [[nodiscard]] std::vector<TLC::CoSave::BondPair>
        BondsForSave() const;

        // The once-per-day conflict gates in durable form — (form A,
        // form B, row day, fight day), translated from the
        // session-local entity ids via the translator. The co-save
        // persists this (v7) so a feud stays a single scene per day
        // across save/load — the gate map is adapter state, never part
        // of the core's snapshot.
        [[nodiscard]] std::vector<TLC::CoSave::ConflictGatePair>
        ConflictGatesForSave() const;

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

        // 0.7.4 Trade with anyone: the vendor census and its seed. Who
        // sells in the loaded world (SimRelevant::IsVendor — a merchant
        // container), remembered by the minds near them: a hungry human
        // with a seller within walking distance remembers the person at
        // weight 1.05 — a hair above the market seed's 1.0, so a person
        // who sells out-scores the bench while both are fresh — and the
        // arrival then trades with them directly (the existing person-
        // target path). Idempotent: a mind that already remembers a
        // seller it can still see is left alone; the memory fades when
        // the seller leaves, and the refresh re-points at whoever is
        // nearest now. Runs at world start (a_announce) and on the
        // tick's per-second refresh (silent). The bench stays the
        // fallback when no seller is remembered.
        void SeedVendors(bool a_announce);

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

        // When the walk-cap deferral line last printed — the aggregate
        // line is rate-limited so a starved world is visible without
        // flooding the log (the 2026-08-11 lesson: 5.1M per-entity
        // deferral lines in five minutes).
        std::chrono::steady_clock::time_point m_LastCapLog{};

        // The arcs and birth run on a day cadence (0.6.0 Stones 5–6):
        // mediation once per day for each feud, birth at most once per
        // day, each tracked by the day it last ran. Initialized to the
        // sentinel (max) — never a real day — so the first day (0)
        // runs, not skipped (a 0 sentinel would eat day 0). Session
        // state, not persisted: after a restore, one run per current
        // day — correct, since a day may have passed while away.
        std::uint64_t m_LastMediationDay =
            std::numeric_limits<std::uint64_t>::max();
        std::uint64_t m_LastBirthDay =
            std::numeric_limits<std::uint64_t>::max();

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

        // The identity stone's voice (0.7.0 Stone 1): a mind's name with
        // its form id — "Marcy Long [00050976]" — or, for a mind with no
        // name yet, just the form id. The form-id form names a form
        // directly (falling back to the market label for a workshop).
        // MarketLabel names a workshop from its base form when one
        // exists. The log's people are people now, and the hex stays
        // beside the name for the console.
        [[nodiscard]] std::string MindLabel(
            LCE::Simulation::EntityId a_entity) const;
        [[nodiscard]] std::string MindLabelForm(std::uint32_t a_formId) const;
        [[nodiscard]] std::string MarketLabel(std::uint32_t a_formId) const;

        // The identity stone's visible half (0.7.0 Stone 1): write a
        // mind's name onto its actor's extra data (the same mechanism as
        // the console SetDisplayName), so the name shows in-game — the
        // pip-boy, the hover, the workshop — and persists in the save
        // with the actor. Only ever called with a name the sim itself
        // generated; a game-named NPC is never renamed (Sturges stays
        // Sturges). No-op when the actor isn't loaded.
        void ApplyActorName(
            std::uint32_t a_formId, const std::string& a_name) const;

        // The per-second tail of the same stone: a restored world's
        // actors stream in gradually, so the restore-time pass only
        // names the ones already loaded. This sweep names a loaded
        // mind's actor the first time it appears — idempotent (an actor
        // already carrying a display name is skipped), and never touches
        // an actor with no mind. Runs inside the per-second block.
        void ApplyLoadedActorNames();

        // Re-derives a mind's species from its actor's race whenever
        // they disagree (0.7.5 field fix — the behemoth table gap): the
        // species is game truth, and a stored tag from before the fix
        // may name an animal Human. An animal never carries a pouch.
        void ReclassifyLoadedMinds();

        // Rebuilds m_Kin from the loaded actors' base forms (0.7.5
        // field fix): the vanilla families never romance. Derived, never
        // persisted (like the settlement groups) — a kin pair only
        // matters when both are loaded, and the bond gates read this
        // set each second, so a pre-fix save's mistake heals the moment
        // both actors are in.
        void RebuildKin();

        // The test hook's brawl loop (0.7.5): when sim.test.forceFight
        // pins a pair, this fires their fight every ForceFightInterval
        // seconds — the pair pinned to an enemy feud, the once-per-day
        // gate bypassed, the aggressor alternating so the shove lands
        // on both sides. Test-only: off when either form id is 0.
        void ForceFightLoop();

        // The scuffle's second beat (0.7.5): a hot-headed victim
        // answers the punch after a beat — push, fall, get up, push
        // back — instead of both shoves landing in the same instant
        // (which read as a double-fall). Fires due counter-shoves and
        // then walks the one who threw first away from the one who
        // answered (the loser slinks off — the flee's first visible
        // beat while the engine's Flee action is still a stub).
        void ProcessPendingRetaliations();

        // The conflict source's settlement (0.7.0 Stone 2): every mind
        // remembers its market as a Trade-kind event whose Other is the
        // workshop entity — this walks the memories and gives each mind
        // a Groups membership keyed by its market's form id. The engine's
        // group echo then spreads a slight (or a warmth) through the
        // settlement, and InheritGroupAttitudes gives a newcomer the
        // settlement's cold shoulder before they ever meet the feud's
        // villain. Derived, never persisted: a restored world re-derives
        // from its restored market memories. Idempotent — a mind with a
        // group keeps it.
        void AssignSettlementGroups();

        // The player window (0.7.0 Stone 3): one line of world news — a
        // bond, a feud, a birth, a death, the market's hours — appended
        // to the feed (the settlement radio's story) and shown on-screen
        // as a HUD notification, throttled by sim.news.cooldown so a
        // flood of lines is not a flood of windows. The caller's own
        // log line stays the verify channel; this is the human window.
        void PushNews(const std::string& a_line);

        // The settlement radio (0.7.0 Stone 3): while a radio the player
        // built is near, the news feed plays as on-screen captions on the
        // radio cadence. Runs in the per-second pass.
        void RadioCaptions();

        // Who runs each market's stall this world (market entity →
        // stall-keeper mind). Set by the first human arrival at that
        // market; every later bench-arrival trades with them. Persisted
        // in the co-save (v3, as FormID pairs) and rebuilt on restore —
        // a market reopens under the same keeper, not whoever arrives
        // first. Cleared on EndWorld.
        std::unordered_map<
            LCE::Simulation::EntityId, LCE::Simulation::EntityId>
            m_StallKeepers;

        // The bond book (0.6.0 Stone 2): every bonded pair, by entity
        // ids, with its kind and the day it formed. Adapter state — the
        // core holds dispositions, the bond is the named state derived
        // from them (Bonds.h). Two channels feed it: the
        // RelationshipChangedEvent (instant — the core crossed a line
        // mid-mutation) and the 1-second ReconcileBonds pass (the net —
        // drift is quiet in the core, so dissolves only the pass can
        // see). Persisted in the co-save (v5) and restored by form ids;
        // cleared on EndWorld.
        Bonds::BondMap m_Bonds;

        // The once-per-day conflict gates (0.7.5 fix): the last world
        // day each pair had words and came to blows, by entity ids.
        // Adapter state — the core's fading memories cannot gate
        // "today" (a weight-1.0 event erases itself in seconds), so the
        // day-scoped truth lives here. Persisted in the co-save (v7)
        // and restored by form ids; cleared on EndWorld.
        ConflictGates::Map m_ConflictGates;

        // The family gate (0.7.5 field find): every entity pair the
        // world knows is kin — the vanilla families' parent-child lines
        // (Kin.h), discovered from the loaded actors' base forms.
        // Adapter state, derived each second, never persisted; the
        // bond gates refuse a romantic bond for a pair in this set.
        Kin::KinSet m_Kin;

        // The reverse index behind RebuildKin: base form id → the minds
        // whose actors carry it. Rebuilt alongside m_Kin each second.
        std::unordered_map<
            std::uint32_t, std::vector<LCE::Simulation::EntityId>>
            m_BaseToMinds;

        // The typed bond lines (Bonds.h), parsed from the core's
        // watch-list once at tuning load — the same values the core is
        // watching, so the events and the derivation cannot disagree.
        Bonds::BondThresholds m_BondThresholds;

        // The observation bus (Request A — stone 08): the core's
        // events — RelationshipChanged and friends — published into the
        // adapter's handlers. One bus for the adapter's lifetime,
        // subscribed once in the constructor, passed to every Update /
        // Remember / ReportOutcome so the sim's changes flow out.
        LCE::Events::EventBus m_Bus;

        // The event channel: the core crossed a bond line — re-derive
        // that pair now, the same rule the 1-second pass applies.
        void OnRelationshipChanged(
            const LCE::Simulation::RelationshipChangedEvent& a_event);

        // The 1-second pass (the dissolve net): re-derives every pair
        // from the live relationships and logs changes. The events are
        // instant; this is complete — quiet drift, restores, and
        // anything the bus missed all surface here.
        void ReconcileBonds();

        // The arcs' day work (0.6.0 Stone 5): one mediation attempt per
        // enemy pair the settlement knows, once per day — the settlement
        // pulls its own feuds apart (Arcs::Mediate). Pure logic in
        // Arcs.h; this is the edge: collect the enemy pairs from the
        // bond book, run the mediation, log the attempts. The day gate
        // lives here; the work itself is AttemptMediation, shared with
        // the feud-start attempt (0.7.0 Stone 2 — a feud is mediated
        // while the settlement still knows it).
        void RunMediation();

        // One mediation pass over the enemy pairs in the bond book — the
        // body RunMediation gates on the day. Called directly at feud
        // formation too (OnBondChange): gossip fades in seconds at
        // sim.memory.fade 0.2/s, so the day pass alone can never find a
        // mediator — the feud must be mediated while the news is fresh.
        void AttemptMediation();

        // The birth stone's day work (0.6.0 Stone 6, experimental): at
        // most once per day, a spouse household has a child — a sim-only
        // mind with no game actor (Birth::Create), fed by the
        // household. The edge: pick the household, create the child, log
        // the birth. Gated by sim.birth.enabled (default 0).
        void RunBirth();

        // One pair's disposition in a given direction, 0 when unknown.
        float DispositionOf(
            LCE::Simulation::EntityId a_from,
            LCE::Simulation::EntityId a_to);

        // Rebuilds m_Bonds from durable (form A, form B, kind, since)
        // pairs after a restore — both forms resolve via the rebuilt
        // translator (their FormRefs rode the snapshot), so this works
        // even for actors not yet loaded. The 1-second reconcile pass
        // then re-derives: a bond whose relationship drifted below its
        // dissolve line dissolves (honest — the world moved while the
        // game was away); everything else stands.        // Rebuilds m_ConflictGates from durable (form A, form B, row
        // day, fight day) pairs after a restore — both entities resolve
        // via the rebuilt translator (their FormRefs rode the snapshot),
        // so the gate survives for every pair that still lives.
        void RestoreConflictGates(
            const std::vector<TLC::CoSave::ConflictGatePair>& a_gates);

        void RestoreBonds(
const std::vector<TLC::CoSave::BondPair>& a_bonds);

        // One bond change, in the world's voice (0.6.0 Stone 2/3): the
        // log line — "settler X and settler Y became friends." /
        // "settler X is feuding with settler Y." — plus the household
        // reaction (Stone 3): the moment a pair becomes spouses their
        // pouches merge into one shared wallet; when the marriage
        // dissolves, the wallet splits. Shared by the event channel and
        // the reconcile pass — whichever detects the change first says
        // it once.
        void OnBondChange(
            LCE::Simulation::EntityId a_entityA,
            LCE::Simulation::EntityId a_entityB,
            Bonds::BondKind a_old,
            Bonds::BondKind a_new,
            std::uint64_t a_sinceDay);

        // The sim-only population (0.6.0 Stone 6): entities with a
        // SpeciesTag of Child and no FormRef — children born to spouse
        // households, with no game actor. Counted at wake and after a
        // restore so the world's log is honest: the census counts
        // actors, the children are minds that only the log can show.
        [[nodiscard]] std::size_t CountSimOnlyChildren();

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

        // The last thing an entity was logged as doing, and when — so
        // intents are logged when they change, or at most once per
        // LogDecisionEvery seconds while a mind flip-flops between
        // near-tied intents (the Rest/Explore re-roll). Both halves matter:
        // the key alone let a per-frame alternation write 22k lines in
        // under three minutes — synchronous file I/O on the game thread.
        std::unordered_map<
            LCE::Simulation::EntityId,
            std::pair<LogKey, std::chrono::steady_clock::time_point>>
            m_LastLogged;
        std::unordered_map<LCE::Simulation::EntityId, WalkSession> m_Walks;

        // When each mind last arrived, and where (target form id + time).
        // The arrival-cooldown guard: a mind that just arrived at its
        // destination has its next MoveTo there treated as satisfied, so
        // a fed mind standing at its market cannot loop MoveTo → instant
        // arrival → feed every frame (the 0.3/s hunger test: 18k
        // animal-fed lines in under a minute — at fast decay a full mind
        // is always most-urgent-hungry).
        std::unordered_map<
            LCE::Simulation::EntityId,
            std::pair<std::uint32_t, std::chrono::steady_clock::time_point>>
            m_ArrivedAt;

        // Who walked to each bench today (0.7.2 Rows): market FormID →
        // (mind, day) — the crossing scan's attendance book. Ephemeral,
        // pruned to the current day, cleared on world reset; the
        // co-save never touches it. The row's once-a-day gate is the
        // Wronged memory (co-saved), so save/load never double-rows.
        std::unordered_map<
            std::uint32_t,
            std::vector<std::pair<
                LCE::Simulation::EntityId, std::uint64_t>>>
            m_MarketAttendance;

        // When each mind was last commanded to wander (the meal-cadence
        // stone: Rest/Explore execute as a bounded wander near home — a
        // real nearby reference — so a fed mind mills around its
        // settlement instead of freezing at the bench or being drifted
        // away by the sandbox). Rate-limited to one wander per mind per
        // cooldown — a command package every frame would be a flood of
        // its own, and re-issuing mid-walk would yank the actor.
        std::unordered_map<
            LCE::Simulation::EntityId,
            std::chrono::steady_clock::time_point>
            m_LastWander;

        // The world's names (0.7.0 Stone 1): every name this world has
        // assigned — procedural names drawn from the lists and game names
        // from the actors — so no two minds share one. Rebuilt at world
        // start and restore from the live Name components (a restored
        // world keeps its names and reserves them against new arrivals);
        // cleared on EndWorld with everything else.
        std::unordered_set<std::string> m_UsedNames;

        // The author's name lists (0.7.0 Stone 1): gender-split first
        // names for people, a separate pool for animals, and the shared
        // family names — overridable in the INI (names.* keys), defaults
        // otherwise. Built once at tuning load, before any world.
        Names::NamePool m_Names;

        // The author's dialogue pools (0.7.1 Talk): one-liners for every
        // situation life throws at a mind — the good (greet, gossip,
        // family), the bad (trade, row), the ugly (grief, fight, feud) —
        // overridable in the INI (dialogue.* keys), defaults otherwise.
        // Built once at tuning load, like the names.
        Dialogue::DialoguePool m_Dialogue;

        // Says one line from a pool: picks a line for the mind and day,
        // logs it as speech, and pushes it to the news feed so the
        // settlement radio reads it as a caption. Speech is a
        // presentation layer — the sim decides *when*, this decides
        // *what*, and silence is a safe default (an empty pool says
        // nothing).
        void Say(
            LCE::Simulation::EntityId a_speaker,
            LCE::Simulation::EntityId a_listener,
            Dialogue::Pool a_pool);

        // The physical escalation (0.7.5 Fights): when a feud's words
        // fail — an enemy pair's row, or a slighted mind facing an
        // enemy keeper — the temper and chance rolls decide whether
        // blows land. Books the Combat on both sides (the feud deepens;
        // the victim carries a threat and may flee), says the fight
        // line, and carries the news.
        void EscalateToFight(
            LCE::Simulation::EntityId a_aggressor,
            LCE::Simulation::EntityId a_victim,
            std::uint64_t a_day,
            bool a_force = false);

        // The news feed (0.7.0 Stone 3): the world's paper, capped and
        // rotated — the settlement radio reads it as captions. Session
        // state (the log file keeps the full record); cleared on
        // EndWorld.
        News::NewsFeed m_News;

        // On-screen news throttle and the radio caption cadence — when
        // each last fired, so a world of news does not flood the player.
        std::chrono::steady_clock::time_point m_LastNews{};
        std::chrono::steady_clock::time_point m_LastRadioCaption{};

        // The recent deaths (the grief announce, 0.6.0 Stone 5): entity
        // value → form id, set when a death is booked and the dead is
        // removed from the translator (FormFor can no longer answer).
        // The grief announce needs the dead's form to say who is
        // mourned — without this the line was dead code, FormFor(dead)
        // always 0. Session state, cleared on EndWorld; keys carry the
        // dead's full id value (generation included), so a recycled
        // registry slot never collides.
        std::unordered_map<std::uint64_t, std::uint32_t> m_RecentDeaths;

        // Which (mind, dead) pairs already got their grief line this
        // session — the announce fires on the fresh crossing (weight ≥
        // 0.9, ~0.5 s of frames), once per bereavement, not every frame
        // (the 2026-08-11 flood: 34 lines in half a second). Bounded by
        // the session's deaths; cleared on EndWorld, so a restored
        // bereavement re-announces once — honest, the line is cheap.
        std::set<std::pair<std::uint64_t, std::uint64_t>> m_GriefAnnounced;

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

        // The bonds the co-save held for this save, riding with
        // m_PendingRestore; consumed by ApplyRestore (which translates
        // the FormID pairs back into this world's entity ids).
        std::vector<TLC::CoSave::BondPair> m_PendingBonds;

        // The once-per-day conflict gates the co-save held for this
        // save (v7), riding with m_PendingRestore; consumed by
        // ApplyRestore (which translates the FormID pairs back into this
        // world's entity ids).
        std::vector<TLC::CoSave::ConflictGatePair> m_PendingGates;

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

        // The test hook's brawl loop (0.7.5): when to fire the next
        // forced fight, and which side threw last (the aggressor
        // alternates so the shove lands on both). Session state;
        // cleared with the world.
        std::chrono::steady_clock::time_point m_LastForceFight{};
        std::uint64_t m_ForceFightCount = 0;

        // The scuffle's queued second beat (0.7.5): who answers whom,
        // and when — the counter-shove fires a beat after the first
        // punch so the exchange reads as a sequence, not a double-fall.
        struct PendingRetaliation
        {
            LCE::Simulation::EntityId Retaliator;
            LCE::Simulation::EntityId Target;
            std::chrono::steady_clock::time_point Due;
        };

        std::vector<PendingRetaliation> m_PendingRetaliations;

        // Whether the current load's completion event was already handled.
        // F4SE can fire both kPostLoadGame and kGameLoaded for one load —
        // the first applies the restore, the second must not wipe it.
        // Reset by PreLoadGame; the startup wake (no preceding load) also
        // starts false and is handled once.
        bool m_LoadCompleted = false;
    };
}
