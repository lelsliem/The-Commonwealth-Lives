═══════════════════════════════════════════════

The Living Commonwealth — Fallout 4 Adapter

Building living worlds through simulation.

═══════════════════════════════════════════════

Project Roadmap

═══════════════════════════════════════════════

Status

Current Version : 0.8.7 — dialog growth. The 0.8 run (0.8.0 →
                   0.8.6c) shipped and was field-verified 2026-08-14
                   (tag `0.8.6`); the dialogue work then landed as
                   this stone (see the 0.8.7 section): every dialogue
                   pool re-curated from the game's own
                   settler/guard/child/ghoul recordings
                   (Docs/Dialogue/LineCatalog.md), shipped in code +
                   INI, plus the Realistic Conversations
                   compatibility (the 33-GMST overrides re-delivered
                   as a tuning file, no ESP).

Current Stage   : 0.8.9 DONE — the birth journey is verified
                   in-game (2026-08-16): the carry, the deferred
                   child spawn that materializes real on save/load,
                   and the dress find (children wear clothes now).
                   The 0.8.7 dialog exchange, 0.8.8 timings & weights,
                   and 0.8.9-road all landed in the same pass — 27/27
                   harness suites green. See the 0.8.7 / 0.8.8 / 0.8.9
                   sections.

Next Milestone  : 0.8.10 animations + fight-feel (the ESP/idle
                   pattern extends; the both-fall/ghost-push fight
                   bugs land here) → 0.8.11 log hygiene + loose ends
                   → 0.8.12 final touches + beta on Nexus

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

STATUS: COMPLETE ✅ (verified in-game 2026-08-10)

[✓] Design — Docs/Design/Executor.md, Docs/Design/Walking.md
[✓] Per-frame tick — a hook on ProcessVMTick (ID 2251368)
[✓] Plan builder — pure and tested; refusals are the contract
[✓] Action table — MoveTo through the Movement::WalkTo seam
[✓] Walking call — pinned statically against Fallout4.exe 1.11.221
[✓] The market — every mind remembers where to trade
[✓] Walking in-game — a MoveTo executes and a settler walks

═══════════════════════════════════════════════

0.4.0 — Co-save · "The World Remembers"

═══════════════════════════════════════════════

Goal: the simulation rides inside the game's save files.

STATUS: COMPLETE ✅ (verified in-game 2026-08-10)

[✓] F4SE serialization records — the adapter's stable type names
[✓] Lifecycle — PreSaveGame → Capture → record; load → record →
    Restore
[✓] Migration — old saves load forward

═══════════════════════════════════════════════

0.5.0 — Living World · "The Settler Goes to Market"

═══════════════════════════════════════════════

Goal: the real test from the contract.

STATUS: COMPLETE ✅ (verified in-game 2026-08-11, tag `0.5.0-beta`)

All stones verified: species split, world facts, weather memory,
per-settlement markets, desync, trade, economy, stall-keepers
persisted, tuning INI, GitHub publish.

═══════════════════════════════════════════════

0.6.0 — Life & Emergent Quests · "The Commonwealth Remembers"

═══════════════════════════════════════════════

Goal: settlers are born, live, and die; they make friends and enemies;
and quests happen because life happens.

STATUS: COMPLETE ✅ (verified in-game 2026-08-11, tag `0.6.0`)

All 16 stones verified: lifecycle, bonds, households, sleep cycle,
wander, gossip, arcs, birth, engine hand-over (Requests A–C).

═══════════════════════════════════════════════

0.7.0 — Identity & the Player Window · "The Player Listens"

═══════════════════════════════════════════════

Goal: the world is alive even when the player isn't watching.

STATUS: COMPLETE ✅ (verified in-game 2026-08-11, tag `0.7.0`)

Names, conflict source (the feud), radio/HUD news. 19/19 suites.

═══════════════════════════════════════════════

0.7.1 — Talk

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-12)

Dialogue pools (greet/gossip/family/trade/row/grief/fight/feud),
seeded picker (per mind, per day), wired into trade, family meal,
shut-stall slight. 20/20 suites.

═══════════════════════════════════════════════

0.7.2 — Rows

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-12)

Verbal altercations at the bench, Wronged memories, settlement
gossip. 21/21 suites.

═══════════════════════════════════════════════

0.7.3 — Names for Everyone

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-12)

