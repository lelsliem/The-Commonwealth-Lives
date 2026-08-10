═══════════════════════════════════════════════

The Living Commonwealth — Fallout 4 Adapter

Building living worlds through simulation.

═══════════════════════════════════════════════

Project Roadmap

═══════════════════════════════════════════════

Status

Current Version : 0.2.0-alpha

Current Stage   : Intent Executor — implemented; in-game verification pending

Next Milestone  : 0.4.0 — Co-save

═══════════════════════════════════════════════

0.1.0 — Scaffold · "The First Heartbeat"

═══════════════════════════════════════════════

Goal: a plugin that loads in Fallout 4 and breathes.

STATUS: COMPLETE ✅ (verified in-game 2026-08-09)

[✓] git init + .gitignore (core conventions)
[✓] xmake scaffold — CommonLibF4 from the local clones, LCE.Core
    driven by its own CMake via the lce.core rule, static CRT,
    one spdlog in the DLL (version + std::format aligned)
[✓] F4SEPlugin_Load heartbeat — loads, listens, logs through
    LCE's API and REX::LOG
[✓] Docs: README, decision log (7 ADRs), Depends provenance

Proof: F4SE 0.7.8 loads the plugin on runtime 1.11.221; GameLoaded
fires; the heartbeat lands in the F4SE log.

═══════════════════════════════════════════════

0.2.0 — Translation · "The Commonwealth Wakes Up"

═══════════════════════════════════════════════

Goal: settlers become minds — entities with needs, memory, and
relationships inside the core's registry.

STATUS: COMPLETE ✅ (verified in-game 2026-08-09)

[✓] Adapter world object — registry + translator, lifecycle wired
    to GameLoaded / PreLoadGame / DeleteGame
[✓] Settler predicate — WorkshopNPCFaction (0x000337F3, extracted
    from the game's Fallout4.esm), player excluded
[✓] Seeded minds — FormRef + satisfied Needs + empty Memory and
    Relationships; read-once, never write (ADR-0024)
[✓] Serializers for every persisted type, registered once at init —
    the core's 0.4.0 snapshot substrate's first real use
[✓] Adapter test harness — 3/3 green, no game required

Proof: "The Commonwealth wakes up: 10 settlers became minds." at
Sanctuary.

═══════════════════════════════════════════════

0.3.0 — Intent Executor · "The Farmer Walks"

═══════════════════════════════════════════════

Goal: the simulation ticks in-game, and intents become game actions.

STATUS: IMPLEMENTED — tick verified in-game; walking call pending

[✓] Design — Docs/Design/Executor.md
[✓] Per-frame tick — a hook on the game's frame driver (0x00C30C0A,
    inside the 5KB driver 0x00C2FD12), once per frame on the game
    thread, real delta. Verified in-game 2026-08-10: fires at 60fps.
    (The original target, ProcessVMTick ID 2251368, proved
    event-driven and was pruned.)
[✓] Plan builder — pure and tested (PlanBuilderTest, 4/4 suites);
    refusals are the contract: unloaded actor, unloaded target, busy
    actor → dropped, re-decided next tick; targetless intents are never
    refused for a target
[✓] Action table — MoveTo through the Movement::WalkTo seam (never
    teleport); Rest/Socialize/Explore/Work/Flee → table slots + log
    lines
[ ] Walking call — the AIProcess::CreateMovementPlanner RVA for
    1.11.221 is pending in-game verification (one constant in
    src/Movement.cpp); until then WalkTo refuses, never teleports
[ ] Walking in-game — a MoveTo executes and a settler walks to market;
    the intent lines themselves are verified (all 11 settlers at
    Sanctuary logged decides Explore)

═══════════════════════════════════════════════

0.4.0 — Co-save · "The World Remembers"

═══════════════════════════════════════════════

Goal: the simulation rides inside the game's save files.

[ ] F4SE serialization records — the adapter's stable type names and
    versioning over the core's Capture/Restore substrate (proven)
[ ] Lifecycle — PreSaveGame → Capture → record; PostLoadGame →
    record → Restore; PreLoadGame/DeleteGame → Clear
[ ] Migration — old saves load forward

═══════════════════════════════════════════════

0.5.0 — Living World · "The Settler Goes to Market"

═══════════════════════════════════════════════

Goal: the real test from the contract.

[ ] World facts — Remember pushes (weather, market open/closed)
[ ] Tuning from the Configuration service
[ ] The real test: a settler goes to market because they are hungry —
    no script
[ ] Nexus name check + publish
