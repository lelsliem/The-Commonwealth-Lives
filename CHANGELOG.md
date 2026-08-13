# The Living Commonwealth — Changelog

**Fallout 4 adapter for the Living Commonwealth Engine (LCE)** — an F4SE
plugin that makes the Commonwealth *live*.

Every record below is a real commit — the full history of what was built
and fixed, newest first. Release notes with the plain-English story live
in [RELEASE_NOTES.md](RELEASE_NOTES.md).

---

## 0.8.1 — the illness field pass: the tell and the radio (2026-08-13)

Two scale fixes the day-12 outbreak exposed — a settlement-wide
outbreak is one story, not a wall of sound and a wall of names.

- **The global cough gate** (`sim.illness.coughGlobal`, default 4):
  the cough was rate-limited per mind only, so 50 sick minds played
  ~4 overlapping MTCoughing idles a second. Now one cough anywhere
  per 4 s, on top of the per-mind interval (12 s) — the tell stays
  audible, the cacophony is gone. A mind that is globally gated does
  not lose its own interval.
- **Burst-paced illness news** (`sim.illness.newsMax` 4 +
  `sim.illness.newsInterval` 10): the announce set already fired
  each "X is ill" exactly once, but a 50–87-case radstorm day
  dumped them all into the feed at once, crowding out every other
  line on the radio. Now at most 4 new names enter the feed per
  10 s window; the rest wait for the next window, and each mind
  still announces exactly once. 0 = no illness news.
- Both are new INI keys + IllnessSettings fields; the code defaults
  match the INI. 26/26 harness suites green.

## 0.8.1 — the co-save audit (2026-08-13)

The mid-outbreak round-trip test caught a real gap: the co-save's
stable-name table (what rides the record) had not grown since the
economy stone, so three components registered since then never
persisted. **Pregnancy/BirthDay (0.7.7)** — an expecting couple lost
the conception on save/load (the changelog's "co-save serialized"
claim was false). **Health (0.8.0)** — a mid-hold illness was lost on
save/load; the sick woke up well. **CompanionTag (0.7.5)** — never
persisted, but harmless: it re-derives every second from the faction.
The four stable names now ride (`companion`, `birthday`, `pregnancy`,
`health`); additive, so old records decode unchanged and unknown names
drop gracefully. `MidOutbreakSaveTest` locks the round-trip: a
mid-hold radstorm sickness (value, kind, severity, day, remaining)
and an in-progress pregnancy (conception, due, parents) restore
exactly. 25/25 harness suites green.

---

## 0.8.0 — Illness & Medicine: the price of being in the wastes (2026-08-13)

Life in the Commonwealth has a price beyond hunger. Every mind carries a
**Health** component (co-save additive — a pre-0.8.0 save loads clean
with full health): radstorms, shared food, wounds from fights, and the
sick passing it to the healthy can all make a mind sick. Health drops to
the hold level (0.4) while the sickness runs; severity grows untreated
(children 2× faster); the sick tire faster and rest more — the visible
cost is rest, not a stat. **Medicine is the trade stone's second good**:
a sick mind at the market buys a dose (25 caps, out of the pouch or the
household's shared wallet), the hold ends, recovery starts early. Broke
sick minds rest instead — honest: sickness without caps means time, not
treatment. An untreated, severity-capped illness drains toward death —
rare, earned, and remembered: the settlement hears who died of
sickness.

- `c611852` 0.8.0 Illness & Medicine: Health component, four vectors,
  hold-then-recover, medicine at the stall
- `f766bc2` Log trims: session archive, probe gate, quiet walks
- `9ca9e31` Contagion tuned 0.05 → 0.03
- `c3e837b` **The market-cure fix**: retune + the hunger counter-toll

**Verified in-game on a natural radstorm day:** 73 medicine buys, 4
sick-but-broke resters, 0 deaths. The retuned curve is non-lethal for
mild cases (radstorm, food, contagion recover; only an untreated wound
crosses the death line late), the hunger counter-toll empties the sick
mind's belly mid-hold so it walks to the market where the medicine
lives, and the co-save restores a healthy world correctly. The
108-death day-11 outbreak — the old INI's lethal defaults (0.01/s
severity over a 120s hold) plus a fatigue coma that parked the sick at
Rest — is gone.