Every unnamed sim-relevant mind gets a name. Role titles gain
the person ("Provisioner Daisy"). Game names win. 21/21 suites.

═══════════════════════════════════════════════

0.7.4 — Trade with Anyone

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-12)

Vendor census — traders, merchants, provisioners as trade targets.
Hungry walk resolves to a person, not only the bench.

═══════════════════════════════════════════════

0.7.5 — Fights

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-12)

Physical escalation: temper + chance, once-per-day gate (ConflictGates
co-saved v7), paired-push kick, exchange on beats (kick → fall →
get-up → retaliation → slink-off), fight lines as subtitles
gated by sim.subtitle.radius. Species + kin splits enforce brawl
is human-only. 23/23 suites.

═══════════════════════════════════════════════

0.7.6 — Fight-feel Bug Pass

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-13)

[✓] ESP-based kick — `TheLivingCommonwealthAnims.esp` (unconditional
    IDLE clone of PairedFrontPushKick, the crowd mod's recipe)
    delivers a real kick animation on both beats (ADR-0051)
[✓] Fall tips instead of sliding — push force 8→3 (the tip-over zone)
    and every beat waits for actors to be on their feet (IsDown guard,
    ADR-0050)
[✓] Ghost-slide fixed — stagger + pushback tuned (stagger 2,
    pushback 25)

═══════════════════════════════════════════════

0.7.7 — Babies

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-13)

[✓] Pregnancy component — conception day, due day, parent IDs,
    co-save serialized
[✓] BirthDay component — child's birth stamp for growth tracking
[✓] Birth lifecycle — conception → pregnancy window → birth event →
    named child → fed by household
[✓] Growth — SpeciesTag upgrades Child → Human after
    sim.birth.childhood days
[✓] Species gate — only Human×Human pairs conceive
[✓] CheckBirths two-pass — prevents iterator invalidation crash
    (remove-component safety)
[✓] Dead-parent safety — skips birth if either parent was destroyed
[✓] INI: sim.birth.enabled, sim.birth.chance (0.05),
    sim.birth.gestation (3), sim.birth.childhood (10)

═══════════════════════════════════════════════

0.7.8 — Visible Children Pairing

═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified in-game 2026-08-13)

[✓] Graceful degradation — sim.birth.visible INI flag (default off);
    without the baby mod, children stay sim-only
[✓] Runtime pairing — PairVisibleChildren scans ProcessLists for
    HumanChildRace/GhoulChildRace actors, filters already-translated,
    collects sim-only children (Species::Child, no FormRef), pairs
    greedily
[✓] FormRef + translator entry — paired child walks, trades, bonds
[✓] External mod: Baby Sim - Babies That Grow Up (Nexus 100934)
    — usable now, editable on permission

═══════════════════════════════════════════════0.7.9 — Bugs & Polish
═══════════════════════════════════════════════

STATUS: COMPLETE ✅ (verified 2026-08-13)

[✓] Codebase audit — no bugs, no stale TODOs, all INI defaults
    match code, all comments reference correct versions
[✓] Version bump — xmake.lua updated to 0.7.9
[✓] Docs sweep — Roadmap, README, AdapterProject, DecisionLog
    all consistent and up to date
[✓] 23/23 harness suites green, build clean

═══════════════════════════════════════════════

0.8.0 — Illness & Medicine

═══════════════════════════════════════════════

STATUS: BUILT ✅ (harness-verified 2026-08-13) — in-game pending

[✓] Health component (adapter-owned, co-save additive): 1.0 healthy,
    0.0 dead, carries the Sickness (kind, severity, contracted day,
    hold remaining)
[✓] The hold-then-recover curve (pure Illness.h): contraction drops
    health to the hold and it holds while the sickness runs; severity
    grows while untreated (children faster); at the cap the tail of
    the hold drains toward death — the rescue window; recovery climbs
    back at the recovery rate
[✓] Four contraction vectors: radstorm days, food (shared meals),
    wounds (fights), and contagion (the sick spread it in their
    settlement)
[✓] The visible cost is rest — Fatigue decay multiplied while ill,
    easing off as health recovers, so the sick tire faster and Decide
    produces Rest more often
[✓] Medicine — the trade stone's second good: a sick mind at the
    market buys a dose (caps leave the pouch or shared wallet), the
    hold ends, recovery starts early; a broke sick mind rests instead
