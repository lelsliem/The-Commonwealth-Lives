# The Living Commonwealth — Changelog

**Fallout 4 adapter for the Living Commonwealth Engine (LCE)** — an F4SE
plugin that makes the Commonwealth *live*.

Every record below is a real commit — the full history of what was built
and fixed, newest first. Release notes with the plain-English story live
in [RELEASE_NOTES.md](RELEASE_NOTES.md).

---

## 0.7.4 verification — the log flood tamed (2026-08-12)

### 0.7.5 fix — blows are people's business: children row, never fight (2026-08-12)

The safety audit after the kin gate found one more gap: the fight
escalation had no species gate, and a child with an enemy (the bond book
gates animals, not children) could throw and take punches at the market.
Rows stay open to everyone — words are natural — but both fight
participants must be Human. The gate lives in EscalateToFight, the single
chokepoint both entry points share (the bench crossing and the shut-stall
slight), so a future fight source cannot forget it.

The audit confirmed safe: the player is excluded from seeding and walks;
companions are never seeded (settler-faction/role/vendor gate); robots,
turrets, and spotlights are device-gated; animals are fed, never feuding;
children are fed at the bench but never trade, run stalls, or romance
(Child species + no aging — siblings can never become eligible); fights
book a sim Combat memory, not hostile game state, so no quest-essential
actor is ever forced into combat; a married couple dissolving into
rivals/enemies is drama, not a bug (the pouch splits on dissolve).

### 0.7.5 fix — family is off the menu: the kin gate (2026-08-12)

The sim forms sweethearts and spouses from dispositions — but the world it
seeds already contains families, and a father and daughter whose warmth grows
over shared meals must never cross the sweetheart line. The bond book now
knows who is kin. A curated table (Kin.h) names the vanilla families'
parent-child lines, verified against the Fallout wiki: the Abernathys (Lucy
with Blake and Connie) and the Finches (Daniel with Abraham and Abigail). The
Longs and Warwicks are deliberately excluded — they are married couples, and
the sim marrying them is lore-correct.

Every actual child — game-born or sim-born — is Child species and already
refused romance; the table exists for the adult relatives who would otherwise
be eligible. RebuildKin indexes loaded actors' base forms (low 24 bits,
stable whatever the load order) and folds curated pairs into a kin set that
both bond channels — the reconcile pass and the event handler — feed into
ApplyPair. A kin pair caps at Friend, never Sweetheart or Spouse, and the
gate heals a pre-fix save's mistake on the first pass. On load the log reads
`kin: N family pairs gated from romance`.

### 0.7.5 fix — the species split is enforced, not hoped (2026-08-12)

The fight test exposed a real gap: behemoths (Gizmo, Harley) were brawling at
the market — trading, feuding, shoving each other. SupermutantBehemothRace was
missing from the animal table, so every behemoth defaulted to Human: a pouch,
a name, enemy bonds, a feud. The animal table grew, and a restored mind's
stored species is no longer trusted — it is re-derived from the actor's race
the moment the actor loads (ReclassifyLoadedMinds), so a behemoth saved as
Human before the fix is corrected in place, pouch dropped.

The behavior gates, three layers so an animal can never leak into people's
business even if a table misses a race: the bond book (Reconcile + the event
channel) refuses a pair where either side is an animal (no friend, rival,
enemy, or spouse — a dog has no feud to row); the bench crossing skips an
animal outright; and restore prunes an old save's animal bonds and animal
stall-keepers, so a pre-fix world heals itself. Owned animals keep their
names; unowned strays stay nameless.

Also fixed: the game's 'Worker' work crews showed no name (added to the
generic-name list — a Worker gets a real name like any other person), and
the bond book now caps spouses at one per mind — a would-be second spouse
bond caps at sweetheart, and an existing marriage is never broken.

### 0.7.5 fix — the once-per-day gate is a day-scoped map (2026-08-12)

The first 0.7.5 test in a restored 610-mind world showed fights firing in
waves — the same pair came to blows 141 times in one session (445 fights
total), everyone shoved repeatedly, both sides falling over. The once-per-day
gates (rows and fights) gated on the pair's Wronged/Combat memory, but
memory fades (sim.memory.fade 0.2/s, forget at 0.1): a weight-1.0 event
erases itself in 4.5 seconds, so the gate re-armed mid-day and the pair
re-rowed and re-fought every ~10s forever. The gate now lives in a durable
adapter-owned map — ordered pair → { last day rowed, last day fought } —
day-scoped, O(1), and co-saved (record v7) so save/load never double-rows or
double-fights. The regression test erases the pair's memories mid-day and
asserts the gate still holds.