---

## 0.7.9 — bugs & polish (2026-08-13)

The clean run into Illness. The codebase audit found **no bugs**: all INI
defaults match the code, all comments reference correct versions, no
stale TODOs beyond the CoSave UID placeholder (a release-time concern).
Version bumped to 0.7.9; the banner stamps the git hash so the log
always names the DLL that ran. Docs swept and consistent (Roadmap,
README, AdapterProject, DecisionLog). 23/23 harness suites green.

## 0.7.8 — visible children: runtime pairing (2026-08-13)

The adapter now pairs grown sim-only children with real game child
actors — no patch ESP needed. `PairVisibleChildren` scans the game's
process lists for `HumanChildRace`/`GhoulChildRace` actors, filters out
actors already translated into minds, collects the sim-only children
(`Species::Child`, no FormRef), and pairs them greedily — one actor per
child. Each paired child gets a FormRef + translator entry → walks,
trades, bonds like any mind. The external Baby Sim mod (Nexus 100934)
is **usable now** — its children are found by race, not FormID — while
*editing* it or shipping it as a hard requirement waits on the author's
permission (already requested). Without the mod the scan finds nothing
and does nothing: graceful degradation via `sim.birth.visible`
(default off). Children stay sim-only — the 0.7.7 behavior — until the
mod is installed.

## 0.7.7 — babies: the birth lifecycle made whole (2026-08-13)