[✓] Death at the bottom — a severity-capped, untreated illness can
    drain health to zero; the mind is removed with the same
    bookkeeping a kill books (never restores), the settlement hears
    "died of sickness"
[✓] Tuning — sim.illness.* INI keys for the whole curve and every
    vector, defaults tuned so death is rare and earned
[✓] IllnessTest — six stages (contract, hold/recover, fatal path,
    medicine, fatigue toll, child fragility); 24/24 suites green

═══════════════════════════════════════════════

0.8.1 — The Illness Field Pass

═══════════════════════════════════════════════

STATUS: IN PROGRESS — the co-save audit, the cough gate, the radio
pacing, the illness kill, and the child retune landed (2026-08-13);
the balance and the wound window wait on an in-game session.

[✓] Mid-outbreak co-save — DONE, and it caught a real bug: Health
    (0.8.0) and Pregnancy/BirthDay (0.7.7) were registered as
    serializers but never named in the co-save's stable-name table, so
    a mid-hold illness and an in-progress pregnancy were both lost on
    save/load (CompanionTag too, harmless — it re-derives). All four
    names now ride the record; MidOutbreakSaveTest locks the
    round-trip. 25/25 suites green.
[ ] Illness economy balance — now that the market cures, verify the
    numbers hold in normal play: 25 caps per dose, the recovery
    window, the medicine price vs typical pouch wealth. The day-12
    test showed 73 buys — confirm the economy doesn't make illness a
    non-event, and that the broke-rest path is the honest minority.
[✓] The cough at outbreak scale — DONE: the per-mind rate limit
    held but a 50-sick settlement was ~4 overlapping coughs a second.
    The global gate (`sim.illness.coughGlobal`, default 4 s) caps one
    cough anywhere per window on top of the per-mind interval — the
    tell stays audible, the wall of sound is gone.
[✓] Illness news cadence — DONE: the once-per-sickness announce set
    was correct, but a radstorm day dumped all 50–87 names into the
    feed at once. Burst pacing (`sim.illness.newsMax` 4 per
    `newsInterval` 10 s) lets an outbreak unfold as a radio story;
    each mind still announces exactly once.
[✓] Sickness takes the body — DONE: the illness death booked the
    sim death but never killed the game actor (the one death the
    adapter causes itself), so the mind died and the body walked on,
    re-entered, and died again. The death now kills the actor (self-
    attacker, lethal) — the corpse appears (0.8.2 buries it), the
    dead stay dead.
[✓] Child fragility retuned — DONE: at childMult 2.0 every untreated
    childhood illness was fatal (any seed ≥ 0.2 crossed the line in
    the hold) and children can't buy medicine — all four outbreak
    deaths were children. Retuned to 1.1: rest cures the common
    vectors; only a wound (unreachable by a child in a brawl) is
    fatal. IllnessTest stage 7 locks it.
[ ] Wound vector sanity — an untreated wound is the only death that
    should be earned; verify the 40 s rescue window reads right in
    game (medicine still turns it around).

═══════════════════════════════════════════════

0.8.2 — The Burial · "The settlement lays its own to rest"

═══════════════════════════════════════════════

STATUS: VERIFIED in-game (2026-08-14) — corpse disabled on
schedule, settlement heard it, ledger clears on save

The corpse must not linger forever. The sim's death path is complete
in the books (death fact, grief, legacy) but the game corpse stays in
the settlement cell forever (no cell reset there). After the mourning
window (`sim.death.burialDays`, INI-tunable), the adapter disables the
corpse ref and logs `the settlement laid X to rest` (+ a news line).
Body state is game state — the adapter owns it, no engine change.

- Disable the corpse after the mourning window, gate on the day since
the death was booked
- News + log receipt, co-save aware (a buried corpse stays buried)
- Verify: kill a settler, wait out the window, the body is gone and
the settlement heard it

═══════════════════════════════════════════════

0.8.3 — The Sick Household · medicine as a stocked good

═══════════════════════════════════════════════

STATUS: VERIFIED — in-game 2026-08-14

The economy stone's next step: medicine is not an endless shelf.
Stalls stock a limited supply per day (`sim.illness.stock`, INI,
default 10) that sells out, so an outbreak can outrun the market —
the broke-rest
path and the honest season return. A sick mind's household notices:
when one spouse is ill, the other's market trip can buy the dose for
the sick one (the shared wallet already exists — the intent is a
family that cares).

