═══════════════════════════════════════════════

The Living Commonwealth — Fallout 4 Adapter

Building living worlds through simulation.

═══════════════════════════════════════════════

Project Roadmap

═══════════════════════════════════════════════

Status

Current Version : 0.7.0 — complete, verified in-game, and shipped
                   (tag `0.7.0`, release live 2026-08-11; 0.6.0
                   shipped as tag `0.6.0` 2026-08-11, 0.5.0 as
                   `0.5.0-beta`)

Current Stage   : Identity & the Player Window — every 0.7.0 stone
                   built, 19/19 harness suites green, and verified
                   in-game (2026-08-11). Settlers are **named on the
                   actors themselves** (Mara Price, not "Settler" —
                   written via SetOverrideName, the same mechanism as
                   the console's SetDisplayName; game actors keep
                   their real names after the base-form fix, pets get
                   unique names, provisioners keep the role); the
                   world has a **conflict source** — a hungry arrival
                   at a closed market is a slight, and the feud arc
                   was proven end-to-end in-game: `the stall … is
                   shut — X went hungry and blames the keeper` → rival
                   bonds → `X is feuding with Y` → gossip → mediation
                   (`arcs: Titus Pratt cooled the feud between …`)
                   with the engine's desperate-hunger gate and two
                   adapter fixes (the feud headline on any enemy
                   crossing; feud-start gossip + immediate mediation —
                   gossip fades in ~4.5 s at sim.memory.fade 0.2, so
                   the once-per-day pass could never find a mediator);
                   the player window works — throttled HUD
                   notifications for bonds/feuds/births/deaths and
                   settlement-radio captions on screen (audio after
                   0.9.0); the co-save round-trips the whole world
                   (673 minds, 78+ bonds, 10 stall-keepers, 4
                   children restored across every load). MCM's UI
                   page stays honestly deferred (the INI already
                   delivers the tuning; the page needs MCM + the CK).

Next Milestone  : 0.7.1 — Talk, then the staged run to 1.0.0
                   (Docs/Design/ReleasePlan.md): 0.7.1 Talk → 0.7.2
                   Rows → 0.7.3 Fights → 0.8.0 Trade with anyone →
                   0.8.1 Illness & Medicine → 0.9.0 the release gate
                   (scale verified, docs) → 1.0.0 freeze and ship

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
    to GameLoaded / PreLoadGame / DeleteGame (DeleteGame — a save FILE
    being deleted — is now a no-op: it used to EndWorld the sim
    mid-world, killing it with no load pending to revive it)
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
    (MarketTest among the 13/13 suites)
[✓] Walking in-game — a MoveTo executes and a settler walks to market;
    verified 2026-08-10: the command-mode travel package (0xC6BE90)
    issued for every MoveTo and live probe distances closed steadily
    (min 85.6→47.8, 118.1→60.9, 722.7→524.7, ...) — settlers walked to
    the Sanctuary workshop, observed in-game ("everyone, even traders")
    — and the session ended with no crash; the crash blamed on the call
    earlier was a corrupt save (same signature in no-DLL runs), now
    prevented by DisableExitSave. Hardenings (2026-08-10): the probe now
    reads the actor's DATA position instead of the 3D node's world
    transform (streaming actors reported 120,000+ unit positions after a
    fast travel, so arrivals never registered), a sanity gate skips
    absurd readings, the session timeout grew 60s → 120s (the old one
    killed slow walkers mid-path across the market radius), and log
    distances are labeled in units (u), not meters

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
    deduped against kGameLoaded); PreLoadGame/new game → Clear
    (serializers survive); DeleteGame (a save file was deleted) never
    touches the running world
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
    when it reopens. Implemented 2026-08-10; verified in-game
    2026-08-11 (the transition lines — `world fact: the market is
    closed (10:00)` — and settlers stopping at night) — see
    Docs/Design/WorldFacts.md
