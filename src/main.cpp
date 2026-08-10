//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE). An F4SE plugin; a client of the core, never a re-implement- //
//   ation of it. The seam is the public API: CreateEntity, DestroyEntity,     //
//   Remember, Update; intents are hints, not commands.                        //
//                                                                             //
//   Please stand by — the Commonwealth is loading.                                      //
//                                                                             //
//=============================================================================//

#include "Adapter.h"
#include "CoSave.h"
#include "Tick.h"

#include <F4SE/F4SE.h>

#include <LCE/Logging/Logger.h>
#include <LCE/Simulation/RegistrySnapshot.h>

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

// The build stamp injected by xmake.lua (the git short hash) — the
// banner's "which DLL ran" identifier. Injected unquoted (a clean
// token) and stringized here, so the compiler never sees escaped
// quotes. Falls back for non-xmake builds.
#ifndef TLC_BUILD_STAMP
#define TLC_BUILD_STAMP unknown
#endif

#define TLC_BUILD_STAMP_STRINGIZE_(x) #x
#define TLC_BUILD_STAMP_STRINGIZE(x) TLC_BUILD_STAMP_STRINGIZE_(x)

namespace
{
    // The plugin's one world object — owned here, by the module, never a
    // reachable global in the core's sense.
    TLC::Adapter g_Adapter;

    //-------------------------------------------------------------------------
    // Co-save (0.4.0) — the simulation rides inside the save file. F4SE
    // calls these during save/load; the adapter and the record codec stay
    // game-free (pure data), and this glue — the only F4SE:: touch in the
    // chain — lives here at the edge.
    //-------------------------------------------------------------------------
    void OnSaveGame(const F4SE::SerializationInterface* a_intfc)
    {
        const auto snapshot = g_Adapter.CaptureWorld();
        const auto stalls = g_Adapter.StallKeepersForSave();
        const auto record =
            TLC::CoSave::Encode(snapshot, g_Adapter.RngState(), stalls);

        REX::INFO(
            "co-save: writing {} entities ({} bytes).",
            snapshot.Entities.size(), record.size());

        if (!a_intfc->WriteRecord(
                TLC::CoSave::kRecordType, TLC::CoSave::kRecordVersion,
                record.data(), static_cast<std::uint32_t>(record.size())))
        {
            REX::ERROR(
                "co-save: WriteRecord failed — the world may not survive this save.");
        }
    }

    void OnLoadGame(const F4SE::SerializationInterface* a_intfc)
    {
        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;

        while (a_intfc->GetNextRecordInfo(type, version, length))
        {
            if (type != TLC::CoSave::kRecordType)
            {
                continue;   // another plugin's record — not ours
            }

            std::vector<std::byte> record(length);

            if (a_intfc->ReadRecordData(record.data(), length) != length)
            {
                REX::ERROR("co-save: record read failed ({} bytes).", length);
                continue;
            }

            LCE::Simulation::RegistrySnapshot snapshot;

            // Pre-seeded with the default stream: a v2 record overwrites
            // it with the saved world's own state; a v1 record leaves it
            // (that world never had a saved stream).
            std::uint64_t rngState = TLC::kRngSeed;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;

            if (!TLC::CoSave::Decode(record, snapshot, rngState, stalls))
            {
                REX::ERROR("co-save: record decode failed (version {}).", version);
                continue;
            }

            REX::INFO(
                "co-save: read {} entities, {} stall-keepers — the world will be restored on load.",
                snapshot.Entities.size(), stalls.size());
            g_Adapter.QueueRestore(std::move(snapshot), rngState, std::move(stalls));
        }
    }

    void OnRevertGame(const F4SE::SerializationInterface*)
    {
        // A new game (or the pre-load reset): the sim starts blank — the
        // next GameLoaded translates fresh. Clear keeps the serializers.
        g_Adapter.EndWorld();
    }

    //-------------------------------------------------------------------------
    // The world is awake: the heartbeat, then the translation — every
    // sim-relevant settler becomes a mind.
    //-------------------------------------------------------------------------
    void OnGameLoaded()
    {
        constexpr std::string_view Heartbeat = "The Living Commonwealth heartbeat: the world is awake.";

        // The visible log (My Games/Fallout4/F4SE/TheLivingCommonwealth.log).
        REX::INFO("{}", Heartbeat);

        // The documented channel: the mod logs through LCE's API, never
        // spdlog directly.
        LCE::Logging::Info(Heartbeat);

        g_Adapter.GameLoaded();
        LCE::Logging::Flush();
    }

