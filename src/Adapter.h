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
    //   GameLoaded   -> StartWorld : settlers become minds
    //   PreLoadGame  -> EndWorld   : the world ends before the next loads
    //   DeleteGame   -> EndWorld   : and when a save is deleted
    //
    // Serializers are registered in the constructor — once, before any
    // world exists (core 0.4.0 contract: they survive Clear()).
    //-------------------------------------------------------------------------
    class Adapter
    {
    public:
        Adapter();

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
        // each walk issued once. It ends when the mind decides something
        // else, the walk is refused, or 30s pass (a stuck walk re-issues).
        struct WalkSession
        {
            std::uint32_t Target = 0;
            std::chrono::steady_clock::time_point Issued{};
        };

        void ExecutePlan(const std::vector<PlanEntry>& a_plan);
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
        std::unordered_map<LCE::Simulation::EntityId, LogKey> m_LastLogged;
        std::unordered_map<LCE::Simulation::EntityId, WalkSession> m_Walks;
    };
}