[✓] Weather memory events — the day's sky, remembered (not gated): the
    live weather forms (verified xEdit dump) classify into six
    categories, pushed as day-stamped world facts ({ invalid,
    WeatherRain, 1.0, day } — the engine grew the Weather kinds,
    appended save-safe). Today's categories refresh all day ("it rained
    this morning" survives until the world turns); yesterday's fade;
    the day-turn logs. Re-derived at the edge — never co-save state.
    Implemented 2026-08-10; verified in-game 2026-08-11 (the sky-turn
    + world-turns lines: `world fact: the sky turns rainy (day 269)`;
    `fw 1ca7e4` forces rain) — see
    Docs/Design/WeatherFacts.md
[✓] Per-settlement markets — every settlement's workshop is its own
    market. A census over the worldspaces' **persistent cells** (base
    form 000C1AEB "Workshop") finds every vanilla settlement market —
    FO4 never fills `formArrays[REFR]`, so the Skyrim-style flat-array
    scan always read 0 in-game (the per-settlement stone was silently
    running its fallback); every settlement workbench is a persistent
    ref, so the persistent-cell scan is the correct FO4-native
    enumeration. Each mind remembers the nearest workshop within
    ~140 m — Sanctuary at Sanctuary, Tenpines at Tenpines, a mind in
    the wastes knows none and explores until it finds one. The legacy
    single-bench fallback (000250FE) covers an empty census, and the
    census now RETRIES when it finds nothing (a false 0 was locking the
    session into fallback) with a one-time base-form probe diagnostic.
    Implemented 2026-08-10; verified in-game 2026-08-11 (the
    `settlement census:` line shows the worldspace count and the probe
    shows what bases the persistent cells hold; a settler at Tenpines
    should walk to Tenpines' bench, not Sanctuary's) — see
    Docs/Design/SettlementMarkets.md
[✓] Desync the herd — every mind's needs are born slightly different
    (VaryNeeds: a deterministic per-entity jitter on each need's value
    and decay rate). Hunger arrives at different times, so the
    settlement stops marching to the market in lockstep; the decay-rate
    jitter is a metabolism, so the stagger persists after every feed and
    grows as the session runs. The co-save already serializes the rate,
    so a mind's rhythm survives restore. Implemented 2026-08-10;
    verified in-game 2026-08-11 (the fed line + MoveTo confidences now
    spread instead of matching; walks trickle instead of wave)
