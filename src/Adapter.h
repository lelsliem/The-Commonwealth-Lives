//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "Executor.h"
#include "Translator.h"

#include "LCE/Simulation/EntityRegistry.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

namespace TLC
{
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
            // destination as the settler moves (≥1 m of progress, 2s
            // apart — silence means standing still), the closest approach
            // so far, and whether arrival was ever logged. The log, not
            // the player's eyes, proves whether the walk happened.
            std::chrono::steady_clock::time_point LastProbe{};
            float LastDistance = -1.0f;   // -1 = nothing logged yet
            float MinDistance = std::numeric_limits<float>::max();
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

        // The market: the workshop form becomes the market entity when it
        // is loaded, so every mind can remember where to trade.
        void EnsureMarket();

        LCE::Simulation::EntityRegistry m_Registry;
        Translator m_Translator;
        bool m_Started = false;

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

        std::unordered_map<LCE::Simulation::EntityId, LogKey> m_LastLogged;
        std::unordered_map<LCE::Simulation::EntityId, WalkSession> m_Walks;
    };
}