- Per-stall daily medicine stock, sells out, replenishes next day
- The family buys for the sick member (shared wallet + a care check)
- Verify: a big outbreak drains the stalls; a married sick settler
  gets dosed by the spouse's trip
- **Verified in-game:** self-buy (25-cap dose from a provisioner),
  the broke-gate, and shelf persistence across save/load. The
  sell-out and family care-buy share the verified `buyDose` path;
  their triggers (a drained shelf, a sick spouse) surface in normal
  play.
- **Field gap parked for the 0.8.6a audit:** only stall-keepers
  earn caps — caravan workers/provisioners/unemployed spend and
  never earn, going permanently broke. A wage or piecework income
  so working minds slowly refill (pouch += N, adapter-side only).

═══════════════════════════════════════════════

0.8.4 — Random Interactions · the trial

═══════════════════════════════════════════════

STATUS: VERIFIED — the trial passed, the stone ships (2026-08-14)

Settlers interact with each other unprompted — not only when hunger
drives them to a bench. The trial first: watch what the existing sim
already produces (person-market trades, bench rows, Socialize,
proximity), run test trials, and implement only what succeeds. If
the trial fails, it closes as a documented finding and the run moves
on.

- The proximity pass: loaded humans, nearest non-walking neighbour
  within `sim.interact.radius`, roll `sim.interact.chance`, speak
  via the existing `Say` — the pool follows the bond (family /
  row / greet-gossip), a jittered `sim.interact.cadence` keeps it
  sparse, and a walking mind never talks. All adapter-side.
- Verify: two settlers who cross paths sometimes greet, talk, or
  trade without a hunger drive; rate-limited, never spammy

═══════════════════════════════════════════════

0.8.5 — MCM + Settings Manager · the player tunes in-game

═══════════════════════════════════════════════

STATUS: VERIFIED — the page renders with real values, sliders hot-apply within a second, and the override persists across reload (2026-08-14)

A Mod Configuration Menu page exposing the tuning keys (hunger
rhythm, market hours, bond thresholds, births gate, illness curve,
and the interaction knobs the 0.8.4 trial proved) so players tune
the world in-game. The INI stays the source of truth; MCM writes an
override file (`Data\MCM\Settings\TheLivingCommonwealth.ini`) the
adapter overlays at load and hot-applies within a second (a
last-write stamp poll in the per-second tick). No native Papyrus
bridge: this pin's VM header exposes no registration surface, so the
override-file route carries zero crash risk. The MCM mod is a soft
dependency — no MCM, the INI alone rules.

- The page: 5 pages / 53 controls (Life, Interactions, Relationships,
  Illness, Birth & Fights) bound to MCM ModSettings; shipped
  defaults in MCM/Settings/TheLivingCommonwealth.ini
- Verify: an MCM change survives reload; defaults restore cleanly;
  no MCM installed → the INI alone still works (soft dependency)

═══════════════════════════════════════════════

0.8.6a — The Audit

═══════════════════════════════════════════════

STATUS: DONE — the audit is written (Run080 §0.8.6a, DecisionLog
0060). Verdict: the ReleasePlan's cuts stand for hands/pex/audio/
provisioner names; MCM is **overruled to KEEP** (0.8.5 proved it);
visible children are redesigned to ship gated on the Baby Sim
permission; and the field added two 0.8.6b items — **the earn-caps
**economy (non-keepers are perpetually broke, 0.8.3) and **log
**hygiene (~200KB/min decision chatter). Fight presentation bugs
defer to 0.8.10 animations.

0.8.6b — Redefine & Loose Ends

═══════════════════════════════════════════════

STATUS: IN PROGRESS — the earn-caps economy is the first item.

Apply the audit: scope redefined to what the beta actually needs;
the loose ends from 0.8.1–0.8.4 closed; the docs reconciled with
the cuts.

- **Earn-caps economy (0.8.6b):** the settlement stipend is BUILT
  and harness-pinned (`StipendMark` + the once-per-day sweep, 27/27
  suites; Economy.md). The "who pays" extension shipped, failed in
  the field, and was BENCHED (2026-08-14) — both knobs stayed in the
  tree, reverted to the working minted stipend: `requireOwned`
  defaults 0 and `source` stays settlement. Both unbench items
  landed in 0.8.6c (below): the ownership AV read and the RemoveItem
  count-semantics fix for player-pays. The minted default never
  regressed.