[✓] Per-tick decay jitter (engine core 0.5.0 stone 07, `90a9d33` —
    SHIPPED and WIRED 2026-08-10) — the engine jitters each tick's
    decay per entity: `DecayRate * Derive(id).NextFloat(1 ± sim.jitter)`
    (default 0.15, `0` off), the herd broken at the source on top of
    VaryNeeds. The adapter owns a seeded Rng, passes it to Update, and
    persists rng.State() in the co-save (record v2 — the Rng header is
    the record's first real format bump) so a restored world resumes the
    same randomness; a v1 record reseeds fresh. In-game verification
    pending (decay spreads per mind even before VaryNeeds' base rates
    differ; sim.jitter = 0 turns it off)
[✓] Tuning from the Configuration service — one text file next to the
    DLL (Data\F4SE\Plugins\TheLivingCommonwealth.ini): the sim.* keys
    feed SimulationTuning::FromConfiguration (memory fade, drift, trust,
    ...); the adapter's own keys (market.open.hour / market.close.hour)
    ride in the same file, replacing the WorldFacts.h constants as the
    hours gate's source. Missing/broken lines keep defaults — a broken
    line never breaks the world. Implemented 2026-08-10; verified
    in-game 2026-08-11 (the `tuning: loaded` line names the file and
    live values; hours overrides moved when settlers stopped; the
    0.7.0 feud test ran entirely from INI knobs) — see
    Docs/Design/Tuning.md
[✓] Seeded need decay rates in the INI — sim.hunger.decay / .fatigue /
    .safety / .social / .comfort (the adapter's keys, sharing the sim.*
    prefix with the core's): the seeded rhythm becomes tunable to "a few
    meals a day" (~0.001–0.005/s; the default 0.1/s is an empty stomach
    in ~10 s). Rates thread StartWorld → SeededNeeds, still jittered per
    mind by VaryNeeds, serialized by the co-save. Missing/broken values
    keep the defaults. Implemented 2026-08-10; verified in-game
    2026-08-11 (sim.hunger.decay = 0.002 paces the market visits; the
    feud tests ran at 0.3–1.0/s) — see Docs/Design/Tuning.md
[✓] Food sources + arrival outcomes — per-species food sources (a dog
    is fed by its owner when the game assigns one, else the settlement;
    humans trade at the market) resolved at seed time; on arrival,
    ReportOutcome per species (Human → Trade — the trade stone makes it
    Success; Child/Animal → Aid, Success — fed, gives nothing in
    return). Implemented 2026-08-10; verified in-game 2026-08-11 (the
    arrival log lines + the feeder readout prove it)
[✓] The real test: a settler goes to market because they are hungry —
    no script.    VERIFIED in-game 2026-08-10: the hunger write-through
    on arrival (fed: Hunger X -> 1.00) closes the loop — needs decay →
    MoveTo → walk → arrive → fed → not hungry → no walk (the fed dog
    deciding Rest was the first sight of the sleep cycle — Rest now
    recovers fatigue, so that is the correct, productive behavior, not
    a stall; 19 feeds, both animals cycling). Goals seeded per species
    (Human: AcquireFood; Child/Animal: none). Engine ask remains: a
    Feed kind (or Aid serving AcquireFood) so animal goals can be
    served when wired.
[✓] The trade stone — a human arrival at the market is a real
    exchange: the first human at each market sets up its stall, every
    later bench-arrival trades with them (Trade, Success — the core
    serves AcquireFood and earns trust), and the trader's half lands
    too (RecordSale: the stall-keeper remembers the customer and warms
    toward them, sim.sale.warmth). The emergent second visit: the buyer
    remembers the merchant, so the next hungry walk resolves to the
    person, not the bench. Implemented 2026-08-10; verified in-game
    2026-08-11 (the `trades with settler` / `sets up the stall` /
    fed-from-the-household lines; stall-keepers restored under the same
    keeper across loads) — see Docs/Design/Trade.md
[✓] The economy stone — the exchange is physical: every human mind
    carries a CapPouch (40 ± 20 caps, deterministic per id), a buyer
    pays what they can afford up to sim.meal.price (default 5) and the
    seller's pouch grows; a broke buyer is fed on the settlement's
    credit, never starved. The pouch is a co-save component (stable
    name cappouch), so a saved purse restores exactly; pre-economy
    saves are back-filled on restore. Implemented 2026-08-10; verified
    in-game 2026-08-11 (the trade lines name the caps; pouches
    round-trip the co-save) — see Docs/Design/Economy.md
[✓] Stall-keepers survive save/load (co-save record v3) — who runs
    each market's stall rides the co-save as (market FormID, keeper
    FormID) pairs — form ids, stable across sessions — and ApplyRestore
    rebuilds the map from the restored FormRefs. A saved market reopens
    under the same keeper instead of whoever arrives first; a pre-v3
    save restores with no keepers and each stall re-derives on the
    first arrival. Implemented 2026-08-10; verified in-game 2026-08-11
    (save with a known stall-keeper, reload — the same settler should
    still be behind the bench, with the pouch intact) — see
    Docs/Design/CoSave.md
[x] GitHub publish — repo live at
    github.com/lelsliem/The-Commonwealth-Lives (author `lelsliem` in
    xmake.lua, hygiene done), pushed 2026-08-10, tag `0.5.0-beta`; the
    release page (DLL + INI from `config/`, 2026-08-11) is live; Nexus
    comes later

═══════════════════════════════════════════════

0.6.0 — Life & Emergent Quests · "The Commonwealth Remembers"

═══════════════════════════════════════════════

Goal: settlers are born, live, and die; they make friends and enemies;
and quests happen because life happens — no scripts. The quest is the
behaviour, visible in the world and the log.