    void OnPreLoadGame()
    {
        g_Adapter.PreLoadGame();
    }

    void OnDeleteGame()
    {
        g_Adapter.DeleteGame();
    }
}

//-------------------------------------------------------------------------
// The F4SE entry point. F4SE_PLUGIN_LOAD expands to
// extern "C" __declspec(dllexport) bool F4SEPlugin_Load(const F4SE::LoadInterface*).
// The version data (F4SEPlugin_Version) is generated by the
// commonlibf4.plugin build rule from xmake.lua.
//-------------------------------------------------------------------------
F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_intfc)
{
    // Init wires the F4SE interfaces and the log sink (msvc + file). The
    // log name is kept free of spaces so the file stays clean. The
    // trampoline is where the Tick hooks' branches are allocated — a few
    // dozen bytes is all the four call-site hooks need.
    F4SE::Init(a_intfc, {
        .logName = "TheLivingCommonwealth",
        .trampoline = true,
        .trampolineSize = 128,
    });

    // The simulation's heartbeat: once per frame on the game thread, the
    // adapter decays the world and executes what the minds decided.
    TLC::Tick::Install([](double a_delta) { g_Adapter.Tick(a_delta); });

    // The co-save (0.4.0): SetUniqueID first, then the callbacks. The UID
    // is a placeholder — replace with the F4SE-assigned UID before
    // release (CoSave.h). Revert clears on new game; Save writes the
    // world; Load queues the restore that GameLoaded applies.
    if (const auto* serialization = F4SE::GetSerializationInterface(); serialization)
    {
        serialization->SetUniqueID(TLC::CoSave::kSerializationUid);
        serialization->SetRevertCallback(OnRevertGame);
        serialization->SetSaveCallback(OnSaveGame);
        serialization->SetLoadCallback(OnLoadGame);
    }
    else
    {
        REX::ERROR("The Living Commonwealth: failed to get the serialization interface.");
        return false;
    }

    // The engine's own logging, behind its API (ADR-0030: own the interface,
    // not the implementation).
    LCE::Logging::Initialize();

    if (const auto* messaging = F4SE::GetMessagingInterface(); messaging)
    {
        messaging->RegisterListener([](F4SE::MessagingInterface::Message* a_msg) {
            switch (a_msg->type)
            {
            case F4SE::MessagingInterface::kGameLoaded:
                // kGameLoaded fires at startup (the menu world wakes);
                // kPostLoadGame fires when a save finishes loading —
                // in-game tests showed real loads complete on screen
                // without a kGameLoaded, so the completion event is the
                // one that must apply a co-save restore. Both route to
                // the same handler; the adapter dedupes one load's pair.
            case F4SE::MessagingInterface::kPostLoadGame:
                REX::INFO("lifecycle: GameLoaded — the world wakes.");
                OnGameLoaded();
                break;
            case F4SE::MessagingInterface::kPreLoadGame:
                // kPreLoadGame fires only from BGSSaveLoadGame::LoadGame —
                // a save is actually being loaded. The name tells us which
                // one: in-game sessions showed a load firing ~10s after
                // the world woke, with no GameLoaded following it (the
                // sim died every run). The name is the clue to what
                // triggers it.
                REX::INFO(
                    "lifecycle: PreLoadGame — loading save '{}'.",
                    a_msg->data != nullptr
                        ? static_cast<const char*>(a_msg->data)
                        : "?");
                OnPreLoadGame();
                break;
            case F4SE::MessagingInterface::kDeleteGame:
                REX::INFO(
                    "lifecycle: DeleteGame — deleting save '{}'.",
                    a_msg->data != nullptr
                        ? static_cast<const char*>(a_msg->data)
                        : "?");
                OnDeleteGame();
                break;
            default:
                break;
            }
        });
    }
    else
    {
        REX::ERROR("The Living Commonwealth: failed to get the messaging interface.");
        return false;
    }

    REX::INFO(
        "The Living Commonwealth v{} loaded (build {}).",
        F4SE::GetPluginVersion(), TLC_BUILD_STAMP_STRINGIZE(TLC_BUILD_STAMP));
    return true;
}