- **Log hygiene:** the decision-chatter rate limit (audit item 2).

0.8.6c — Scale in the Field

═══════════════════════════════════════════════

STATUS: PASSED 2026-08-14

The hard gate, measured: a normal save with hundreds of settlers
must tick inside a frame budget (`TickReport` once a minute). A beta
that chugs at 600 minds is dead on arrival.

- Verify: a restored 600+ mind save holds frame time over a long
  session — DONE: four reports at 618–619 live minds, worst single
  Update 4.13–8.17 ms (typical ~5 ms) vs. the 16.6 ms frame budget;
  game played smooth throughout. The sim's share is roughly half a
  frame; Decide is the heaviest pass (a later optimization target,
  not a gate failure).

- **The who-pays bench reopens (the two 0.8.6b failures, Economy.md §
  Who pays):**
  1. **Ownership read — DONE + FIELD-VERIFIED 2026-08-14.** The
     truth is the game's own `WorkshopPlayerOwnership` actor value
     (form 0x33C in Fallout4.esm; its editor ID is literally
     `WorkshopPlayerOwned`), read off each census workshop ref by
     form ID — the exact storage `SetOwnedByPlayer` writes and the
     game's own checks read. The second half was timing: the query
     once ran at the menu-world wake, read fresh defaults, and never
     re-ran after the save loaded — `EndWorld` now re-arms it per
     world. Field log: menu wake 0 of 28, loaded save 28 of 28 owned,
     matching the player's map. The road not re-walked (all died in
     the field): `GetOwner()` null everywhere, the per-ref
     `OwnedByPlayer` script property (fresh defaults until the cell
     streams), the quest's `Workshops` script array (built at runtime
     from `WorkshopsCollection`, empty through the retry window), and
     both `getav` console names.
  2. **Player-pays deduction — DONE, field-verified 2026-08-14**
     (`the wage bill of 5190 caps came from the player's purse
     (25100 -> 19910)`). Root cause was the count's sign: the unified
     inventory native removes on a POSITIVE count and adds on a
     negative one; the first build passed `-totalOut`. Fixed with
     positive count + `kSelling` reason + the before/after
     `GetGoldAmount` diagnostic. Both who-pays items are now done and
     field-verified.

═══════════════════════════════════════════════

0.8.7 — Dialog growth

═══════════════════════════════════════════════