STATUS: COMPLETE ✅ — every stone verified in-game 2026-08-10/11
and shipped as tag `0.6.0` (2026-08-11). The board below carries the
verification legs: a real kill books confirming and never restores;
friends, sweethearts, and a marriage emerged from shared meals and
survived save/load (v5 bond map, ADR-0012/0013); the family stall
fed the spouse for free; the sleep cycle closed the loop; the
meal-cadence wander replaced the freeze at the bench (ADR-0017
addendum 2);
gossip spread a death to 643 minds in-game; grief fired exactly once
per bereavement after the flood fix (two bugs surfaced and were
fixed in test — the announce's dead-code form lookup and the family
meal not warming the couple, ADR-0017 addenda 5–6); and 3 sim-only children
were born, fed, and restored across two save/load cycles. The engine
hand-over (Requests A–C) is in the engine's AdapterProject.md — the
engine shipped stone 08 (RelationshipChangedEvent + sim.bond.
threshold.*) and stone 09 (Society — Groups & Traits), and gained
InteractionKind::Death for this stone.

[x] Stone 1 — The world keeps its books: the per-second census
    (Lifecycle::Diff, pure + tested) — arrivals become minds mid-session
    (one SeedMind path with the wake seed), deaths destroy the mind,
    clean its book entries (walk, log, feeder, stall), and write the
    death fact to every survivor (core gained InteractionKind::Death),
    and faction-left departures are removed with a goodbye. **Verified
    in-game 2026-08-10**: a real kill books confirming → died one second
    apart, the dead never restore and never ghost-walk, a fresh new game
    books zero false deaths, and already-dead settlers from old saves
    are parked and never booked (three hardenings: the 3D gate,
    two-pass confirmation, the alive-first rule — ADR-0011)
[x] Stone 2 — Bonds: named relationship states (friend / sweetheart /
    spouse / rival / enemy) from Disposition/Trust thresholds, persisted
    in the co-save (record v5). **Built + tested 2026-08-11 (11/11
    suites) and VERIFIED in-game** — the EventBus subscription (Request
    A), the adapter's own sim.bond.threshold.* defaults + INI keys, a
    mutual + sticky derivation (formation at the line, dissolution
    halfway back; the 1-second reconcile pass catches the quiet
    dissolves), the event channel for instant formation, the v5 bond
    section (form pair, kind, since-day) in the co-save, and the two
    in-game discoveries that made bonds real: the buyer's half of a
    trade is Remember({keeper, Social}) (Trade builds trust, never
    disposition) and the living drift clock (sim.drift.rate = 0.0002,
    ~1 h half-life — the core's 0.05/s demo default erased feelings
    between meals). ADR-0012.
[x] Stone 3 — Households: couples share a pouch, a stall, a bench, a
    bed; the shared wallet round-trips. **Built + tested 2026-08-11
    (12/12 at build; 13/13 current) and VERIFIED in-game** — the
    Spouse bond forms a household (`Households.h`, pure): the pouches
    merge into one shared wallet (split on dissolve), the family bench
    feeds the keeper's spouse without exchange, PouchOf resolves the
    wallet on both sides of the bench, and the one-pouch invariant is
    enforced silently on restore (derived state — no record bump,
    ADR-0013). In-game 2026-08-11: a real marriage emerged from shared
    meals (0x2a8a7 + keeper 0x50976, sweetheart then spouse across
    sessions), the spouse rode the co-save's v5 bond map
    (23 bonds restored), and the family bench fed the spouse for free
    (`fed from the household's meal`). The bed and walking-together
    stay deferred; the game-side Rest/Explore execution (the missing
    half that gates the meal cadence — settlers wander off between
    meals because those intents are table slots) is a documented gap
    for a later stone.