The fight also answers back now: a victim whose temper is at or above
sim.fight.temper shoves the aggressor back — one exchange, never a loop.
Running away already existed (the Combat threat feeds the engine's
danger-awareness; the victim flees when Safety is their most urgent need).

### 0.7.5 Fights — the feud turns physical (2026-08-12)

The verbal altercation (0.7.2 Rows) gets its physical escalation: when an
enemy pair rows at the bench (or a slighted mind faces an enemy keeper),
the temper line and the chance coin decide whether blows land. The fight
books the engine's Combat wrong on both sides — the feud deepens, the
victim carries a threat and starts avoiding the aggressor (the engine's
danger-awareness), the settlement hears it (gossip), the fight pool
speaks, and the radio carries it. sim.fight.chance (0.1; 1.0 forces every
eligible escalation — the test knob) and sim.fight.temper (1.0). v1
books the fight with zero new game calls; the punch is visible too —
the victim is shoved back from the aggressor via the game's own
knockback (AIProcess::KnockExplosion, the same physical push the "Get
Out Of My Face" mod uses), sim.fight.push (default 3; 0 off).

### 0.7.4 fix (0.8.1 field finding) — decisions log: stable minds go silent

The 1s decisions cadence (previous fix) fired for every mind, so a restored
600-mind world wrote ~640 lines/s (65 KB/s of synchronous game-thread file
I/O) — the 'little hangs'. A stable mind now prints its intent once and stays
quiet until the intent actually changes; flip-flopping minds are still capped
at one line per second. Same seed + same world, the log drops from hundreds
of lines/s to tens.

The growing frame hang traced to the verify channel itself: near-tied
intents (Rest/Explore) re-rolled every frame, and each re-roll wrote a
synchronous file line on the game thread — 22,478 "decides" lines in
under three minutes, 75% of the whole log, alongside every HUD message
moment. The game froze silently at the end of that session.

- **Time-based decision logging** — `LogPlanEntry` dedupes on intent
  *and* time: a mind's "decides X" line logs when its intent changes,
  or at most once per `sim.log.decisions.every` seconds (default 1.0)
  while it flip-flops. The verify channel stays readable — one line per
  mind per second at most — instead of a 150-line-per-second file-I/O
  flood on the game thread.
- The intent-change gate still fires instantly on real decisions (a
  walk's target, an arrival), so nothing observable is lost; only the
  per-frame Rest/Explore re-roll noise is throttled.

---

## 0.7.4 Trade with anyone — the sellers are minds (2026-08-12)

The bench was the only food source a hungry mind remembered; the world
is full of sellers. 0.7.4 widens who the walk resolves to — a person
who sells, not only the bench. The engine needed nothing: the core
already resolves a Trade memory to a person.

- **The vendor signal** — `SimRelevant::IsVendor`: the game's runtime
  vendor faction, or a merchant container on any base-form faction
  (deterministic — the runtime faction is computed lazily). Anyone who
  sells passes sim-relevance on their own and becomes a full mind — a
  name, needs, memory, co-save, just like a settler. The road trader,
  the marketplace stall-keeper, the caravan merchant — all in.
- **The who-sells seed** — `SeedVendors` mirrors the market seed:
  every human mind remembers the nearest seller within walking
  distance, at weight 1.05 — a hair above the market seed's 1.0, so a
  person who sells out-scores the bench while both are fresh (the
  core's tie-break goes to the first event, and the market seed runs
  first). The bench takes over again when the seller leaves. A vendor
  never shops at their own stall; sellers near each other trade with
  each other — the marketplace comes alive.
- The arrival already traded directly with a person target and
  `RecordSale` warmed the seller — the exchange is unchanged, only who
  is remembered widened. Pure `NearestVendor` (Market.h) tested; 21/21
  harness suites green.

  **Verification fix (same day):** the live test showed 6,634
  person-targeted walks — settlers walking to Kessler, Deb, the
  caravan guards — but zero trades: the closed-market slight gate
  fired for *every* human arrival, person targets included (it never
  checked `atBench`), so a settler reaching a vendor while the
  settlement market was shut hit the "stall is shut, went hungry"
  branch and returned. The stall is the bench's geography: a walk to a
  *person* is at the seller's own market, so the gate now applies to
  bench arrivals only — person trades land whatever the hour (the
  world-facts gate still stops new walks after hours; only walks
  already in flight arrive).

## 0.7.3 verification — the road's people (2026-08-12)

In-game testing showed the role rule missed two of the very people it
was built for:

- **Supply-line settlers are named by the game itself.** The label
  "Provisioner" lives on the reference's display name while the base
  form stays the generic "Settler", so the role rule (reading the
  base) never matched — and the game's own text-display override
  swallowed the sim's write. The role rule now reads the actor's
  **display name first** (falling back to the base), and the sweep
  writes through a bare role-word display; a deliberate player rename
  is still respected.
- **The road caravans hold no settler faction.** The generic
  "Provisioner" and "Caravan Guard" NPCs that roam the Commonwealth
  with the brahmin never passed sim-relevance, so the sim couldn't
  name them. A role-word **base form now passes the gate on its own**
  (device-race and NPC-base checks still apply) — the road's people
  are minds, and they are exactly who a hungry settler trades with on
  the road (0.7.4). Confirmed live: Trader Chloe, caravan guards with
  names.
- **One game limitation, proven and accepted.** The visible name of a
  supply-line provisioner cannot be changed while assigned — the game
  re-derives "Provisioner" from the assignment itself, ahead of any
  extra-data override (diagnostic: `base 'Settler'`, wrote "Provisioner
  Atlas", read-back "Provisioner"). Even the console `setname` fails
  there, per the top Nexus renaming mod's known issues. The sim's role
  name still lives in memory, log, and co-save; the radio speaks it;
  and the write lands the moment the provisioner is reassigned.

## 0.7.3 Names for everyone — the roles gain people (2026-08-12)

Trade memories (0.7.4) reference a *person* — but the game-name-first
rule kept the role placeholders ("Provisioner", "Guard", "Minuteman")
bare, so every provisioner in the Commonwealth read identical in
memory. A role label is now a title, not a name: the sim keeps the
role as a prefix and adds the person.

- **`Names.h`** gains the role machinery, pure and tested: `IsRoleName`
  (case-insensitive match: Provisioner, Guard, Minuteman, Caravan
  Guard, Trader, Merchant — people only), `HasRolePrefix`,
  `GenerateRoleName` ("Provisioner Cole", never a family name) and
  `GenerateUniqueRole` (deduped against the world like any name).
- **All three naming paths** honour the rule: a role mind is seeded
  "Role First" at creation; the per-second sweep guards the base-name
  converge so the bare role word is never stamped back on, and a
  pre-0.7.3 mind (a bare role word, or a restore-time full name)
  converges to its role name; restored worlds self-heal on the next
  sweep.
- **Real names still win** — Sturges stays Sturges; the rule touches
  only the nameless roles. Raiders never reach the sim, so enemies
  keep the game's own labels.
- 21/21 harness suites green (NamesTest extended with the role rules);
  in-game verification pending.

## 0.7.2 Rows — in-game verification fixes (2026-08-12)

0.7.1's speech and 0.7.2's rows verified live in a 673-mind restored
world: named dialogue fired on shut-stall slights exactly as designed.
Verification exposed two real seams, both fixed edge-only (no engine
change):

- **The feud's geography includes the keeper.** The row scan read
  only the attendance book (who walked here today) — a keeper
  planted or restored at her own bench never walks, so she never
  entered it and could never row with the very minds every shut-
  stall slight aimed at her. The scan now crosses the stall-keeper
  directly at each arrival; the once-per-day Wronged gate keeps a
  keeper who did arrive today a harmless double-scan.
- **The workshop's props are not minds.** Turrets and spotlights hold
  the settler faction and were seeding as Human — needs, walks (434
  active walks in the test session), stall slights, and feuds
  (`Deacon is feuding with Missile Turret` ×4). `IsSimRelevant` now
  excludes device/robot races (the three turret races, robots,
  LibertyPrime) and any actor with no NPC base record; a polluted
  co-save self-heals via `PruneDeviceMinds` on restore (one summary
  line, before pouches/names/bonds rebuild), and a fresh world never
  seeds them. The species table's "robots are deliberately absent"
  note is finally honored; synths stay Human.

Same-session follow-up: the wall-mounted spotlight's race
(`0x01002804`, Automatron) slipped both tables — the new prune
announces any Human mind with an unknown race once per session, and
that line named it, so the spotlight joined the device table too. The
unknown-race announce stays: it is the device table's way of
learning.

21/21 harness suites green; build clean. ADR-0026.

---

## 0.7.0 — Identity & the Player Window (2026-08-11)

Settlers have **names**, relationships can go **bad**, and the player
**hears** what's happening.

- `0fccdfe` 0.7.0 Identity: names, slights and feuds, and the news feed
- `cbd8128` Names that show in-game: curated pools, actor display-name
  overrides, INI synced
- `09194c0` Names fix: read the game name from the base form, not the
  reference
- `65b6686` Name the junkyard dog again and give provisioners a first name
- `231b2d3` Sweep converges stale species-word animal names to the naming
  rule
- `b72938d` Pet names unique per world, provisioners keep the role, audio
  radio after 0.9.0
- `b1a4c6e` Identity.md: mark the feud's in-game verification
  engine-blocked
- `160d395` Ship sim.hunger.desperate = 0.2 — the feud's gate, now live
- `95784aa` Feud headline fires on any crossing into Enemy, not just a
  jump from None
- `3a5b2b8` Mediate a feud the moment it breaks — gossip fades before the
  day pass
- `a89b562` Docs: 0.7.0 is complete and verified in-game — every doc
  flipped
- `f69379a` Release materials for 0.7.0: full changelog and the
  copy-paste release description
- `9027e3c` Plan 0.8.0 Real Events: trade with anyone who sells,
  conversations, and altercations
- `f2335c0` RealEvents plan: simple one-liner dialogue with the author's
  five-line escalation ramp
- `908ca0b` Draft the dialogue pools: the good (greet/gossip), the bad
  (row/trade), and the ugly (grief/fight/feud)
- `a804256` Plan the path to 1.0.0: the staged run, the release gate,
  and the honest cuts
- `e5c065f` Polish pass: docs caught up to shipped 0.7.0, the CoSave
  test's future-version gate fixed, warnings cleared

**Verified in-game:** names on actors, slights on closed markets, rival →
enemy crossings ("is feuding with"), feud gossip through the settlement,
mediation by a well-liked neighbor, HUD news + radio captions, co-save v6
round-trip. **Shipped** as tag `0.7.0` (release live 2026-08-11).

---

## 0.7.1 — Talk (2026-08-11)

The first Real Events stage: the sim's existing social interactions
become visible — a mind that trades, eats with its family, or is
slighted at a shut stall says a line.

- `Dialogue.h` — the author's pools (the good greet/gossip/family, the
  bad trade/row, the ugly grief/fight/feud), INI overrides
  (`dialogue.*`), and a seeded picker (per mind and day — the same
  line all day, a new one tomorrow; two pools never pick in lockstep;
  the Rng stream untouched).
- `Say` wired into the paid trade (buyer to keeper), the family meal
  (spouse to spouse), and the shut-stall slight (the first words of a
  feud); the line rides the news feed so the settlement radio reads it
  as a caption.
- 20/20 harness suites green (DialogueTest added). In-game
  verification pending.
- `04b0ef5` Plan 0.8.1 Illness & Medicine: the engine's one open
  design question answered — Health is an adapter-owned component
  (fact-plus-tick), not a new NeedType
- `2d34030` Life/death visibility: the body is buried (0.8.2, after
  the mourning window), the child is a mind by design (visible
  children are a CK asset stone, post-1.0) — ADR-0023
- `180546d` Visible children get a reachable path (Baby Sim, Nexus
  100934 — permission + integration, not build-from-scratch); named
  traders (We Have Names, Nexus 74287) are a census input; Crime and
  Punishment assessed and skipped

---

## 0.7.2 — Rows: the verbal altercation (2026-08-11)

A feud gets a scene: rivals and enemies who cross paths at the same
bench have words. Each remembers the other wronged them — the engine's
unprompted-wrong channel (Wronged, −0.25), distinct from the shut-
stall's executed let-down (−0.1) — the settlement hears the shouting
(gossip), and a crossing can push a pair over the enemy line the
instant it lands.

- `src/Rows.h` — the crossing-row logic, pure like Gossip:
  `Exchange` (Wronged both ways + gossip) and `AlreadyRowedToday` (the
  once-a-day gate, co-saved so save/load never double-rows).
- Wired at bench arrivals: an ephemeral per-day attendance book (who
  walked to each market today, pruned, never co-saved); each arriving
  mind rows with any rival/enemy already there — two `Say` lines (the
  row pool) + the `words first` log line, the radio reads the row as
  a caption.
- The shut-stall slight keeps its −0.1 channel and its Say, unless
  the keeper already rowed this arrival (no repeated line).
- 21/21 harness suites green (RowsTest added). In-game verification
  pending.

---

## 0.6.0 — Life & Emergent Quests (2026-08-11)

Settlers are born, live, and die; they make friends and enemies; quests
happen because life happens.

- `e9e1537` Ship the tuning INI template the adapter has always read
- `6e4bd74` Mark 0.5.0 complete: GitHub publish done, next milestone 0.6.0
- `cea2eff` Link the live GitHub repo in the README
- `90adbeb` Add the 0.5.0-beta release notes
- `9fee775` Plan 0.6.0: Life & Emergent Quests — "The Commonwealth
  Remembers"
- `d1fceaa` Fold the author's ideas into the plan: names, agency, the
  player window
- `70f2f52` 0.6.0 Stone 1: the world keeps its books
- `4ac96f1` Harden the death check: require a streamed-in actor (3D)
  before reading death markers
- `c4b3dee` Two-pass death confirmation: a corpse reads dead twice, an
  artifact once
- `b5f6617` Deaths require a prior alive sighting: a death is a transition
- `451b59e` Document the seen-alive death rule in ADR-0011
- `a5ea7d6` Mark Stone 1 verified in-game: the book keeps clean, kills
  register
- `9454e5a` Stone 2 — bonds: the EventBus subscription, mutual + sticky
  derivation, and co-save v5
- `abe0f8f` Bonds need the buyer's warmth and a slower clock: the first
  in-game test's findings
- `d38ae5d` Mark Stone 2 (bonds) verified in-game
- `08ef98d` Stone 3 — households: the Spouse bond becomes one shared wallet
- `fe6c13f` Stone 3.5 — the sleep cycle: Rest recovers Fatigue so fed
  minds wake and walk again
- `26b7805` The sleep cycle's payoff was swallowed by the walk layer:
  arrival must end the walk session
- `7127112` Review pass: rest rescues parked minds; the walk cap is
  tuning; the log tells the truth
- `59d0caa` Walk cap: per-market budget and one rate-limited line — the
  log flood and the starvation, both fixed
- `1237baa` Arrival cooldown: a mind that just arrived is satisfied — no
  more per-frame arrival/feed loop
- `514bcc4` Stone 3 (households) verified in-game: the marriage emerged,
  survived save/load, and the family bench fed the spouse
- `57a8fcd` 0.6.0 build pass: meal-cadence hold, gossip, arcs, birth —
  16/16 suites green
- `27141de` Household monogamy guard: a second marriage cannot merge a
  third pouch into the shared wallet
- `985107c` Meal-cadence wander: Rest/Explore command a bounded walk to a
  real nearby refr instead of freezing the settlement
- `3d9555d` 0.6.0 truth items: children counted at wake, gossip death
  line, tunable wander
- `f34eb17` Fix the restore-birth: a reload no longer births a child or
  re-mediates every feud
- `ca16aa7` Fix grief: the announce was dead code, and the family meal now
  keeps marriages warm
- `5ac05af` Grief announce: once per bereavement, not every frame of the
  fresh window
- `a8a56e6` Close out 0.6.0: mark the milestone verified in-game and
  shipped as tag 0.6.0

---

## 0.5.0 — The Living World (2026-08-10)

The heartbeats, translations, walks, co-save, and the living world stones
0.1–0.5.

- `ccccde6` First stones: scaffold + docs for the Fallout 4 adapter
- `0823a1a` Fix the plugin link: one spdlog in the DLL
- `d7ed7e3` Trim Depends to the two clones the build uses
- `3b4a9aa` Record upstreams for the trimmed Depends clones
- `7653665` Adopt core 0.4.0: snapshot substrate + version pin
- `23b6328` Docs: first heartbeat verified in-game
- `b1e00d9` Design: entity ↔ form translation stone
- `ac9207a` Translation stone: the settlers wake up
- `ddc9e6e` Docs: translation stone verified in-game
- `4e034e3` Close out milestone 0.2: roadmap + version bump
- `0b39732` Design: intent executor stone
- `6be4522` Intent executor stone: the simulation ticks in-game and
  intents act
- `a93ddb1` Fix the per-frame tick: hook the game's frame driver, not the
  VM queue
- `5e081bc` Restore all five tick-hook candidates with per-hook fire
  counters
- `24373f2` Prune the tick to the verified per-frame pair
- `0260a34` Walk the farmer to market: pin the walk call, seed the market
- `fbcb875` Instrument the walk: a distance probe proves arrival in the log
- `900ad82` Scope the market to walking distance: the probe's cross-map
  lesson
- `623b433` Let the walk probe outlive the fading memory
- `41ad291` Add first-pass tick markers to split the silent-log mystery
- `b1facee` Make the walk probe dense: log as the settler moves, not on a
  clock
- `a42a079` Read the walker's live position from its 3D node, not
  GetPosition
- `7c34435` Command the walk: kMove state so sandbox cannot override it
- `73813a2` Name the mystery load: log lifecycle messages with the save
  name
- `cd19873` Survive the exit-save reload: revive the world when a load
  aborts
- `915a721` Command the walk: the game's command-mode travel package
- `60a385a` Walk with the game's real move-here: pin
  InitiateCommandModeTravelPackage
- `c32b701` Refuse the travel call: it needs the game's command state and
  crashed
- `b5737ad` Log the WalkTo command-state refusal once per process
- `b552609` Walking: re-enable the travel call — the crash was the save,
  not the call
- `af23618` Walking stone verified in-game — settlers walk to market
  (0.3.0 done)
- `b815ca9` Co-save (0.4.0): the sim rides inside the save file
- `00d5c0d` Co-save: abort recovery discards the aborted load's snapshot
- `2c9a1a2` Abort recovery: wait 60s, not 12s — real loads are slow, not
  dead
- `da67ff6` Co-save: apply the restore on kPostLoadGame, not just
  kGameLoaded
- `1b6a727` Co-save stone verified in-game — 637 minds restored from the
  co-save
- `99ee495` AdapterProject: rewrite the handoff for the verified 0.1–0.4
  state
- `e73fd33` Add a fitting quote to every source banner
- `28c14f0` Split behaviour by species: animals cannot trade, buy, or talk
- `0fdf885` Add Child species and verified race FormIDs to the behaviour
  split
- `552ff3f` Finalize the handoff: species split built, core stones synced
- `30ec5ac` Re-seed the market memory on co-save restore — restored worlds
  walk again
- `e9547de` Re-push the market fact every second: late-loading restored
  minds walk
- `ddea8eb` Mark the restore-walk fix verified in-game
- `909fd6e` Per-species food sources and arrival outcomes: a dog is fed,
  not traded
- `3e98451` Design the real test: the closed hunger loop, no script
- `8a079e2` Widen the arrival radius to the real stop distance
- `48ed9b4` Close the hunger loop: arrival feeds, the trip pays off
- `d2c6f17` Mark the real test verified in-game — the loop closes
- `312912f` World facts: the market's trading hours and the radstorm gate
  the sim
- `762b52c` Tuning and migration: the last two unchecked stones close 0.5.0
- `fdc43e6` Pin the radstorm gate: CommonwealthGSRadstorm 001C3D5E
  verified from the xEdit weather dump
- `81ffbd8` Weather memory events: the day's sky, remembered — a label,
  not a door
- `9d9100e` Weather log lines: adjective labels + a world-turns line that
  reads right
- `9ac493f` Per-settlement markets: each settlement's own bench, not a
  single global market
- `9488b3c` Desync the herd: per-mind need jitter so hunger doesn't march
  in lockstep
- `601a41a` Expose seeded need decay rates in the INI (sim.*.decay)
- `a169a96` The trade stone: a human arrival at the market is a real
  exchange
- `b059246` The economy stone: a physical exchange — pouches, prices, and
  paid meals
- `fe34115` Docs: sync all adapter docs to the engine's per-tick decay
  jitter (stone 07)
- `38d2668` Wire the engine's decay jitter: own an Rng, pass it to Update,
  persist its state
- `21ca636` Fix the census fallback and the silent tuning line
- `3821a1d` Version banner: bump to 0.5.0 and stamp each build with the
  git hash
- `4459131` Fix the settlement census: scan persistent cells, not the
  empty REFR form array
- `cc3f6ec` Persist stall-keepers in the co-save record (v3): markets
  reopen under the same keeper
- `9511962` Fix the keeper-return edge: a stall-keeper at their own bench
  no longer re-claims the stall
- `43c4e4f` Fix the idle-world lifecycle and harden walk probing
- `54480e7` Tidy the docs for 0.5.0: rewrite README, add LICENSE, sync
  stale references
- `6db265c` Sync to the 0.5.0 core contract: memory world-days, record v4,
  pin 0.5.0+
- `4db8781` Plan change: publish to GitHub first, Nexus later
- `b343efe` Set the plugin author to lelsliem
