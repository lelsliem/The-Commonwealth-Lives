# The Living Commonwealth — Decision Log

We record why, not just what. Every architectural decision is logged — the
core's convention (ADR-0011 there; the same discipline here). Once accepted,
a decision only changes if a better alternative exists.

---

## 0001 — Build System: xmake

**Accepted · 2026-08**

The adapter builds with **xmake**, not CMake. CommonLibF4 (the current
libxse generation) has no CMake support — it is an xmake project — and its
plugin rule (`commonlibf4.plugin`) generates the entire plugin contract:
`F4SEPlugin_Version` data, the Windows version resource, and the
`Data/F4SE/Plugins` install layout. Fighting that with a hand-rolled CMake
wrapper would be worse on every axis (Law 001: simple things).

The core stays CMake. The adapter does not adopt the core's build system;
it *drives* it — the `lce.core` rule configures and builds the core into
`Build/core` and links `LCE.Core` + its spdlog backend statically. `xmake`
remains the one command.

*Four questions:* simpler — yes (the ecosystem's own tooling); belongs —
yes (it is the ecosystem's standard); needed — yes (CommonLibF4 2.x only
builds this way); helps — yes (the handshake is exactly what this stone
proves).

## 0002 — Dependencies: local clones, pinned by provenance

**Accepted · 2026-08**

Dependencies come from the **local clones in `Depends/`**, not from
FetchContent/xrepo. The clones were moved into this tree deliberately and
are pinned by their history to a build whose `RUNTIME_LATEST` is
**1.11.221 — exactly the project's game runtime**. That match is the whole
point: the plugin declares `CompatibleVersions({ F4SE::RUNTIME_LATEST })`
and must never drift from the game it runs in.

The clones are **gitignored** — they are third-party trees with their own
`.git` directories and GPL provenance, and committing them would poison the
repo. `Depends/README.md` records what each is and how it was obtained.
The one unavoidable network fetch is commonlib-shared's own spdlog package
via xrepo (first build only); documented in the README.

*Alternative considered:* FetchContent/xrepo of CommonLibF4 (the core's own
pattern, and the doc's stated preference) — rejected for now because the
pinned local copy already matches the game runtime, and offline builds work.

## 0003 — Runtime: static CRT everywhere

**Accepted · 2026-08**

Static MSVC runtime everywhere — CommonLibF4, commonlib-shared, and the
plugin all build with the static CRT, matching the core
(`MultiThreaded$<$<CONFIG:Debug>:Debug>`) and the F4SE plugin ecosystem.
Mode-aware: debug builds use `/MTd`, release `/MT` — the core's Debug libs
are `/MTd`, and mixing static runtimes fails to link (LNK2038, discovered
on the first link and fixed here). The core decides the setting; everything
then follows. xrepo packages (spdlog) pick the same runtime automatically.

## 0004 — Logging: through LCE's API; eyes via REX::LOG

**Accepted · 2026-08**

The mod logs through **`LCE::Logging`** (the documented channel — never
spdlog directly; ADR-0030 in the core) **and** `REX::LOG` for the visible
F4SE log file. LCE's logger is console-bound (`stdout_color_mt`) — inside a
game process that is invisible — so `REX::LOG::Info` is the eyes
(`My Games/Fallout4/F4SE/TheLivingCommonwealth.log`) and LCE's APIis the contract. If the core's logger ever gains a file sink, the adapter's calls
do not change.

**One spdlog in the plugin.** The core builds spdlog compiled into
`LCE.Core.lib`, and commonlib-shared links its own spdlog (xrepo) as a
public dependency. The adapter links **only the xrepo copy** and lets
LCE.Core's spdlog references resolve against it — linking both put two
spdlog implementations in the DLL (duplicate symbols, first link failed).
Version skew (core 1.17 vs xrepo 1.16) is API-compatible for what LCE
uses and disappears when the core's logging gets a file sink or a
header-only option; noted so it is revisited, not forgotten.

## 0005 — Target: runtime 1.11.221, address library, next-gen F4SE

**Accepted · 2026-08**

The plugin targets the **next-gen runtime 1.11.221** (the game installed on
this machine; also the vendored CommonLibF4's `RUNTIME_LATEST`). It uses
the **Address Library** (`UsesAddressLibrary(true)`) — required in-game,
standard practice, and what keeps the plugin working across runtime
patches. F4SE itself is a runtime-only dependency: CommonLibF4 replaces it
as the static dependency, exactly as its README states.

## 0007 — Core pinned to 0.4.0+ (the snapshot API)

**Accepted · 2026-08**

The `lce.core` rule verifies the core checkout's `Version.h` and refuses
anything below 0.4.0 — the adapter's co-save stone stands on the snapshot
API (`RegisterSerializer<T>`, `Capture`, `Restore`, `Clear`), which landed
in 0.4.0. A stale checkout fails loudly at configure time with a clear
message rather than silently building against the wrong API.

The snapshot contract, as the core states it: a type with no serializer is
not persisted; the snapshot is a **process-local** exchange (the adapter
owns the durable F4SE co-save record, its type names, and its versioning);
`Restore` requires the same registrations; `Clear` keeps them. The core's
Snapshot suite (14/14 green) proves the round-trip — the farmer still goes
to market after save/load.

## 0008 — Translation stone: the settlers wake up

**Accepted · 2026-08**

On GameLoaded the adapter translates every loaded settler into an entity
inside the core's registry: a `FormRef` (the entity knows its game form),
seeded `Needs` (all satisfied), empty `Memory` and `Relationships`. The
predicate is **WorkshopNPCFaction membership** (`0x000337F3`, verified by
parsing the game's own `Fallout4.esm` — the often-cited
"WorkshopSettlerFaction" does not exist in the base game). The game is
read once and never written — the value↔ActorValue write-through belongs
to the executor stone (ADR-0024).

Serializers are registered once at init for every persisted type (Needs,
Memory, Relationships, Goals, Intent, FormRef), exercising the core's
0.4.0 snapshot substrate for the first time. The adapter's first test
harness (3/3 green, no game required) proves the translator tables, the
seeding, and a full Capture/Restore round-trip through the adapter's own
serializers.

## 0006 — Versioning

**Accepted · 2026-08**

The adapter versions independently, starting at **0.1.0** for this scaffold
stone. The core is 0.3.1 and its 0.4.0 milestone births this project, but
the adapter is its own artifact with its own milestone rhythm: on milestone
completion, bump the version in `xmake.lua`, the README badge, and the
plugin version data (generated from `set_version`), then commit after the
author approves.

## 0009 — The census scans persistent cells, not the REFR form array

**Accepted · 2026-08-10**

Fallout 4 never populates `TESDataHandler::formArrays[REFR]` the way
Skyrim does — `GetFormArray<TESObjectREFR>()` returned 0 across retries
in-game (two world starts), so the per-settlement census silently ran
its single-bench fallback for every session since the stone landed. The
census now enumerates each worldspace's **persistent cell**
(`TESWorldSpace::persistentCell` → `ForEachReference`) and keeps refs
whose base is the workshop workbench (`000C1AEB`): every settlement
workbench is a persistent ref, and persistent refs are loaded at world
start, so one pass finds all 28 markets with valid positions. Verified
in-game: `13 worldspaces, 28 workshops known`.

## 0010 — DeleteGame never tears down a running world; the walk probe reads data positions

**Accepted · 2026-08-10**

Two session-killing bugs from the Diamond City test, both fixed at the
source. `kDeleteGame` fires when a save FILE is deleted — an autosave
rotation did it mid-world — and the handler called `EndWorld`, killing
the sim with no pending load armed to revive it (later saves wrote 0
entities until a full restart). It is now a no-op: a new game or a load
owns the world's life through PreLoadGame/GameLoaded. And the walk probe
read the actor's 3D node world transform, which lies for streaming
actors (post-fast-travel walkers measured 120,000+ units from where they
stood, so arrivals never registered); it now reads the actor's **data
position**, skips readings beyond a sanity gate, and the session timeout
grew to 120s so slow walkers across the market radius can finish.

## 0011 — Deaths are detected by the census sweep, not an event hook; the core gained InteractionKind::Death

**Accepted · 2026-08-10**

Stone 1 ("the world keeps its books") needed to know when a mind is
gone. Two candidate signals: a game death event (none is exposed in the
dependency — commonlibf4 has no Character/actor death event source), or
a poll. The poll won: the tick's existing one-second census reads the
game's own markers — a killer handle (`myKiller`, set the moment an
actor is killed), the corpse-cleanup timer (`checkMyDeadBodyTimer`), or
a deleted ref — all offset-pinned members in the dependency, no
hard-coded form ids. The classification is a pure, tested function
(`Lifecycle::Diff`: unknown+relevant → arrival; known+dead → death;
known+alive+out-of-faction → departure), keeping the game read at the
edge like every other ADR-0024 seam.

The death fact needs a way to say "died" in memory; the core's
`InteractionKind` had none. The adapter asked for one small append-only
enum value (`Death`, ordinal 11 — the co-save writes raw ordinals, so
old saves decode unchanged). It is a fact, never a door: Decide gates
only Trade and Social, so a death never blocks a walk or a trade. The
settlement remembers who is gone; the dead are simply absent and never
restore. Grief (Stone 2) reads the fact.

**Hardening (same day):** the raw marker reads only happen on a fully
streamed-in actor — a 3D node must exist first (a corpse keeps its 3D,
so real deaths still register). In the first in-game run, actors entered
the process lists before their members were initialized after a 665-mind
restore, and the unchecked read booked 11 false deaths in one frame,
3 s after the restore. The gate turned that into 0.

**Hardening, round two (same day):** two-pass confirmation was not
enough — the spawn burst after a big load reads the same actors dead on
their very first sighting for ~2 s (deterministic: the same two
persistent Sanctuary settlers, three builds, three loads), so a second
pass merely confirmed the artifact. The rule now: a death is a
transition, so a mind must have read **alive at least once** before it
can be booked dead. Never-alive dead reads are parked forever and
un-park the moment the actor reads alive.

## 0012 — Bonds are derived adapter state: two channels, one derivation; mutual and sticky

**Accepted · 2026-08-11**

Stone 2 ("bonds") needed named relationship states — friend, sweetheart,
spouse, rival, enemy — built on the core's Disposition/Trust numbers.
Two questions drove the design.

**Where does the bond live?** The core holds dispositions and trusts;
the *name* is the adapter's vocabulary (the core's `BondThreshold` is a
name/value pair precisely so the world names its own lines). The bond
book (`Bonds::BondMap`, keyed by unordered entity-id pair) is adapter
state, persisted like the stall-keepers — the co-save translates it to
form ids at the edges (v5: form A, form B, kind ordinal, since-day). A
v4 save loads with an empty bond section and the 1-second pass
re-derives bonds from the restored relationships.

**How does a bond change?** Two channels feed one derivation
(`Bonds::ApplyPair`), so they cannot disagree. The event channel (Request
A — the core's `RelationshipChangedEvent`, edge-triggered on
`sim.bond.threshold.*` crossings) is instant: the adapter re-derives the
pair the moment a line is crossed. The 1-second `ReconcileBonds` pass is
complete: the core's drift is deliberately quiet — a bond cooling below
its line is a dissolve, not an event — so only the pass sees dissolves,
restores, and anything the bus missed. Whichever channel detects a
change first says it once; the other finds the pair resting.

**Mutual and sticky.** The pair's shared disposition is the *minimum* of
the two directions — both must feel it (one-sided warmth is not yet a
bond; spouse's "(mutual)" in the plan is the min rule at +0.8). And
formation is immediate while dissolution is sticky: a bond persists
until the pair falls halfway back from its line (friend +0.3 dissolves
below +0.15). Without the stickiness, drift (0.05/s toward neutral)
would dissolve a fresh +0.31 friendship within a second of its formation
line — every "became friends" instantly followed by "no longer friends".
Same-family downgrades wait for the sticky dissolve; only a family flip
(friends turned rivals) changes the bond outright. This is hysteresis,
chosen deliberately and documented in `Bonds.h`.

## 0012 addendum — bonds need the buyer's warmth and a slower clock (two in-game discoveries)

**Accepted · 2026-08-11 (the first in-game bond test)**

The first test of Stone 2 ran clean — v0.6.0 banner, tuning loaded, 663
minds restored, the market trading — but no bond formed. Reading the
log against the core's source showed why, two independent gaps:

1. **Trade warms trust, never disposition.** The core's `Trade` outcome
   only touches Trust (+TrustGain); only Aid/Social (+DispositionGain)
   and Wronged/Combat (−DispositionLoss) move disposition. So a buyer
   trading with the keeper grew *trust* but their disposition toward the
   keeper stayed 0 forever — and the bond derivation takes the pair's
   minimum, so min(0, keeper's warmth) = 0, no bond, ever. The fix is
   the buyer's half: on a successful trade the buyer also
   `Remember({keeper, Social})` — a meal at the bench is company, the
   courtship's raw material (Life.md), and Remember publishes the
   crossing on the bus so the instant bond log fires. The keeper's half
   was already there (RecordSale).

2. **The core's drift default erases feelings between meals.** Drift is
   exponential toward neutral at DriftRate — 0.05/s is a ~14 s half-life,
   tuned for the 0.3.0 fast demo. With the living rhythm (a meal every
   ~8 real minutes), any warmth was gone within a minute of the meal.
   The adapter's world now runs the same slow clock the shipped INI sets
   (`sim.drift.rate = 0.0002`, ~1 h half-life), injected when the config
   names none — the adapter's defaults ARE the living-world defaults,
   like the bond lines. Four shared meals cross the friend line.

Both are tuning/wiring decisions, not core changes: the core's
`sim.drift.rate` key was already the designed knob, and the social half
of a trade was always the plan — the adapter had simply never wired it.

## 0013 — Households are derived state: the marriage rides the bond map, the wallet rides the pouch component

**Accepted · 2026-08-11**

Stone 3 ("couples share one pouch, one stall, the same bench") needed a
way to persist a household. Two candidate homes: a new co-save record
section (v6), or derived state built from what already round-trips. The
derived option won, and no record bump was needed:

- **The marriage rides the bond map** (co-save v5): a household *is* a
  Spouse bond — the deepest line, mutual +0.8. When the bond change
  handler sees a pair cross into Spouse, it forms the household
  (`FormHousehold`): the two pouches merge into one on the deterministic
  lower-id holder. When the bond dissolves, the wallet splits
  (`DissolveHousehold`: holder keeps the remainder, the other takes
  half).
- **The wallet rides the `CapPouch` component**: a merged household is
  simply one member with a pouch and one without. `PouchOf` resolves a
  married member's pouch to the spouse's on both sides of the bench, so
  the shared wallet round-trips with zero new bytes.
- **The invariant is the schema**: one pouch per married pair, one per
  unmarried human. `Enforce` repairs it silently on restore (a saved
  marriage is already merged — no-op; a pre-household save's spouse pair
  with two pouches merges on the first pass) and defensively each second.
  A dead spouse's pouch passes to the widow(er) in `RemoveMind` before
  the bond is erased.

The alternative — a v6 household record — would have duplicated the
spouse bond (already persisted) and the pouch (already a component);
derived state keeps one source of truth and heals any drift by
construction. What this stone deliberately does NOT do: the shared *bed*
(no rest intent exists in the adapter yet) and walking to market
*together* (no path coordination) — both documented as deferred in
Life.md, waiting on companionship/rest intents of a later stone.

## 0014 — The sleep cycle: Rest recovers Fatigue, because the need loop only decays

**Status:** accepted (2026-08-11) — built, 13/13 suites green.

**Context.** The 24-hour market test (2026-08-11) exposed a parked world:
seven trades, seven different buyers, zero repeats — every customer ate
once and went silent (`decides Rest (0.98)` and nothing after). The
cause: the engine's need loop only *decays*; nothing ever *restored*
Fatigue. Only Hunger was restored, and only on the meal. So the moment
a mind ate (Hunger → 1.0), its drained Fatigue (0.0 after a long
session at any decay rate) became the most urgent need → Rest — and
Rest was a table slot that did nothing. A fed mind parked in Rest
forever, never hungry again, never walking, never sharing a second
meal with anyone. Without repeated meetings, no bond could ever form —
the household verify's dead end was not the economy or the market
hours, it was the missing half of the need loop.

**Decision.** Rest is the recovery side. A mind whose last intent was
Rest recovers its Fatigue need at `sim.rest.recovery` per second
(default 0.2/s — a full nap in ~5 s; INI-tunable, adapter-default when
the config names none, exactly like the drift and bond lines). The
recovery runs in `Adapter::Tick` *before* `Update`, so this tick's
decisions see the rested mind: hunger is most urgent again → `MoveTo`
→ walk → trade → meal → Rest → recover → repeat. The same pair finally
meets repeatedly, and bonds can form.

**Why this shape.**
- **Pure + testable:** `RestRecovery(Needs&, rate, delta)` in
  `Behaviour.h` next to `RestoreHunger` — capped at 1.0, -1 sentinel
  when a mind somehow lacks Fatigue (defensive; all seeds have one).
- **Driven by the intent, not a timer:** keying off the stored Rest
  intent means a mind recovers only while it actually rests, and stops
  the moment `Update` re-decides it to MoveTo. No separate rest
  bookkeeping to corrupt or persist.
- **No new record:** Fatigue is already a component in the co-save; the
  recovery rate is tuning, not state. No record bump.
- **Fixes the living world too:** at any decay rhythm, Fatigue reaches 0
  given enough sim time and nothing restored it — the whole settlement
  would eventually park and only fresh arrivals would move. The sleep
  cycle is the missing half of *every* mind's loop, not just the demo's.

**What this stone deliberately does NOT do:** the game-side bed (a
resting mind still stands in place — the Rest table slot executes
nothing in-world) and walking to market *together*. Both stay deferred
(companionship/social stones); the sim-level recovery is the observable
half, and SleepCycleTest pins the exact bug: fed + drained-fatigue →
Rest; after a nap → MoveTo to the remembered market.

## 0015 — The review pass: rest rescues parked minds; the walk layer is fixed; the log tells the truth

**Date.** 2026-08-11. The review of everything through the sleep cycle
(26b7805) surfaced one structural gap, two tunability gaps, and several
log/doc contradictions; this decision records the fixes.

**The structural gap.** The sleep cycle keyed its recovery on the stored
Rest *intent* — but the engine's Decide returns `nullopt` when the most
urgent need is Safety with no remembered threat (you can't flee from
nothing), and `Update` then *removes* the intent. A Safety-drained mind
is therefore invisible to any intent-keyed pass forever. The recovery
now reads the *needs*: a mind with no intent whose most urgent need is
Fatigue or Safety is resting, recovers, and rejoins the loop.
`RestRecovery` restores Fatigue, Safety, and Comfort (a nap fixes all
three; Social stays decay-only — that is the future Socialize stone's
job). `MostUrgentNeed` mirrors the engine's internal MostUrgent (lowest
value, first-in-list wins ties) so the adapter asks the same question
the engine answers. SleepCycleTest gains the parked-mind rescue.

**The walk-layer discovery (26b7805, folded into this decision).** The
sleep cycle worked but its payoff was swallowed by the walk session: on
arrival the session stayed in the walk table for the full 120 s timeout,
holding one of the 16 walk slots, and the "already walking" check then
swallowed the fed mind's re-walk to the bench it was standing at —
7 trades, 7 buyers, zero repeats, no bond could form. Arrival now ends
the session (slot freed immediately) and a Reached session is a new
trip.

**Tunability.** The walk cap is no longer a constant: `sim.walk.cap`
(default 16) in the INI, parsed separately from the float rates because
it is a size. The launch banner now prints all five need rates plus the
cap, so the log always says which rhythm actually ran (the hunt was
started by a hunger INI that read as if it were 0.002/s — the banner
never showed the rates).

**Decision.** Rest is the recovery side for every need it plausibly
fixes, and the recovery pass answers to the needs, not the intents,
because the engine can silence a mind entirely. The walk cap is tuning.
The banner and the docs tell the truth about both.

**Costs and deferrals.** Restoring Safety/Comfort on rest is a
simplification (one rate for three needs); the Social need and the
game-side bed remain deferred as before. The `(defaults)` markers on the
tuning lines are a nicety, not a guarantee — the printed values are the
truth.

## 0016 — The walk layer must not flood: per-market budgets and the arrival cooldown

**Date.** 2026-08-11. The 0.3/s hunger tests (fast demo) exposed two
floods that the earlier walk fixes (ADR-0015) had left standing.

**The per-market cap (59d0caa).** ADR-0015's cap logging worked too
well: in the 600-mind revival world every hungry settler decides MoveTo
every frame, so the per-entity deferral line wrote 5.1M log lines in
five minutes — and the single global 16-slot cap starved every market
(two Sanctuary trades, then nothing). The cap is now a **per-market
budget**: each market's walkers share its own slice (16), counted by
walking sessions targeting that form, so one settlement's hunger can no
longer be starved by the Commonwealth-wide flood. The deferral log is
one aggregate line per pass, rate-limited to every 5 s.

**The arrival cooldown (1237baa).** Even per-market, a fed mind standing
at its market is always the most-urgent-hungry need at fast decay rates
(0.3/s vs 0.002/s — 100×), so it re-decides MoveTo every frame, the walk
layer re-issued the trip it just completed, the arrival fired instantly,
and the arrival → feed loop ran 18k times in under a minute (the dog was
fed every frame: `Hunger 1.00 -> 1.00`). Arrival erases the session
(ADR-0015 — the slot must free), so the walk table alone cannot answer
"was this mind just here?". A new `m_ArrivedAt` map (entity → target +
time) records each arrival; the MoveTo branch treats a mind that arrived
at this target within 10 s as satisfied — no new walk, no arrival, no
feed, no spam. One feed per cooldown per mind, bounded and quiet.

**Decision.** The walk layer is the adapter's throttle on the game's
movement flood; it must be bounded per destination, and a completed
arrival must answer the "just here" question even after its session is
gone. Both are tuning/state, not new records — no co-save bump.

**Costs and deferrals.** A 10 s cooldown lengthens the meal cycle for a
mind standing at the market (bounded, predictable); the cooldown is a
constant, not yet an INI key. The observed 7–10 min meal gaps for
settlers who wander (Rest/Explore are table slots; the game's sandbox
moves them) are a separate gap, deferred to a game-side Rest/Explore
execution stone.

## 0017 — The rest of 0.6.0: meal-cadence, gossip, arcs, birth

**Date.** 2026-08-11. Stones 1–3.5 closed (ADRs 0011–0016); this pass
builds the remaining 0.6.0 stones — 4 (gossip), 5 (arcs), 6 (birth) —
plus the game-side Rest/Explore execution the marriage test exposed
(ADR-0016's deferred gap). Harness 13/13 → 16/16; in-game verification
of this pass is pending.

**The meal-cadence hold.** ADR-0016 deferred the 7–10 min meal gaps:
a fed mind that decides Rest or Explore got nothing in-game (both were
table slots), the sandbox wandered it away from its bench, and the next
shared meal was minutes off instead of the cooldown's ~10 s. The fix is
`Movement::HoldPlace`: the same verified command-mode travel package,
targeted at the actor itself, commands it to "travel" to its own
position — the package parks the actor and suspends the sandbox.
Rate-limited (one hold per mind per 10 s — a command package every
frame is a flood of its own). Walk, rest, and explore are now all real
game commands.

**Gossip (Stone 4).** `Gossip.h`, pure: a bond crossing, a feud, or a
death writes a fact into every mind of the settlement. The "gossip
radius" is the settlement itself — one book per settlement; strangers
and fresh arrivals never hear it (written once, never replayed). Wired
into `OnBondChange` (formation names both participants) and the death
bookkeeping in `RemoveMind` (the fact the grief arc reads).

**Arcs (Stone 5).** `Arcs.h`, pure. The Feud: `Mediate` runs once per
day (adapter's `RunMediation` on the day cadence) over every enemy pair
in the bond book; a third mind who has heard of both sides steps in — a
mediator both sides like cools the feud a step toward zero and earns
their trust, an unloved meddler is told off. The Grief: `Grieving`/
`ApplyGrief` — a fresh death memory of someone at/above the friend line
drains the survivor's Social at `sim.arc.grief.decay` extra per second;
they seek company. Both are derived from persisted components (memory
events, relationships), so they survive save/load with no record. The
courtship is visible now (shared meals are it); departure and famine
stay sketched in Life.md.

**Birth (Stone 6, experimental).** `Birth.h`, pure: a spouse household
has a child — a **sim-only mind**, no game actor, no form, no
translator entry. Seeded like any mind, warm to both parents (and the
parents to it), fed by the household every tick (never walks to a
market). One birth per sim day at most, the household the Rng draws;
gated by `sim.birth.enabled` (default 0). The census cannot evict it —
`Lifecycle::Diff` classifies the census, never the registry.

**No co-save bump.** A child is an entity carrying existing components
(SpeciesTag, Needs, Memory, Goals, Relationships) — all already
serialized. Gossip and arcs are derived, not stored. Bonds and
households already ride v5. This is the same "derived state, no record"
shape as households (ADR-0013).

**Costs and deferrals.** A held settler stands in place (no
furniture/animation — game-side polish). Socialize/Work/Flee remain
table slots. The grief's vengeance/comfort fork and the departure and
famine arcs remain sketched. The 10 s hold cooldown is a constant.

**Addendum — the polygamy edge (same day, first in-game run).** The
birth test's session produced a second marriage for the Sanctuary
keeper while its first was live — the bond layer honestly reads Spouse
to two minds (bonds are pure derived disposition; a beloved settler
crosses +0.8 with two people). A second `FormHousehold` merged the
second spouse's pouch into the shared wallet, and a later 2-way split
would have silently vanished the third member's caps. Fixed at the
household layer, which stays monogamous by construction:
`Households::InHousehold` (derived from the components — scan every
spouse bond, shared iff exactly one pouch exists; order-independent,
restore-proof) guards both callers: `OnBondChange` refuses to form a
second household (the second marriage stands as a bond — the family
bench still feeds both spouses — but the wallets stay personal), and
`Enforce` (the silent per-pass repair) skips the same pairs, so the
invariant can never be re-merged behind the events' back. No co-save
bump — the guard reads components the v5 world already persists.

**Addendum 2 — the hold froze the settlement; the wander replaced it
(same day, first in-game run of 27141de).** The 10 s commanded hold
worked — meals collapsed to the cooldown cadence — but it looked dead:
every resting or exploring settler stood frozen at the bench (the
birth test's session). The movement intents were narrowed from the
start: Rest and Explore are *wanders*, not *stands*, and the game's
own sandbox is the best idle animation engine there is. `WanderNear`
(1.11.221, the same byte-verified travel pin) commands the actor to a
real, nearby reference in its own cell — furniture preferred, then any
non-actor object within the radius — via `TESObjectCELL::
ForEachReferenceInRange`. On arrival the sandbox resumes until the
next command (rate-limited to one wander per mind per 30 s, so a
re-issue never yanks a mid-walk actor), playing the game's idles — a
settler may even sit at the bench it walked to. The meal cadence
holds: the sandbox cannot drift the actor away before the next
command, and the market fact re-seeds every second regardless. Empty
cells fall back to `HoldPlace` — parked, never teleported. The travel
issue was refactored into a silent `IssueTravel` helper so the wander
(a command every cooldown across hundreds of minds) never floods the
log; the plan-entry decision line is the narrative. `HoldPlace` stays
as the empty-cell fallback only.

**Addendum 3 — the 0.6.0 truth items (same day).** Three small items
so the remaining stones are *verifiable* instead of silent. (1) The
sim-only population shows in the log: `The Commonwealth wakes up: N
sim-only children born to their households.` (and `restored too` on
the restore path) — the census counts actors, so without the line a
born child is invisible. (2) Gossip's observable half: a death logs
`gossip: N minds remember settler X is gone.` — the count
`Gossip::Spread` returned, one line per death, no per-mind flood. (3)
The wander is tunable: `sim.wander.cooldown` (seconds between
commands, default 30) and `sim.wander.radius` (game units, default
4000) replace the constants, matching the project's every-number-is-a-
key rule.

**Addendum 4 — the restore-birth (same day).** The first in-game run
of the truth items showed a birth one second after every load: the
arcs' day gates (`m_LastBirthDay` / `m_LastMediationDay`) are session
state initialized to the max sentinel ("never ran"), so the first
tick after a restore always fired today's birth and mediation — a
reload was treated as a new day. On restore the gates are now seeded
to `CurrentDay()`: the world has already had its day's chances, and
tomorrow turns them again. The same session also showed why the
children wake-line was absent — the loaded save predated the births
(Capture iterates all live registry slots, formless children
included — verified in the engine's `EntityRegistry::Capture`), so
the line will appear once a save is made after a birth.

**Addendum 5 — the grief test found two real bugs (same day).** The
first in-game grief test killed the Sanctuary keeper's spouse and got
the gossip line but no grief: (1) the announce was dead code — it
resolved the dead's form via `FormFor(dead)`, but `RemoveMind` already
destroyed the entity and removed it from the translator, so the form
was always 0 and the line was skipped every time; the drain worked,
the line could not. A small session map (`m_RecentDeaths`, entity
value → form id, recorded at booking time, cleared on EndWorld) gives
the announce the dead's form. (2) The keeper did not qualify — its
warmth to the dead had eroded below the friend line, because the
family bench feeds the spouse for free but never warmed the couple:
after marriage they stopped trading, and drift (0.0002/s, ~1 h
half-life) quietly killed the marriage's feelings in about an hour.
The family meal is now the marriage's heartbeat: a shared meal at home
warms both directions (Remember(Social) + RecordSale, the same warmth
a bench-sale carries), so marriages stay warm and grief for a spouse
finds the love there.

**Addendum 6 — the grief line, once per bereavement (same day).** The
first real grief announce (ca16aa7) worked — `arcs: settler 0x50976
grieves for 0x2f2a7` — but printed 34 times in half a second: the
fresh window (memory weight ≥ 0.9) is ~0.5 s of frames, and the
announce ran in every frame of it. It is now once per (mind, dead)
pair per session (`m_GriefAnnounced`, cleared on EndWorld — a restored
bereavement re-announces once, cheap and honest). The same session
also showed the death being re-booked (the census re-observed the
corpse alive-then-dead after it streamed into the loaded cell) — the
lifecycle's honest per-session transition, which re-stamped the death
fact and is what let the whole chain fire in this session.
