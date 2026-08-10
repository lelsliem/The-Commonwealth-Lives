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

STATUS: IMPLEMENTED — tick and walking verified in-game (2026-08-10)

[✓] Design — Docs/Design/Executor.md, Docs/Design/Walking.md
[✓] Per-frame tick — a hook on ProcessVMTick (ID 2251368): two of its
    four call sites (0x010E9F7E, 0x010EA08E), once per frame on the
    game thread, real delta. Verified in-game 2026-08-10 by per-hook
    fire counters: both sites fired once per frame for 12,600+ frames;
    the other two sites never fired and a driver-site detour fired
    once at startup — all pruned
[✓] Plan builder — pure and tested (PlanBuilderTest, 4/4 suites);
    refusals are the contract: unloaded actor, unloaded target, busy
    actor → dropped, re-decided next tick; targetless intents are never
    refused for a target
[✓] Action table — MoveTo through the Movement::WalkTo seam (never
    teleport); Rest/Socialize/Explore/Work/Flee → table slots + log
    lines
[✓] Walking call — pinned statically against Fallout4.exe 1.11.221 by
    RTTI chain: the movement controller's DoSetPlannerDirectControl
    (NPC subobject vtable 0x2567B68, slot [2] = 0xdc92f0, this =
    controller+0x138), guarded by a runtime vtable check that refuses
    rather than teleport; WalkTo no longer refuses by default
    (src/Movement.cpp, Docs/Design/Walking.md)
[✓] The market — every mind remembers where to trade: the Sanctuary
    workshop (REFR 000250FE, verified from the ESM) becomes the market
    entity when loaded,
    and the Trade seed makes hungry settlers decide MoveTo
    (MarketTest, 5/5 suites green)
[✓] Walking in-game — a MoveTo executes and a settler walks to market;
    verified 2026-08-10: the command-mode travel package (0xC6BE90)
    issued for every MoveTo and live probe distances closed steadily
    (min 85.6→47.8, 118.1→60.9, 722.7→524.7, ...) — settlers walked to
    the Sanctuary workshop, observed in-game ("everyone, even traders")
    — and the session ended with no crash; the crash blamed on the call
    earlier was a corrupt save (same signature in no-DLL runs), now
    prevented by DisableExitSave

═══════════════════════════════════════════════

0.4.0 — Co-save · "The World Remembers"

═══════════════════════════════════════════════

Goal: the simulation rides inside the game's save files.

STATUS: IMPLEMENTED — verified in-game (2026-08-10): a save wrote 637
entities (105 KB) and the load restored them (`The Commonwealth wakes
up: 637 minds restored from the co-save`), with the restored world
ticking its first pass.

[✓] F4SE serialization records — the adapter's stable type names and
    versioning over the core's Capture/Restore substrate (CoSave, tested
    — CoSaveTest 6/6 suites green)
[✓] Lifecycle — PreSaveGame → Capture → record; load → record →
    Restore (applied on the load's completion event — kPostLoadGame,
    deduped against kGameLoaded); PreLoadGame/DeleteGame/new game →
    Clear (serializers survive)
[ ] Migration — old saves load forward (the version seam is in
    CoSave::Decode; the first schema change exercises it)

═══════════════════════════════════════════════

0.5.0 — Living World · "The Settler Goes to Market"

═══════════════════════════════════════════════

Goal: the real test from the contract.

[✓] Species/behaviour split (groundwork) — SpeciesTag component +
    BehaviourProfile table: animals cannot trade, buy, or talk (no
    Social/Comfort needs → no Socialize/Work intents); the market
    memory keeps Trade-kind so the dog still walks; the profile decides
    what arrival means (Aid, not Trade) — see Docs/Design/Behaviour.md
[ ] World facts — Remember pushes (weather, market open/closed)
[ ] Tuning from the Configuration service
[ ] Arrival outcomes — ReportOutcome per species (Human → Trade with
    the trader; Animal → Aid with the settlement)
[ ] The real test: a settler goes to market because they are hungry —
    no script
[ ] Nexus name check + publish
