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
[✓] Migration — old saves load forward. The record is
    self-describing (each component under its stable name), so the seam
    is two rules in Decode: version > current refuses (a future format
    is not ours to guess), version ≤ current loads — an older record
    decodes without the components a newer build added, and the safe
    default applies (a pre-species save restores minds with no tag →
    Human); a component name this build does not know (a removed type)
    is skipped and dropped, never fatal. The first real change in the
    wild was additive (species) — no bump needed. Implemented
    2026-08-10, pinned by CoSaveTest's crafted v0/v2 fixtures

═══════════════════════════════════════════════

0.5.0 — Living World · "The Settler Goes to Market"

═══════════════════════════════════════════════

Goal: the real test from the contract.

[✓] Species/behaviour split (groundwork) — SpeciesTag component +
    BehaviourProfile table: Human / Child / Animal (race FormIDs
    verified in xEdit 2026-08-10). Children and animals cannot trade,
    buy, or run stalls; animals also lack Social/Comfort needs (no
    Socialize/Work intents); the market memory keeps Trade-kind so the
    dog still walks; the profile decides what arrival means (Aid, not
    Trade) — see Docs/Design/Behaviour.md
[✓] World facts — the doors the world shuts, pushed as memory events
    with an invalid Other (Remember's world-fact channel): the market's
    trading hours (08:00–20:00, the first tuning keys) gate the hungry
    walk — a remembered { invalid, Trade } fact makes Decide Explore
    instead of MoveTo — and a radstorm gates the gatherings ({ invalid,
    Social }; weather forms pinned from the xEdit dump — only
    CommonwealthGSRadstorm 001C3D5E matches, see Docs/WeatherForms.md).
    The
    refresh pattern keeps a shut door shut (fact topped to full weight
    each second — no flicker, no memory growth) and lets it fade (~4.5 s)
    when it reopens. Implemented 2026-08-10; in-game verification
    pending (the transition lines + settlers stopping at night) — see
    Docs/Design/WorldFacts.md
[✓] Weather memory events — the day's sky, remembered (not gated): the
    live weather forms (verified xEdit dump) classify into six
    categories, pushed as day-stamped world facts ({ invalid,
    WeatherRain, 1.0, day } — the engine grew the Weather kinds,
    appended save-safe). Today's categories refresh all day ("it rained
    this morning" survives until the world turns); yesterday's fade;
    the day-turn logs. Re-derived at the edge — never co-save state.
    Implemented 2026-08-10; in-game verification pending (the sky-turn
    + world-turns lines; `fw 1ca7e4` forces rain) — see
    Docs/Design/WeatherFacts.md
[✓] Per-settlement markets — every settlement's workshop is its own
    market. A one-time census over the REFR form array (base form
    000C1AEB "Workshop") finds every vanilla settlement market; each
    mind remembers the nearest workshop within ~140 m — Sanctuary at
    Sanctuary, Tenpines at Tenpines, a mind in the wastes knows none
    and explores until it finds one. The legacy single-bench fallback
    (000250FE) survives when the census finds nothing (an interior, a
    bare world). Implemented 2026-08-10; in-game verification pending
    (the `settlement census:` log line + each workshop's formid in    the MoveTo decisions; a settler at Tenpines should walk to Tenpines'
    bench, not Sanctuary's) — see Docs/Design/SettlementMarkets.md
[✓] Desync the herd — every mind's needs are born slightly different
    (VaryNeeds: a deterministic per-entity jitter on each need's value
    and decay rate). Hunger arrives at different times, so the
    settlement stops marching to the market in lockstep; the decay-rate
    jitter is a metabolism, so the stagger persists after every feed and
    grows as the session runs. The co-save already serializes the rate,
    so a mind's rhythm survives restore. Implemented 2026-08-10;
    in-game verification pending (the fed line + MoveTo confidences now
    spread instead of matching; walks trickle instead of wave)
[✓] Tuning from the Configuration service — one text file next to the
    DLL (Data\F4SE\Plugins\TheLivingCommonwealth.ini): the sim.* keys
    feed SimulationTuning::FromConfiguration (memory fade, drift, trust,
    ...); the adapter's own keys (market.open.hour / market.close.hour)
    ride in the same file, replacing the WorldFacts.h constants as the
    hours gate's source. Missing/broken lines keep defaults — a broken
    line never breaks the world. Implemented 2026-08-10; in-game
    verification pending (the `tuning: loaded` line + an hours override
    actually moving when settlers stop) — see Docs/Design/Tuning.md
[✓] Food sources + arrival outcomes — per-species food sources (a dog
    is fed by its owner when the game assigns one, else the settlement;
    humans trade at the market) resolved at seed time; on arrival,
    ReportOutcome per species (Human → Trade, Partial — no trade yet;
    Child/Animal → Aid, Success — fed, gives nothing in return).
    Implemented 2026-08-10; in-game verification pending (the arrival
    log lines + the feeder readout prove it)
[✓] The real test: a settler goes to market because they are hungry —
    no script. VERIFIED in-game 2026-08-10: the hunger write-through
    on arrival (fed: Hunger X -> 1.00) closes the loop — needs decay →
    MoveTo → walk → arrive → fed → not hungry → no walk (the fed dog
    decided Rest, not MoveTo, 6 ms after feeding; 19 feeds, both
    animals cycling). Goals seeded per species (Human: AcquireFood;
    Child/Animal: none). Engine ask remains: a Feed kind (or Aid
    serving AcquireFood) so animal goals can be served when wired.
[ ] Nexus name check + publish