[x] Stone 3.5 — The sleep cycle: a resting mind recovers the needs a
    nap fixes — Fatigue, Safety, Comfort (sim.rest.recovery, default
    0.2/s) — so a fed mind wakes and walks again. **Built + tested
    2026-08-11 (13/13 suites)** — the 24h-market test exposed the
    park-forever bug: the need loop only decays, only Hunger was ever
    restored (on the meal), so a fed mind with drained Fatigue decided
    Rest — a table slot that did nothing — forever. The fix restores
    the rested needs before Update, keyed on the needs (not just the
    Rest intent), so a Safety-drained mind the engine would otherwise
    silence (nullopt — nothing to flee) is rescued too (ADR-0014,
    extended by ADR-0015). The loop closes (eat → rest → recover →
    walk → eat) and the same pair can finally share repeated meals and
    bond. **Verified in-game 2026-08-11** (Stone 3.75, the wander
    stone, ADR-0017 addendum 2): Rest/Explore now execute as a
    bounded wander
    (Movement::WanderNear — a real nearby reference, furniture
    preferred) so a fed mind mills around its settlement instead of
    freezing at the bench; the same pair shares repeated meals on the
    ~10 s cooldown cadence, and the first friendship (and later a
    marriage) landed within minutes at demo pace.
[x] Stone 4 — Gossip: bond, death, and feud events spread to every
    mind in the settlement (Gossip.h — the "gossip radius" is the
    settlement itself); wired into the bond-change handler and the
    death bookkeeping. **Verified in-game 2026-08-11**: a kill logged
    `gossip: 643 minds remember settler 0x2f2a7 is gone.` (and the
    observer line — 641 minds — on the follow-up).