STATUS: DONE (first half) — the curated catalog, the voice-aware
         picker, the ghoul bank, and the Realistic Conversations
         compatibility all shipped and verified in-game 2026-08-15;
         27/27 harness suites green. The audio trigger probe (can
         the DLL make the game play a line's .fuz?) is built and
         deployed, awaiting the in-game verdict.

**The curated catalog replaces the drafted lines.** Every pool was
re-curated from the game's own recordings. The voice-bank survey
(Docs/Dialogue/LineCatalog.md) extracted and classified the game's
settler bank (~17,500 files), guards (4 voices), Gen3 synths,
children, and ghouls, and each pool now carries the game's real
lines — Greet 30, Gossip 23, Row 32, Trade 30, Family 16, Grief 15,
Fight 2, Feud 2 — with per-voice coverage recorded (`(8v)` = all 8
settler voices recorded it). Shipped in both `src/Dialogue.h`
(defaults) and the INI (`dialogue.*`), one content swap, zero new
machinery. The comma gotcha was handled: the list parser splits on
commas, so mid-line pauses use em-dashes. Fight/Feud stay thin by
design — rare events whose real content (who, why) is
memory-driven caption text.

**The voice-aware picker (ground truth from the Voices BA2).** The
design rule (decided with the author): a line only speaks if the
speaker's voice bank recorded it — the game resolves audio by voice
type, so a voice can never say a line its bank lacks; when no line
exists for that voice, the mind stays mute (captions only), and
nothing can be done if the game simply doesn't provide it.

- **Voice** (Dialogue.h) — the 8 settler voices, 4 guards, 2
  children, 2 ghouls, as a bitset (VoiceSet).
- **Coverage table** — every curated line's recorded voices, as
  **ground truth from the game's own `Fallout4 - Voices.ba2`** name
  table (a formid → voice-set map keyed by normalized text; NOT the
  catalog's `(nv)` counts, which say how many voices but not which).
  The survey corrected two assumptions: "Last warning." is recorded
  by all 8 settler voices *and* the guards (0x0FFF — shared), while
  "No loitering." is genuinely guard-only (0x0F00).
- **PickForVoice** — the voice-aware picker: only draws lines the
  speaker's voice recorded; a voice with nothing in the pool returns
  empty (mute). The seed folds the voice, so two voices of the same
  mind disagree. Custom INI lines (no catalog entry) have coverage 0
  — caption-only, never wrong-voice audio.
- **VoiceOf / VoiceOfForm** (Adapter) — resolves a mind's voice from
  its actor's NPC voiceType editor id (verified against the ESM's
  VTYP table); nullopt → caption-only. Say() uses PickForVoice when
  a voice resolves and logs `[voice]` vs `[cap]` per line, so the
  in-game log proves which path ran.
- **The ghoul bank** — ghoul settlers resolve to their own voice
  types; the survey found they recorded the same 8v generic lines as
  the settlers (52 of the pool's lines, same formids), so 64
  curated lines carry ghoul coverage and a ghoul speaks them from
  the same table. Named voices (Sturges, Marcy, companions) have no
  recording of the generic lines — the mute rule stands for them.

**The audio trigger probe (sim.diag.audioProbe).** The remaining
question — can the DLL make the game actually *play* a curated
line's .fuz on an actor, not just caption it? — is now a built,
deployed experiment. The vendored commonlibf4 has no direct Say
wrapper (the FO4 Address Library has no Actor::Say entry), so the
probe alternates the two game-owned routes every
`sim.diag.audioProbe.every` seconds on the nearest loaded settler:
`[probe say]` dispatches the papyrus VM's own `Say` on the picked
line's parent topic (the exact call Papyrus dialogue mods make —
the game's own INFO selection is voice-filtered, .fuz + lip +
subtitle), and `[probe greet]` fires `AIProcess::ProcessGreet` (the
ambient greeting entry, the same family as the kick's PlayIdle
seam). To route the probe through the production path, the catalog
table gained its INFO form ids (a text → formid column, ground
truth from the LineCatalog) and `FormIdFor()`. Off by default;
the log tags each attempt so the in-game ear knows which route
fired. Verdict pending in-game.

**The Realistic Conversations compatibility.** The 2018 ESP's 33
GMST overrides re-delivered as a tuning file
(`Realistic Conversations.ini` next to the DLL): the adapter applies
each key to the game's own `GameSettingCollection` at load — no
xedit patch, no ESP, no load-order slot. A missing file is the off
switch; a setting the game no longer has is skipped, never fatal.
The NPC-to-NPC voiced chatter it drives is the game's own system;
our sim's subtitle lines are a separate text channel that never
touches those settings.

The remaining piece is the audio trigger: playing a line's actual
.fuz on an actor (the vendored commonlibf4 has no direct Say
wrapper — the probe tests the ProcessGreet / dialogue-package
route). Nothing engine-side is needed: voice types and audio are
purely game-facing, which the adapter already owns.

0.8.8 — Timings & Weights

═══════════════════════════════════════════════

STATUS: DONE — built 2026-08-15, field tuning pending

**Registers (in-world, 0.8.9-register, built 2026-08-15):** the
bond names the register the exchange asks the game to voice —
family for a bonded household (`kMisc_Greeting` + hello fallback),
flirt for a compatible unpaired pair (the moment, the game's own
words), greet for the crowd. The register threads both exchange
beats; `sim.interact.register.family` gates the subtype attempt.
Field verdict (2026-08-15): the game refuses `kMisc_Greeting` for
settler voices, so the attempt defaults off. Flirty Commonwealth
evaluated and not integrated (player-targeted male-only explicit
lines); the greeting topic is a proven slot for custom flirt lines
if ever wanted.

**Road feed (0.8.9-road, verified in-game 2026-08-15):** the road
people — Provisioners, Caravan Guards, Caravan Workers — keep no
settlement-market memory and eat from the caravan's supplies at
`sim.road.feedThreshold` (default 0.25; `road: … ate on the road`
×6 on test). Their bonds and memories travel with them — road
people exchange with each other and with settlers in passing (15
road-person exchanges on test). Guards and traders keep their
markets.

The new interactions' pacing, tuned in INI and the MCM Interactions
page (`sim.interact.*`):

- `pairCooldown` (60 s) — a specific pair can't re-exchange within
  the window (the same two settlers never greet twice a minute).
- `dailyCap` (0 = off) — how many interactions a mind may open per
  sim day before it goes quiet.
- `weight.greet / .gossip / .family / .row` — the pool pick is now a
  weighted roll: greet and gossip dominate the crowd, family is
  boosted ×10 for bonded pairs, row is the rare quiet line between
  friends, and a feud pair is a hard row whatever the weights say.

0.8.9 — Babies, implemented

═══════════════════════════════════════════════

STATUS: VERIFIED IN-GAME (2026-08-16)

The birth journey made visible/real, in three pieces:

- **The carry (visible journey):** on birth (sim.birth.visible), the
  mother visibly holds a swaddled bundle — the mod's ethnicity
  variant when loaded, the game's own Shaun bundle (babybundled,
  which the mod's bundles are literal copies of) when not, so
  everyone gets a carry. Equipped through the game's own equip path;
  the mod's scripts are never touched; the hold (mother, bundle,
  born day) rides the co-save v10 (BabyHold section) so a mid-carry
  survives save/load.
