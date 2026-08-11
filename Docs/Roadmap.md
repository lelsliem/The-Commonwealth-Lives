═══════════════════════════════════════════════

The Living Commonwealth — Fallout 4 Adapter

Building living worlds through simulation.

═══════════════════════════════════════════════

Project Roadmap

═══════════════════════════════════════════════

Status

Current Version : 0.6.0 — complete, verified in-game, and shipped
                   (tag `0.6.0`, release page live 2026-08-11; 0.5.0
                   previously tagged `0.5.0-beta`)

Current Stage   : Life & Emergent Quests — every stone on the 0.6.0
                   board is built, 16/16 harness suites green, and
                   verified in-game (2026-08-10/11). The world keeps
                   its books (arrivals, deaths, departures — a real
                   kill books confirming, the dead never restore and
                   never ghost-walk); bonds are real (friend →
                   sweetheart → spouse emerged from shared meals across
                   sessions, restored from the co-save v5 map, with
                   the living drift clock and the trade-as-trust rule
                   behind them); households share one pouch, one
                   stall, one bench (the family stall verified in-game,
                   marriages survive save/load); the sleep cycle closes
                   the loop (eat → rest → recover → walk → eat); the
                   meal-cadence wander executes in-game (Rest/Explore
                   command a bounded walk to a real nearby reference,
                   furniture preferred — settlers mill around instead
                   of freezing at the bench); gossip spreads deaths to
                   every mind (`643 minds remember settler X is gone`
                   verified in-game); grief fires and drains the
                   bereaved's Social (`arcs: settler 0x50976 grieves
                   for 0x2f2a7 — they seek company.` — exactly once per
                   bereavement after the flood fix); and sim-only
                   children are born, fed by their households, and
                   restored from the co-save (`3 sim-only children
                   restored too` across two save/load cycles). The
                   only honest deferral: the feud arc's organic
                   appearance — nothing in 0.6.0 makes dispositions
                   negative, so enemy pairs cannot form yet (the
                   conflict source is 0.7.0's good-and-bad
                   relationships; the feud is harness-verified).

Next Milestone  : 0.7.0 — Identity & the Player Window: names for
                   settlers, negative relationships (the feud's
                   conflict source), and the radio so the player
                   hears the world

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
    Implemented 2026-08-10; in-game verification pending (the
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
    in-game verification pending (the fed line + MoveTo confidences now
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
    line never breaks the world. Implemented 2026-08-10; in-game
    verification pending (the `tuning: loaded` line + an hours override
    actually moving when settlers stop) — see Docs/Design/Tuning.md
[✓] Seeded need decay rates in the INI — sim.hunger.decay / .fatigue /
    .safety / .social / .comfort (the adapter's keys, sharing the sim.*
    prefix with the core's): the seeded rhythm becomes tunable to "a few
    meals a day" (~0.001–0.005/s; the default 0.1/s is an empty stomach
    in ~10 s). Rates thread StartWorld → SeededNeeds, still jittered per
    mind by VaryNeeds, serialized by the co-save. Missing/broken values
    keep the defaults. Implemented 2026-08-10; in-game verification
    pending (create the INI with sim.hunger.decay = 0.002 and settlers
    should visit the market a handful of times a game day instead of a
    steady stream) — see Docs/Design/Tuning.md
[✓] Food sources + arrival outcomes — per-species food sources (a dog
    is fed by its owner when the game assigns one, else the settlement;
    humans trade at the market) resolved at seed time; on arrival,
    ReportOutcome per species (Human → Trade — the trade stone makes it
    Success; Child/Animal → Aid, Success — fed, gives nothing in
    return). Implemented 2026-08-10; in-game verification pending (the
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
    person, not the bench. Implemented 2026-08-10; in-game verification
    pending (the `trades with settler` / `sets up the stall` lines) —
    see Docs/Design/Trade.md
[✓] The economy stone — the exchange is physical: every human mind
    carries a CapPouch (40 ± 20 caps, deterministic per id), a buyer
    pays what they can afford up to sim.meal.price (default 5) and the
    seller's pouch grows; a broke buyer is fed on the settlement's
    credit, never starved. The pouch is a co-save component (stable
    name cappouch), so a saved purse restores exactly; pre-economy
    saves are back-filled on restore. Implemented 2026-08-10; in-game
    verification pending (the trade lines name the caps; sellers' pouches
    grow while buyers' shrink) — see Docs/Design/Economy.md
[✓] Stall-keepers survive save/load (co-save record v3) — who runs
    each market's stall rides the co-save as (market FormID, keeper
    FormID) pairs — form ids, stable across sessions — and ApplyRestore
    rebuilds the map from the restored FormRefs. A saved market reopens
    under the same keeper instead of whoever arrives first; a pre-v3
    save restores with no keepers and each stall re-derives on the
    first arrival. Implemented 2026-08-10; in-game verification pending
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

STATUS: BUILT (2026-08-11) — all three stones implemented, 19/19
harness suites green (NamesTest, SocietyTest, CoSaveV6Test), engine
0.7.0 (Legacy) wired into the death/birth paths. In-game verification
pending. Design: Docs/Design/Identity.md.

[x] Stone 1 — Names: `Name` component (additive — the record's v6 bump
    is the legacy section, not the component), game names first
    (Sturges stays Sturges), gender-split pools + a separate animal
    pool, all curated in the INI (`names.first.male/.female/.animal`,
    `names.last`), owned animals named / strays nameless, back-filled
    on restore; decisions, bonds, gossip, arcs, births, deaths, and
    the radio all speak names with the console hex beside them
[x] Stone 2 — The conflict source (the feud's organic fuel): the
    engine's decided channels (kind and result, never a sign) — a
    hungry arrival at a closed market reports
    `ReportOutcome({keeper, Social, Failure})`, the temper line
    (`sim.slight.temper`, from the engine's JitteredTraits substrate)
    decides who blames, settlement `Groups` (derived from the market
    memory, never persisted) spread the echo through the settlement,
    and rival/enemy bonds form on their own — the built feud arc
    finally begins in the wild
[x] The radio, step 1: world events (bonds, births, deaths, feuds,
    market openings) become one-line news — HUD notifications
    (throttled) + the news feed the radio reads
[x] The radio, step 2: a settlement radio object plays the news as
    on-screen captions while the player is near (`radio.base.formid`
    — flagged for xEdit verification, configurable)
[x] The radio, step 3 (deferred): real audio — only when assets exist
[ ] MCM + Settings Manager — deferred, honestly: the INI already
    delivers the tuning behavior (loaded at boot, survives reload);
    the UI page needs the MCM mod + the Creation Kit (an author-side
    asset task, not a sim task)

═══════════════════════════════════════════════

0.8.0 — Settler Agency · "Hands in the World"

═══════════════════════════════════════════════

Goal: settlers have hands — their intents produce real world changes:
build, move items, destroy. Sketched 2026-08-10 (Docs/Design/Life.md,
"Beyond 0.6.0").

STATUS: PLANNED — sketches only, no code.

[ ] Move items: a hauler transfers stock between containers; the stall
    sells what was stocked
[ ] Build: a builder places objects via the workshop placement path;
    the settlement grows what it built
[ ] Destroy: a destroyer clears clutter (disable refs); decay, raids,
    and spite take things apart
[ ] Engine ask (tentative): labour as first-class goals (Construct /
    Haul / Demolish) grows Request B — otherwise the adapter maps
    Prosper → labor intents on the existing surface
