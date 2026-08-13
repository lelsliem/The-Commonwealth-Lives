═══════════════════════════════════════════════

The Living Commonwealth — Fallout 4 Adapter

Building living worlds through simulation.

═══════════════════════════════════════════════

Project Roadmap

═══════════════════════════════════════════════

Status

Current Version : 0.8.0 — Illness & Medicine complete and verified
                   in-game 2026-08-13, pushed to GitHub. A natural
                   radstorm day proved the whole chain: 73 medicine
                   buys, 4 sick-but-broke resters, 0 deaths. The
                   market-cure fix (retune + hunger counter-toll) is
                   committed (`c3e837b`).

Current Stage   : 0.8.0 shipped — Health component, four contraction
                   vectors (radstorm, food, wound, contagion), the
                   hold-then-recover curve, the Fatigue toll + hunger
                   counter-toll, medicine as the trade stone's second
                   good, death at the bottom. 24/24 harness suites
                   green.

Next Milestone  : the 0.8.x run to the release gate
                   (Docs/Design/Run080.md):
                   0.8.1 illness field pass (in progress) →
                   0.8.2 burial → 0.8.3 sick household →
                   0.8.4 MCM → 0.8.5 player's stage →
                   0.8.6 scale → 0.8.7 trust → 0.8.8 docs →
                   0.8.9 bugs & polish →
                   0.9.0 the release gate (verify, not build) →
                   1.0.0 freeze and ship

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

STATUS: IN PROGRESS — the co-save audit landed (2026-08-13)

The seams the 0.8.0 in-game verification exposed, closed one at a
time. Each item is a real field finding from the natural-radstorm
test (2026-08-13) or a follow-on hardening pass.

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
[ ] The cough at outbreak scale — MTCoughing every 12 s per sick
    mind is a flood in a settlement-wide outbreak; verify the rate-
    limit holds and the tell stays audible without spamming.
[ ] Illness news cadence — "X is ill" once per sickness is the
    design, but a 50-sick outbreak must not swamp the radio; confirm
    the announce set holds at scale.
[ ] Wound vector sanity — an untreated wound is the only death that
    should be earned; verify the 40 s rescue window reads right in
    game (medicine still turns it around).

═══════════════════════════════════════════════

0.8.2 — The Burial · "The settlement lays its own to rest"

═══════════════════════════════════════════════

STATUS: PLANNED (ADR-0023's planned stone)

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

STATUS: PLANNED

The economy stone's next step: medicine is not an endless shelf.
Stalls stock a limited supply per day (`sim.illness.stock`, INI) that
sells out, so an outbreak can outrun the market — the broke-rest
path and the honest season return. A sick mind's household notices:
when one spouse is ill, the other's market trip can buy the dose for
the sick one (the shared wallet already exists — the intent is a
family that cares).

- Per-stall daily medicine stock, sells out, replenishes next day
- The family buys for the sick member (shared wallet + a care check)
- Verify: a big outbreak drains the stalls; a married sick settler
  gets dosed by the spouse's trip

═══════════════════════════════════════════════

0.8.4 — MCM + Settings Manager · the player tunes in-game

═══════════════════════════════════════════════

STATUS: PLANNED (deferred from 0.7.x — Life.md's player-window half)

A Mod Configuration Menu page exposing the tuning keys (hunger
rhythm, market hours, bond thresholds, births gate, illness curve,
gossip radius, population cap) so players tune the world in-game;
the INI stays the source of truth, MCM reads and writes it (or an
override file), with a restore-defaults button. Requires the MCM mod
as a soft dependency and a small Papyrus surface on the adapter side.

- Verify: an MCM change survives reload; defaults restore cleanly;
  no MCM installed → the INI alone still works (soft dependency)

═══════════════════════════════════════════════

0.8.5 — The Player's Stage · news where the player is

═══════════════════════════════════════════════

STATUS: PLANNED (0.9.0's news-polish item, pulled forward)

The news feed prioritizes the player's settlement: what happens where
the player stands comes first, the wider Commonwealth second, and the
throttle stays sane at 600 minds (one headline per beat, never a
wall). The radio is the storyteller — this makes it *about* the
player's people.

- Verify: standing in Sanctuary, the headlines are Sanctuary's;
  the feed never floods during a mass event

═══════════════════════════════════════════════

0.8.6 — Scale in the Field · the hard gate, measured

═══════════════════════════════════════════════

STATUS: PLANNED (0.9.0's hard gate, pulled forward)

The engine proved 5000 minds round-trip and year-long soaks; the
adapter must prove a normal save with hundreds of settlers ticks
inside a frame budget. The engine's `TickReport` (once a minute)
measures it; a release that chugs at 600 minds is dead on arrival.

- Verify: a restored 600+ mind save holds frame time over a long
  session; any hotspot found is fixed or gated behind a knob

═══════════════════════════════════════════════

0.8.7 — The Trust Story · remove-the-DLL is safe

═══════════════════════════════════════════════

STATUS: PLANNED (0.9.0's trust item, pulled forward)

Remove-the-DLL behavior documented and proven: the co-save rides in
the save, the save still loads without the mod, clean migration, no
stale-record corruption. The save/load proof is the mod's reputation.

- Verify: load a saved world with the DLL removed, then reinstall —
  nothing corrupts, nothing explodes, the world resumes

═══════════════════════════════════════════════

0.8.8 — Player Docs · the mod-page copy

═══════════════════════════════════════════════

STATUS: PLANNED (0.9.0's docs item, pulled forward)

A real README: what it does, install, tune, known limits,
compatibility notes (Sim Settlements and other renaming mods,
place-anywhere, baby mod), the settings cheat-sheet. The mod-page
copy the release will use.

═══════════════════════════════════════════════

0.8.9 — Bugs & Polish · the clean run into the gate

═══════════════════════════════════════════════

STATUS: PLANNED

The named sweep, not a bucket: every field note from 0.8.2–0.8.8
folded back in, the docs reconciled, warnings cleared, the harness at
full green. The standing rule — each fix lands with its ADR and its
log receipt.

═══════════════════════════════════════════════

0.9.0 — The Release Gate (verify, not build)

═══════════════════════════════════════════════

STATUS: PLANNED — the verify, not build, gate. The four gate items
were built in the 0.8.x run (scale 0.8.6, trust 0.8.7, docs 0.8.8,
news 0.8.5); 0.9.0 re-verifies all four together in one long session
and signs them off. Nothing ships before it.

═══════════════════════════════════════════════

1.0.0 — Freeze and Ship

═══════════════════════════════════════════════

STATUS: PLANNED

The show is complete — people visibly living — and the stage holds.