- **The child (the Commonwealth's own):** after sim.baby.holdDays
  (default 2) the bundle comes off and a child from the game's own
  vanilla pool — the farm children, gender-matched to the sim
  child's own deterministic gender — spawns at the mother's feet
  (persistent) via the **deferred spawn**: the ref is deliberately
  left un-initialized, invisible until the game's own save/load
  routine completes the actor (facegen, AI process, animation) and
  it steps out fully real. That is the only spawn route that works —
  forcing the init immediately yields the half-born headless,
  T-posing, immobile child that even survives saves, and the game's
  own PlaceAtMe dispatch is the documented PackVariables CTD. The
  child never joins the settler faction (no duplicate mind); it is
  paired with the sim-only child mind (name rides the extra data
  from the first moment). The pair (mother, child actor) rides the
  co-save v11 (VisualChild section) so a child waiting to
  materialize survives the very load that completes it. Gated on
  sim.baby.visualChild (default on).
- **The clothes (the dress find, 0.8.9-child):** children kept
  spawning in their pants because the ChildOutfit* records are OTFT
  outfit bundles (BGSOutfit), not ARMOs — a cast to TESBoundObject
  failed on every one, so the equip silently no-op'd while the log
  claimed "dressed and fully real". The child base now gets its
  default outfit (defOutfit/WNAM) set to a gender-matched bundle at
  spawn — the game's own child-clothing path (the bundles are
  orphans no NPC record references) — so the init that materializes
  the child also applies its clothes; a per-tick pass confirms the
  dress once the child reads fully initialized (3D AND an AI
  process) and falls back to equipping the real ARMOs inside the
  bundle. Gender-aware (the dress is female-only). Verified: children
  appear dressed and stay dressed across reloads.
- **The feed (never a starving baby):** FeedChildren now home-feeds
  every not-yet-grown child, paired or not — a newborn paired with a
  visible baby actor still cannot walk to a bench; the household feed
  is its meal until GrowChildren makes it a grown mind that walks
  like anyone.

The crib walk and the market baby-goods shelf were CUT: the mod runs
as its author designed, and the sim only adds the moment of birth and
the moment of the child. The baby-mod integration stays a soft
dependency (permission requested, author engaged). 27/27 harness
green, deployed.

0.8.10 — Animations + fight-feel

═══════════════════════════════════════════════

STATUS: PLANNED

The interactions get bodies: the ESP/idle pattern proven by the kick
(0.7.6) extends to the new interactions — greeting gestures, chat
stances, the altercation's shove — with graceful fallback when the
ESP is absent. The deferred fight presentation bugs (the both-fall
look, the ghost-push slide) land here with the animation pass.

0.8.11 — Log hygiene + loose ends

═══════════════════════════════════════════════

STATUS: PLANNED

The audit's second gap (the decision-chatter rate limit) plus the
parked questions — "should an unowned settlement have minds?" and
the other who-is-the-world loose ends — closed before the release
materials.

0.8.12 — Final Touches + beta

═══════════════════════════════════════════════

STATUS: PLANNED

The clean run before the beta: every field note folded in, docs
reconciled, warnings cleared, harness at full green, the README
written for players, the release package assembled — then the first
public release ships on Nexus; GitHub stays the source; beta
feedback feeds the post-beta run.