The instant-birth proof became a journey. A `Pregnancy` component
(conception day, due day, parent IDs — co-save serialized) and a
`BirthDay` component (the child's birth stamp) drive it: spouses
conceive on a day-roll (`sim.birth.chance`, default 0.05) once bonded
and fed, the due day is conception + `sim.birth.gestation` (default 3
sim-days), the birth fires on the due day with gossip and news, and at
`sim.birth.childhood` (default 10) the child's species moves Child →
Human — needs normalize and it walks to market like any mind. A species
gate keeps reproduction human-only (Servomech Swarmbot couples no
longer conceive). The day-261 crash — `RemoveComponent` inside
`ForEachWithComponent` invalidating the iterator — is fixed with a
two-pass CheckBirths (collect due mothers, then create), plus a
dead-parent safety that skips a birth if either parent is gone.
Verified in-game: 9 children born in the first test, 14 more
expecting.

## 0.7.6 — the fight-feel pass: the kick is real (2026-08-13)

The two 0.7.5 presentation bugs closed. **The kick plays for real** —
previously the "kick" was the stagger flinch: the vanilla
`PairedFrontPushKick` IDLE records carry conditions (raider-only,
`RaiderRootBehavior.hkx`) and `PlayIdle` refuses them outside combat.
The fix is our own 380-byte ESP,
`data/TheLivingCommonwealthAnims.esp` — unconditional IDLE clones of
the proven crowd-mod recipe (same subrecords, `MeleeBehavior.hkx`,
no conditions, byte-verified) — resolved load-order-independently and
played on the attacker, on the first shove AND the retaliation. Missing
ESP falls back to the flinch — nothing crashes. **The fall tips instead
of sliding** — `sim.fight.push` drops 8 → 3 (the tip-over zone measured
in the loop tests) so the knock only puts the victim down; the paired
push is the shove. And the **IsDown guard** waits for both actors to be
on their feet (500ms re-check) before a fall or the retaliation fires,
killing the both-fall look. The push/punch variety experiment
(kick/body-slam/push) was tested and reverted — the kick alone reads
right.

## 0.7.4 verification — the log flood tamed (2026-08-12)

### 0.7.5 — the scuffle reads as a sequence (2026-08-12)

The loop test showed the exchange wrong: no visible push, both bodies
hitting the deck at once. Three fixes: the punch's force moves to 5
(the user's INI had 3, which plays a collapse, not a shove); the
retaliation is now a beat later (sim.fight.retaliation.delay, default
4s) via a pending queue — push, fall, get up, push back — instead of
both shoves in one instant, and the forced loop's pair always answers
so the chain is watchable on demand; and the loser slinks off — a new
Movement::WalkAwayFrom walks the one who threw first to the far side
of the scene from the one who answered (the engine's Flee action is
still a stub; this is the adapter's first visible flee, for sim fights
too). The scene gate stays: a parted pair never ghosts a punch.

### 0.7.5 — the shove logs its receipt (2026-08-12)

The loop's close-range fights read as a double-fall with no visible
shove — and zero retaliation lines in 37 fights proved it was the
game's physics (the victim's ragdoll clipping the adjacent aggressor),
not two punches. Every physical punch now logs its receipt: `shove:
<pushed> pushed by <puncher> — <force> force at <distance> u.` — so a
fall can always be matched to its push or proven not to be one. The
base force moves 3 → 4 (3 read as a tip-over in testing), the jitter
spreading 3–5 — a shove that visibly registers.

### 0.7.5 fix — the shove is a bench scene: no more ghost falls (2026-08-12)

The force-test loop exposed it: a fight can book between minds that are
far apart, and KnockExplosion uses the aggressor's position as the
knockback origin — a distant origin threw the victim by a ghost, a fall
with no one near. The physical shove is now gated on proximity (within
400 units — the bench scene): adjacent, the shove and its retaliation
fire exactly as before; at range, the shove waits (logged "brawl at
range (N u) — the shove waits for the bench") while the fight still
books — the feud, the gossip, the news are sim-level truth and stay.
The sim's own fights are untouched (they require a bench crossing); the
force-test loop is now safe to watch from anywhere.

### 0.7.5 — the test hook: a pinned pair brawls on demand (2026-08-12)

sim.test.forceFight pins two minds (by low-24-bit form id) and brawls
them on a loop every sim.test.forceFight.interval seconds — the full
fight machinery on demand: Combat memories, deepened feud, gossip,
threat, the shove with its jitter, the retaliation, the fight line, and
the news (marked "(test brawl)"). The pair is pinned to an Enemy bond
and the once-per-day gate is bypassed (BookFight's a_force) so the loop
repeats without a day roll; the aggressor alternates so the shove lands
on both sides; the species belt still holds. Off by default. Sturges
(0001A4D8) vs Jun Long (0001A4DB) is the ready-made test pair.

### 0.7.5 fix — the Workers get names at last (2026-08-12)

The naming fix landed earlier, but the Sanctuary work crews (the game's
own "Worker" NPCs, e.g. 000FA7F2/000FA7F6) still read "Worker": their
minds were seeded before the generic list grew, so the sim's stored Name
was literally the word "Worker" — and the per-second sweep wrote the
stored name back to the game as-is, never re-deriving. The naming sweep
now converges a Human whose stored name is itself a placeholder: a
"Worker" or "Settler" persisted from a pre-fix save is replaced in
place with a real procedural name before the write-through. NamesTest
now asserts "Worker" is generic, locking the regression.

### 0.7.5 polish — every shove lands a little differently (2026-08-12)

The fight's shove force is now the base sim.fight.push scaled by a
deterministic ±25% jitter off the victim's id (and the retaliation off
the aggressor's) — the same pair's brawl reads the same, a new victim
reads new, and a restored fight shoves exactly as the original did. The
frequency default stays sim.fight.chance = 0.1 — rare and earned (the
temper line at 1.0 means only the churlish half is eligible, so roughly
one in twenty enemy crossings turns to blows). The INI documents the
feel (3 = shove, 5-6 = solid stagger, 10+ = ragdoll) and the presets
(0.1 drama, 0.25 lively, 1.0 test knob) so it can be dialed without a
rebuild.

### 0.7.5 fix — the companion's dating pool is closed (2026-08-12)

A companion dismissed to a settlement becomes a mind — trading, naming,
befriending, feuding — but must never romance a settler. The game
applies HasBeenCompanionFaction (0x000A1B85) permanently at recruitment,
and a companion can only reach a settlement by being dismissed there, so
the faction is the exact signal. A CompanionTag marker rides each mind
(co-saved, re-derived every second like the species — a pre-fix save
heals), and both bond channels refuse it a sweetheart or spouse — capped
at Friend — while enemies stay enemies: friends and feuds are fine, the
dating pool is closed. On load: `companion: <name> is a companion —
friends and feuds, never romance.`

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