[x] Stone 5 — Emergent arcs: the feud (a settlement that has heard of
    an enemy pair tries to cool it once per day — a liked mediator
    warms the pair a step toward zero, an unloved meddler is told
    off) and the grief (a loved death drains the bereaved's Social —
    they seek company). Arcs.h, pure; RunMediation/RunBirth wired on
    the day cadence. **Verified in-game 2026-08-11**: killing the
    keeper's spouse fired `arcs: settler 0x50976 grieves for 0x2f2a7
    — they seek company.` — exactly once per bereavement after the
    flood fix (ADR-0017 addenda 5–6). The feud's organic appearance is
    deferred to 0.7.0 (nothing makes dispositions negative yet — the
    arc is harness-verified).
[x] Stone 6 — Birth (experimental, INI-gated sim.birth.enabled): a
    spouse household has a child — a sim-only mind (no game actor),
    fed by the household, bonded to both parents, living in the
    co-save like any mind. Birth.h; one birth per sim day. **Verified
    in-game 2026-08-11**: `birth: a child is born to 0x50976 and
    0x2f2a7` and `3 sim-only children restored too — fed by their
    households` across two save/load cycles (the restore-birth bug —
    a reload treated as a new day — found and fixed, ADR-0017
    addendum 4).
[x] Co-save: no v6 bump needed — a child is an entity carrying
    existing components (SpeciesTag, Needs, Memory, Goals,
    Relationships), all already serialized; bonds v5 already persist
    the household. Gossip and arcs are derived from persisted
    components (memory events, relationships) — no record of their
    own (ADR-0017).
[x] Tuning: bond thresholds (v5), grief decay, mediation on/off,
    births on/off — sim.arc.grief.decay, sim.arc.mediation,
    sim.birth.enabled in the shipped INI
[x] Engine hand-over: Request A — RelationshipChanged observation
    event (core stone 08 candidate); Request B — GoalType growth
    (optional, deferred). Handed to the engine 2026-08-10; the
    engine shipped stone 08 (RelationshipChangedEvent +
    sim.bond.threshold.*) and stone 09 (Society — Groups & Traits).

═══════════════════════════════════════════════

0.7.0 — Identity & the Player Window · "The Player Listens"

═══════════════════════════════════════════════

Goal: the world is alive even when the player isn't watching — settlers
have names, relationships can go *bad*, and the player hears what's
happening and tunes the world in-game. Drafted 2026-08-11
(Docs/Design/Identity.md); earlier sketches 2026-08-10
(Docs/Design/Life.md, "Beyond 0.6.0").

STATUS: COMPLETE ✅ — every stone built, 19/19 harness suites green,
verified in-game 2026-08-11, and shipped as tag `0.7.0` (release
live 2026-08-11).
Design: Docs/Design/Identity.md. The verification legs: names live
on the actors (the base-form read fix — the reference full-name
lookup is empty for most actors, so Mama Murphy was being renamed
until the name came from the base form; the per-second sweep names
streaming actors and heals stale stamps; the INI synced to the
curated pools so the player-facing lists are the ones that run;
provisioners keep the bare role; pet names deduped per world); the
feud arc proven end-to-end in the wild — the engine's desperate-
hunger gate (`sim.hunger.desperate`, engine commit `509a54d`,
handed over as `81cfe48`/`1b82478`) made the slight reachable, then
23 `is feuding with` crossings, feud gossip, and 127 mediation
attempts (`arcs: Titus Pratt cooled the feud between …`) in a
stressed session; two adapter fixes landed during the hunt (the feud
headline fires on any crossing into Enemy, not just a jump from
nothing; the feud is mediated at formation because gossip dies in
~4.5 s — the once-per-day pass could never find a mediator); HUD
notifications verified on-screen ("such and such became friends"),
radio captions verified on-screen, audio honestly deferred after
0.9.0.

[x] Stone 1 — Names: `Name` component (additive — the record's v6 bump
    is the legacy section, not the component), game names first
    (Sturges stays Sturges), gender-split pools + a separate animal
    pool, all curated in the INI (`names.first.male/.female/.animal`,
    `names.last`), owned animals named / strays nameless, back-filled
    on restore; decisions, bonds, gossip, arcs, births, deaths, and
    the radio all speak names with the console hex beside them.
    **Verified in-game 2026-08-11** — with the four fixes the hunt
    surfaced: names now read from the **base form** (the reference
    read is empty for most actors — it was renaming Mama Murphy,
    Marcy and Jun); names are **written onto the actors** via
    SetOverrideName (the workshop view, pip-boy and hover read
    "Mara Price", not "Settler"); a **per-second sweep** names
    restored minds' actors as they stream in (fast-travel to a
    settlement names its settlers on arrival) and heals stale
    stamps; the shipped INI's pools were **synced to the curated
    lists** (the INI overrode Names.h silently — one source of
    truth). Pets deduped per world (the five Bandits), provisioners
    keep the bare role, and the junkyard dog is a species label, so
    owned dogs draw names again.
[x] Stone 2 — The conflict source (the feud's organic fuel): the
    engine's decided channels (kind and result, never a sign) — a
    hungry arrival at a closed market reports
    `ReportOutcome({keeper, Social, Failure})`, the temper line
    (`sim.slight.temper`, from the engine's JitteredTraits substrate)
    decides who blames, settlement `Groups` (derived from the market
    memory, never persisted) spread the echo through the settlement,
    and rival/enemy bonds form on their own — the built feud arc
    finally begins in the wild. **Verified in-game 2026-08-11** —
    the engine shipped the desperate-hunger gate (commit `509a54d`)
    that makes the slight reachable, and the whole chain fired:
    `the stall at 00054BAE is shut — Paladin Danse went hungry and
    blames the keeper` → rival bonds (Deacon, Dot Hawke, and the
    settlement echoed) → `bonds: X is feuding with Y` → feud gossip
    → `arcs: Titus Pratt cooled the feud between …` (127 mediation
    attempts in the stressed run). Two adapter fixes landed during
    verification (both committed): the feud headline fires on any
    crossing into Enemy (the rival → enemy path is how shut-stall
    feuds actually arrive), and the feud is mediated **at
    formation** — memory fade (0.2/s) erases a gossip fact in
    ~4.5 s, so the once-per-day mediation could never find a
    mediator; the feud-start spread + immediate attempt close it.
[x] The radio, step 1: world events (bonds, births, deaths, feuds,
    market openings) become one-line news — HUD notifications
    (throttled) + the news feed the radio reads. **Verified in-game
    2026-08-11** — the pop-ups appear top-left ("X and Y became
    friends", feud headlines).
[x] The radio, step 2: a settlement radio object plays the news as
    on-screen captions while the player is near (`radio.base.formid`
    — flagged for xEdit verification, configurable). **Verified
    in-game 2026-08-11** — captions rotate on screen.
[x] The radio, step 3 (deferred): real audio — only when assets exist
    (scheduled after 0.9.0)
[ ] MCM + Settings Manager — deferred, honestly: the INI already
    delivers the tuning behavior (loaded at boot, survives reload);
    the UI page needs the MCM mod + the Creation Kit (an author-side
    asset task, not a sim task)

═══════════════════════════════════════════════

0.8.0 — Real Events · "Trade With Anyone" (and the staged run to 1.0.0)

═══════════════════════════════════════════════

Goal: the world's events become real — trade with anyone who sells,
conversations between minds, and altercations that start with words and
sometimes turn physical. Planned 2026-08-11 (Docs/Design/RealEvents.md
+ Docs/Design/ReleasePlan.md); still sketched here, no code.

The roadmap to 1.0.0 is staged so each piece lands, tests, and ships
in-game before the next (Docs/Design/ReleasePlan.md):

STATUS: PLANNED — design docs written, no code.

[x] 0.7.1 — Talk: the dialogue pools speak on the social interactions
    the sim already makes; captions via the radio channel. **Built
    2026-08-11** — `Dialogue.h` (the author's pools: the good
    greet/gossip/family, the bad trade/row, the ugly grief/fight/
    feud), INI overrides (`dialogue.*`), a seeded picker (per mind
    and day — the same line all day, a new one tomorrow), `Say`
    wired into the paid trade, the family meal, and the shut-stall
    slight (the first words of a feud); speech rides the news feed
    so the settlement radio reads it. 20/20 harness suites green
    (DialogueTest). In-game verification pending (needs a session).
[x] 0.7.2 — Rows: the verbal altercation — rivals and enemies who
    cross paths at the same bench row: each remembers the other wronged
    them (engine Wronged, −0.25), the settlement hears the shouting
    (gossip), and the row can push a pair over the feud line. The
    crossing is `src/Rows.h` (pure), wired at arrival; the shut-stall
    slight keeps its −0.1 channel; physical escalation stays 0.7.3.
    In-game verification (2026-08-12) found the feud's geography
    missing its keeper — the scan only saw walkers, and a planted or
    restored keeper never walks — so the keeper is now scanned
    directly at each arrival. Same session exposed the workshop's
    props as minds (turrets, spotlights hold the settler faction and
    seeded as Human): device/robot races are now excluded from
    sim-relevance and a polluted co-save self-heals via the prune.
[ ] 0.7.3 — Fights: physical escalation via real combat (the game's
    own punch/shove anims — no pex needed); feedback into bonds and
    news
[ ] 0.8.0 — Trade with anyone who sells: a vendor census (traders,
    marketplaces, provisioners as mobile traders) — the hungry walk
    resolves to a person, not only the bench
[ ] 0.8.1 — Illness & Medicine (Docs/Design/Illness.md): a `Health`
    component (adapter-owned, co-save additive — the engine's locked
    hold-then-recover shape, answered as fact-plus-tick, not a new
    NeedType); radstorm/food/wound/contagion vectors at the edge; a
    Fatigue multiplier makes the sick rest; medicine is the trade
    stone's second good; untreated sickness can die (the existing
    death path). No engine change — the handoff's one open design
    question answered in the doc
[ ] 0.8.2 — Burial: after the mourning window (`sim.death.burialDays`),
    the adapter disables the corpse ref and logs the settlement laid
    X to rest (+ news) — the body doesn't linger forever (no cell
    reset in settlement cells). Game state, adapter-owned, no engine
    change (ReleasePlan: life/death visibility)
[ ] 0.9.0 — The release gate (hard): performance at scale verified
    in-game (a normal save with hundreds of settlers ticks inside a
    frame budget), the remove-the-DLL trust story documented, player
    docs and compatibility notes, news polish (player's settlement
    prioritized)
[ ] 1.0.0 — Freeze and ship
[ ] CUT from 1.0.0 (the honest calls, ReleasePlan.md): the "hands"
    pillar (build/move/destroy — post-1.0 if ever), pex scripted
    scenes (real combat covers fights), MCM (INI already delivers), 
    audio radio (captions work; assets deferred), visible child
    actors (sim-only children stay)
