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

## 0018 — 0.6.0 is complete: the verification legs and the tag decision

**Accepted · 2026-08-11**

Every stone on the 0.6.0 board is now verified in-game, and the milestone
is shipped as tag `0.6.0` (the first non-beta release). The board's last
three legs were closed by direct in-game observation, and the verification
itself surfaced four real fixes, all recorded in ADR-0017's addenda:

- **The meal-cadence wander** (addendum 2): the first hold implementation
  froze the settlement at the bench; `Movement::WanderNear` (a real nearby
  reference in the actor's own cell, furniture preferred, one command per
  mind per 30 s) replaced it. Approved in-game — settlers mill around
  their settlement between meals, and the meal cadence holds.
- **The truth items** (addendum 3): children counted at wake, the gossip
  death line, `sim.wander.cooldown`/`sim.wander.radius` INI keys — the
  sim-only population and the gossip stone became observable in the log.
- **The restore-birth** (addendum 4): a reload was treated as a new day —
  the arcs' day gates are session state and started at "never ran", so
  every load re-birthed and re-mediated. The gates now seed to the current
  day on restore.
- **The grief announce** (addenda 5–6): the line was dead code (the dead's
  form was resolved after its entity was destroyed), and the family meal
  never warmed the couple, so marriages quietly died of drift and grief
  had no love to find. Both fixed; the announce is once per bereavement
  (was 34 lines per frame-window).

The one honest deferral: the feud arc's *organic* appearance. Nothing in
0.6.0 makes dispositions negative, so enemy pairs cannot form in-game —
the arc is harness-verified, and its conflict source is 0.7.0's
"relationships good *and bad*".

The changelog-worthy summary for the release page is: settlers are born,
live, and die; they remember who is gone; they make friends, sweethearts,
and spouses from shared meals — and grieve for the ones they lose.

## 0019 — 0.7.0: identity, conflict, and the player window (2026-08-11)

The engine answered the open question first (2026-08-11, its own
decision record): negative social travels by **kind and result, never
a sign** — `ReportOutcome(Other, Social, Failure)` is −0.1, `Remember
(Other, Wronged)` is −0.25, and `event.Weight` is salience only. The
draft's `Remember({keeper, −Social})` is superseded by the decided
channels; no hand-over was needed.

**The three stones, built together (19/19 harness suites):**

1. **Names.** A `Name` component, additive to the co-save (the record's
   v6 bump is the legacy section, not the component — the format is
   self-describing; old records back-fill on restore, exactly like the
   economy's pouches). The game's own names win; a generic "Settler"
   draws from **gender-split pools** (the actor's sex; an unset sex
   draws from the id) with a shared family list, and **animals draw
   from their own pool, named only when owned** — the ownership rule
   the author asked for (a stray stays "animal [FF0197BF]" until
   someone claims it). The author curates all four lists in the INI
   (`names.first.male/.female/.animal`, `names.last`); a missing or
   broken list keeps the built-in default. Every channel speaks names
   with the console hex beside them.
2. **The conflict source.** A hungry human whose walk lands at a
   **closed market** reports `ReportOutcome({keeper, Social, Failure})`
   — the executed interaction that went badly, the engine's designed
   channel. The **temper line** (`sim.slight.temper`; the engine's
   `JitteredTraits` substrate, deterministic per id) decides who
   blames — a churlish mind cools −0.1 toward the keeper, a warm one
   shrugs it off (a world outcome, memory only). Settlement `Groups`
   (derived from the market memory, never persisted — a restored world
   re-derives) let the echo spread the chill at
   `sim.group.inheritance`, and two or three let-downs cross the rival
   line: the 0.6.0 feud arc — gossip, mediation — finally begins in
   the wild. The echo also warms settlements toward their feeder (a
   sale's +0.1 echoes +0.05); the mutual bond rule keeps that from
   manufacturing bonds (settlement → keeper warms, keeper → settlement
   does not — min wins).
3. **The player window.** World events become one-line news — bonds,
   feuds, births, deaths, market openings — shown as throttled HUD
   notifications and appended to the news feed; a settlement radio
   (base form `radio.base.formid`, default 0x1A17D — **flagged for
   xEdit verification**, configurable so a wrong pin is a config line)
   speaks the feed as captions while near. **MCM is deferred,
   honestly**: the INI already delivers the tuning behavior; the UI
   page needs the MCM mod and the Creation Kit — an author-side asset
   task, not a sim task. Real audio likewise waits for assets.

**The engine's 0.7.0 proved through the adapter:** deaths `Bequeath`
their memories to the household's heir (facts at or above the floor,
scaled) and leave their name as a registry-level legacy (the co-save's
v6 section); births `InheritMemory` from both parents (person-facts
only — the feud travels on the memory channel, the inherited cold
shoulder on the group echo).

**Verify sentence (verified in-game 2026-08-11 — addendum 1 below):**
the log greets the world by name; names survive save/load and fast
travel; a settler is slighted at a closed bench, the settlement's echo
agrees, and a feud begins — no script.

### Addendum 1 — The 0.7.0 verification hunt (2026-08-11)

0.7.0 is complete and verified in-game. The hunt surfaced eight real
finds — six fixes and two structural discoveries — all committed and
pushed (tag `0.7.0`, release live 2026-08-11):

1. **Names were a no-show on the actors** (`7458cb2` → `09194c0`). The
   sim spoke names in the log, but the workshop view still read
   "Settler". Fix: write the generated name onto the actor via
   `ExtraDataList::SetOverrideName` (the same mechanism as the
   console's `SetDisplayName`, persisted with the save) at seed,
   restore, and a per-second sweep that names streaming actors the
   first time they appear.
2. **The reference full-name read was empty for most actors**
   (`09194c0`) — `TESFullName::GetFullName(ref)` reads the sparse-map
   entry, which is empty for nearly everyone, so *everyone* looked
   generic and Mama Murphy, Marcy and Jun were renamed. The name now
   comes from the **base form** (`Actor::GetNPC()`), the species-aware
   generic check treats plain species words as nameless for animals
   ("Dog", "Cat", "Junkyard Dog" — Dogmeat stays Dogmeat), and the
   per-second sweep self-heals: the base form is the eternal truth,
   stale generated stamps are removed. The existing save healed on
   load.
3. **The INI overrode the curated pools silently** — the shipped INI's
   16-per-pool lists shadowed `Names.h`'s curated 82/78/54/75 (family
   tails included), so the curated names never ran. Synced: the INI is
   now the single source of truth carrying the full curated pools.
4. **The junkyard dog lost its name when the base-form fix landed**
   (`65b6686`) — "Junkyard Dog" isn't a species word, so it read as a
   real name and the stamp was reconciled away. It's now a namable
   species label: owned → named from the pool again, stray → nameless.
   Provisioners were briefly "Provisioner <first>" and are reverted to
   the bare role (no cascade to raiders/guards; `b72938d`).
5. **Pets dedup per world** (`b72938d`) — five Bandits restored from
   the co-save; a restore-time pass re-draws unique names. Cross-
   session dedup stays a noted gap (each session only sees its own
   world).
6. **The feud was structurally unreachable — twice.** First the engine
   blocker (hand-over `81cfe48` → core `509a54d`): the market-closed
   fact made Decide refuse MoveTo, so no arrival ever landed on a
   closed bench; the engine shipped `sim.hunger.desperate` (adapter
   ships 0.2) and desperate minds now walk to the shut market anyway.
   Then the adapter's own blocker (`95784aa`, `3a5b2b8`): the feud
   headline only fired on a direct None→Enemy jump (the normal
   rival→enemy path logged "are now enemies"), and mediation ran once
   per day while gossip dies in ~4.5 s (`sim.memory.fade` 0.2) — by
   the next day turn nobody remembered the feud, so no mediator could
   ever be found and the arc silently did nothing (day 271's pass ran
   with a feud in the book and produced zero attempts). Fixes: any
   crossing into Enemy is the feud line + news; the Enemy crossing
   spreads the feud gossip (the "a feud starts" channel Gossip.h
   documented but never wired) and attempts mediation immediately,
   while the settlement still knows.
7. **The banner stamped the pre-fix hash** — the build stamp is `git
   rev-parse HEAD` at build time, and a build before the commit prints
   the *previous* hash (the 19:58 DLL contained the feud fix but
   announced `160d395`). Rebuild-after-commit is now the ritual so
   the banner tells the truth.
8. **The feud arc verified in-game, end to end:** `the stall at
   00054BAE is shut — Paladin Danse went hungry and blames the
   keeper` → rival bonds (direct −0.1 + settlement echo) → `X is
   feuding with Y` → feud gossip → `arcs: Titus Pratt cooled the
   feud between …` (23 feuds, 127 mediation attempts in the stressed
   always-closed session; a clean build restored 673 minds, 78 bonds,
   10 stall-keepers, 4 children with zero errors). HUD notifications
   and radio captions verified on-screen; radio audio honestly
   scheduled after 0.9.0.

**Known cosmetic:** when several feuds form in the same second, each
crossing runs a full mediation pass, so a pair can be mediated twice
back-to-back (a duplicated `arcs:` line). Harmless — the second pass
just cools +0.05 more and self-corrects.

## 0020 — 0.7.0 shipped; the staged run to 1.0.0 (2026-08-11)

0.7.0 shipped as tag `0.7.0` (release live 2026-08-11). The next
decisions were made together with the author, and recorded in
`Docs/Design/ReleasePlan.md`:

1. **The mod is its own product, not just an engine proof.** It is a
   *show*: people visibly living. Everything serves the show, hardens
   the stage, or is cut.
2. **The path is staged** so each piece lands, tests, and ships
   in-game before the next: 0.7.1 Talk → 0.7.2 Rows → 0.7.3 Fights →
   0.8.0 Trade with anyone → 0.9.0 the release gate → 1.0.0 freeze and
   ship.
3. **The cuts (the honest calls):** the "hands" pillar (build / move
   items / destroy — riskiest, collides with the workshop ecosystem,
   and the player wants people, not construction) is out of 1.0.0;
   pex-driven scripted scenes are out (real combat covers fights with
   the game's own animations); MCM is out of 1.0.0 (the INI already
   delivers every knob; the page is author-asset work); audio radio is
   out (captions work; voices need assets); visible child actors are
   out (sim-only children stay).
4. **The only hard gate is scale** (0.9.0): a normal save with hundreds
   of settlers must tick inside a frame budget, verified in-game.
5. **The dialogue pools** were drafted in the author's tone (the good —
   greet/gossip, the bad — row/trade, the ugly — grief/fight/feud) and
   committed to the INI (`dialogue.*`), one-liners only; the row pool
   is the author's five-line verbal→physical ramp. The keys parse and
   sit unused until 0.7.1 reads them.

## 0021 — Illness is a component, not a need (2026-08-11)

The engine's endgame handover named disease as the one accepted new
surface, locked its shape (hold-then-recover: health holds reduced
while ill, climbs at a rate after the recovery time), and asked the
adapter to decide between a new `NeedType` and a fact-plus-tick rule.

**Decision: fact-plus-tick, adapter-owned** — a `Health` component at
the edge, co-save additive like `name`, with the hold and the recovery
driven by the adapter's own tick. Not a `NeedType`:

1. **Health's curve is not a need's curve.** The five needs decay toward
   0 and urgency grows; health holds then recovers. A `NeedType` would
   confuse `Decide`'s urgency model for a value that deliberately does
   not behave like a drive.
2. **Every cause is already an edge read** — radstorms (the weather
   fact rides memory), food quality, Combat wounds, contagion through
   the settlement Groups. The edge owns the vectors; the edge should
   own the state.
3. **The adapter already owns stateful components** (CapPouch, Name,
   SpeciesTag) with the proven co-save pattern. Health slots in.
4. **The cost flows through an existing need** — while ill, the
   adapter multiplies Fatigue decay, so a sick mind tires faster and
   rests more (the sleep cycle already exists). No new goals, no new
   commands, no engine surface.

The engine keeps `Decide` vocabulary-free. The design is
`Docs/Design/Illness.md`; the stone lands as 0.8.1, after Trade with
anyone (medicine is the trade stone's second good — caps for a cure,
so a broke settlement suffers honestly).

## 0022 — Speech is a presentation layer (0.7.1 Talk, 2026-08-11)

The first Real Events stage makes the sim's existing social
interactions *visible*: when a mind trades, eats with its family, or
is slighted at a shut stall, it says a line. Three decisions:

1. **The pools are INI data, not code** — `Dialogue.h` carries the
   author's starter one-liners as defaults, overridable per list
   (`dialogue.*`, comma-separated) with the same contract as the name
   pools: a missing or broken line keeps the default, never breaks the
   world. The words are content; the author curates them without a
   recompile.
2. **The picker is per mind and per day, seeded — not per utterance.**
   The same mind says the same line all day (a greeting you can get
   used to) and a different one tomorrow (the world moves on). Seeded
   like the names (a splitmix fold with the entity, the day, and a
   per-category salt) so two pools never pick in lockstep and nothing
   consumes the Rng stream — the co-save's randomness is untouched.
3. **The sim decides *when*; this decides *what*; silence is safe.** A
   pool that is empty (or a list the author deliberately emptied)
   says nothing — speech never gates a trade or a meal. The line rides
   the news feed, so the settlement radio reads it as a caption; it
   does not pop a HUD notification (speech is quieter than news).

Wired in 0.7.1: the paid trade (buyer to keeper, the `trade` pool),
the family meal (spouse to spouse, the `family` pool), and the
shut-stall slight (the slighted mind to the keeper, the `row` pool —
the first words of a feud, feeding 0.7.2 Rows). 20/20 harness suites
green (DialogueTest pins the pools, the ramp's order, determinism,
the day input, the salt, the INI contract, and empty-pool silence).
## 0023 — The body is buried; the child is a mind (0.7.1 pre-test, 2026-08-11)

Asked before the 0.7.1 test: *when one dies the body never moves —
wouldn't it be buried or disappear another way? and a birth child
never gets spawned — the whole life/death cycle?* Two decisions:

1. **The corpse must not linger forever — planned stone 0.8.2
   (Burial).** The sim's death path is complete in the books (death
   fact, grief, legacy) but the game corpse stays in the settlement
   cell forever (no cell reset). After the mourning window
   (`sim.death.burialDays`, INI-tunable), the adapter disables the
   corpse ref and logs `the settlement laid X to rest` (+ a news
   line). Body state is game state — adapter-owned, no engine change.
2. **A child is a mind, not a body — the honest cut.** Born children
   are sim-only by design: no FormRef, no actor, no game presence.
   Making a child visible requires a Creation Kit child template
   (race, markers, no-combat) — an author-asset stone, genuinely
   post-1.0. The visible story is the household (parents walk, eat,
   trade, grieve; the child is fed, bonded, named, counted at wake).
   Confirmed as a decision, not an accident (ReleasePlan, life/death
   visibility).
## 0024 — A row is a crossing, not a let-down (0.7.2 Rows, 2026-08-11)

The feud's conflict source (0.7.0) feeds a feud; Rows gives it a
scene. Three decisions:

1. **The row is the engine's Wronged channel; the shut-stall stays
   Social/Failure.** A shut stall is an executed interaction that went
   badly (−0.1, verified feud pacing). A row — two rivals or enemies
   who cross paths — is the *unprompted* wrong (−0.25, full loss), the
   channel Identity.md named but nothing yet used. The two channels do
   not mix, so 0.7.0's verified pacing is untouched and the row is a
   new, harsher event on top.
2. **Words once a day, gated by the co-saved memory — not ephemeral
   state.** The once-a-day gate is a Wronged memory of the other
   stamped today (either direction). It rides the co-save, so a row
   survives save/load without ever repeating; the crossing's attendance
   book (who walked to each bench today) is ephemeral and pruned, and
   the co-save never touches it.
3. **The crossing is the bench, and the exchange is pure.** The feud's
   geography is the stall (Identity.md), so the scan runs only at bench
   arrivals. The bookkeeping lives in `Rows.h` (pure, like Gossip —
   no game types, tested in the harness); the adapter supplies the
   words (`Say`) and the log. Physical escalation (a shove, a punch)
   stays 0.7.3, where the game's own combat animates everything.
## 0025 — Own the mind, borrow the polish (2026-08-11)

A Nexus-wide survey (Landscape.md) before 0.7.3: what exists, what we
own, what to borrow. Three decisions:

1. **The core is ours and stays ours.** No mod gives settlers minds —
   needs, decisions, walks, trades, relationships, life and death. The
   relationship/family space is entirely player-facing on the Nexus.
   The release story claims the empty space, not a crowded one.
2. **Don't chase the crowds.** Sim Settlements owns building (the
   "hands" pillar stays cut); a dozen cosmetic renamers own record
   edits (our runtime naming wins on stickiness, species, co-save);
   player-survival mods own the player's needs. Compatible, never
   competing.
3. **Borrow, don't build, the polish.** Settler Sandbox Expansion
   (idle sandbox life), Barter (vendor economy depth for 0.8.0), NPCs
   Travel (road life for provisioners), and voiced settler dialogue
   (the audio path, later) are noted as optional companion mods and
   idea sources. The only dependency pursued: Baby Sim, permission
   pending.

---

### ADR-0026 — The feud's geography includes the keeper; the workshop's props are not minds (2026-08-12)

**Context:** In-game verification of 0.7.1/0.7.2 (the 09:47 session, 673-mind restored world) showed 0.7.1's speech firing perfectly on shut-stall slights but zero formal rows — and the feud headlines that did land were `Deacon is feuding with Missile Turret` (×4) and `Spotlight - Wall-Mounted` (×2). Two real seams:

1. The row crossing scan reads the attendance book ("who walked here today"), so a stall-keeper planted or restored at her own bench never enters it — the very mind every shut-stall slight is aimed at could never row back.
2. The workshop's props (turrets, spotlights) hold the WorkshopNPCFaction — the sim-relevance gate — and ClassifySpecies defaults their races to Human, so they seed as minds: needs, walks (434 active walks in the test session), stall slights, and feuds.

**Decision:** (a) The row scan crosses the stall-keeper directly at each arrival, alongside the day's walkers; the once-per-day Wronged gate makes a keeper who did arrive today a harmless double-scan. (b) `IsSimRelevant` excludes device/robot races (the three turret races, robots, LibertyPrime) and any actor with no NPC base record; `ApplyRestore` runs `PruneDeviceMinds()` after the translator rebuild — before pouches, names, or bonds — so a polluted co-save self-heals quietly (one summary line) and a fresh world never seeds props. Stragglers still streaming in at restore are caught by the census as departures when they load.

**Consequences:** No engine change (edge-only: game knowledge at the edge, ADR-0024). The species table's "robots are deliberately absent — a robot is its own species for a later stone" note is finally honored. Synths stay Human (a synth settler is a person).

**Follow-up (same day):** the diagnostic in the prune announced the wall-mounted spotlight's race — `0x01002804` (DLCRobot.esm/Automatron, record 0x2804) — which joined the device table. A Human-classified mind whose race is neither a known organic race nor a known device now announces itself once per session (a permanent line, the table's way of learning).

---

### ADR-0027 — A role label is a title, not a name: the roles gain people (2026-08-12)

**Context:** 0.7.3 (pulled forward from the 0.8.0 pillar) makes every
unnamed sim-relevant mind individually nameable, because Trade memories
(0.7.4) reference a person — and a world full of identical "Provisioner"
labels cannot tell two provisioners apart. The game-name-first rule kept
the bare role word ("Provisioner", "Guard", "Minuteman") as the mind's
name: correct for a real NPC (Sturges stays Sturges) but a memory
dead-end for the generic roles.

**Decision:** A role label is not a name to hand out. `Names.h` gains
`IsRoleName` (case-insensitive match against the role list: Provisioner,
Guard, Minuteman, Caravan Guard, Trader, Merchant — people only, an
animal's "Junkyard Dog" stays a species word), `HasRolePrefix`,
`GenerateRoleName` (title + a gendered first name from the person's
pool: "Provisioner Cole", never a family name), and `GenerateUniqueRole`
(deduped against the world like any name). The role check sits beside
`IsGenericName` on all three naming paths: the seed gives a role mind
"Role First" at creation; the per-second sweep's base-name converge is
guarded so the bare role word is never stamped back over the role name,
and a pre-0.7.3 mind (a bare role word, or a restore-time full name)
converges to its role name; restored worlds self-heal on the next sweep.

**Consequences:** Two provisioners are distinguishable in memory and on
the actor (SetOverrideName). A provisioner household's child takes the
provisioner's first name as its family name via the existing `FamilyOf`
rule ("Mara Cole") — accepted, reads as a real name, noted for the
households stone if it ever grates. Raiders never reach the sim (no
workshop faction), so enemies keep the game's own labels.


**Follow-up (same day, in-game verification):** two of the very people
the stone was built for stayed bare. (1) The game names a supply-line
settler "Provisioner" itself — the label lives on the reference's
display name (`GetDisplayFullName`), the base form stays the generic
"Settler", so the base-form role rule never matched, and the game's own
text-display override made the sweep's write guard skip the actor. (2)
The road caravans — generic "Provisioner"/"Caravan Guard" NPCs roaming
with the brahmin — hold no settler faction, so `IsSimRelevant` never
let them in to be named. The role rule now reads the display name
first (base fallback; real names and placeholders still decided by the
base exactly as before — the display read is only ever a role
candidate), the sweep writes the role name through a bare role-word
display (a player rename is respected), and a role-word base form
passes sim-relevance without the faction, gated by the device-race and
NPC-base checks so no prop sneaks back. The road's people are minds —
and they are exactly who a hungry settler trades with on the road
(0.7.4).

**Second verification (same day) — the supply-line mask is game-deep.**
A one-line diagnostic (`base 'Settler', wrote 'Provisioner Atlas',
after 'Provisioner', text-display present`) proved the visible name of
a supply-line provisioner cannot be overridden: the game re-derives
"Provisioner" from the assignment itself, ahead of any extra-data
override. The top Nexus renaming mod (Settler Renaming, 6,856
endorsements) documents the same wall — even the console `setname`
command fails there. Accepted as a universal FO4 limitation: the sim's
role name lives in memory, the log, and the co-save (the 0.7.3
requirement), the radio speaks it, and the write is kept because it
lands the moment the provisioner is reassigned to another role.

---

### ADR-0028 — Trade with anyone: the sellers are minds (2026-08-12)

**Context:** 0.7.4 (the 0.8.0 pillar pulled forward) widens who the
hungry walk resolves to — the bench was the only food source, but the
world is full of sellers: road traders, marketplace stalls, the caravan
merchants. The engine needed nothing (the core already resolves Trade
to a person once a Trade memory to one exists); the question was purely
who counts as a seller and how a mind comes to remember one.

**Decision:** (a) **The vendor signal.** `SimRelevant::IsVendor` — the
game's runtime vendor faction (fast path) or a merchant container on
any base-form faction (`TESFaction::vendorData.merchantContainer`,
deterministic — the runtime faction is computed lazily). Anyone who
sells passes `IsSimRelevant` on their own, becoming a full mind: a
name, needs, memory, co-save — the same machinery as a settler. (b)
**The who-sells seed.** `SeedVendors` mirrors `SeedMarketMemory`:
every human mind with a loaded actor remembers the nearest seller
within the market radius at weight 1.05 — a hair above the market
seed's 1.0, because the core's `ChooseTarget` breaks ties to the
first event and the market seed runs first; a person who sells
out-scores the bench while both are fresh, and the bench takes over
again when the seller leaves. The arrival already trades directly with
a person target, and `RecordSale` warms the seller — the exchange is
unchanged, only the geography of who is remembered widened.

**Consequences:** Sellers join the sim — a road trader is a mind, a
Diamond City stall-keeper is a mind, and the marketplace becomes a
place where sellers trade with each other when no customer is near (a
vendor's nearest seller is another vendor). A vendor never shops at
their own stall (the seed excludes the mind's own form). The stall
economy is untouched when no seller is within walking distance — the
bench remains the fallback. The engine stays untouched; this is game
knowledge at the edge (ADR-0024), the pure `NearestVendor` rule tested
in the harness.


### ADR-0029 — The verify channel is load, not free: decisions log on change or cadence (2026-08-12)

The per-frame decision lines were the adapter's own drag: a mind with
near-tied Rest/Explore scores re-rolls its intent every frame (the
core's decide adds per-call jitter), and the key-based dedupe let each
re-roll write a synchronous file line on the game thread — 22,478
"decides" lines in under three minutes of one session, 75% of the log,
growing the frame hang the player reported as "longer the more we add."
`LogPlanEntry` now logs on intent change or once per
`sim.log.decisions.every` seconds (default 1.0), whichever comes first:
real decisions (a walk's target, an arrival) still print instantly; only
the per-frame flip-flop is throttled. The core's tie-break jitter is
flagged for the engine handoff — a settled mind should rest, not
re-roll — but the adapter no longer amplifies it into a file-I/O flood.

### ADR-0030 — A fight is a Combat wrong, booked, not an animation (2026-08-12)

**Context.** 0.7.5 Fights needs the feud's physical escalation: an enemy
pair's row can turn to blows. The design (RealEvents.md) said "real
combat or a scripted scene", with pex deferred. Two questions: what the
fight IS in the sim, and how the game shows it.

**What the fight is.** The row (0.7.2) books Wronged both ways (an
unprompted wrong, −0.25). A fight is the same shape with the heavier
kind: the engine's Combat channel — the decided unprompted-wrong
channel, −0.25 each way — so the feud deepens on both sides, the
crossing publishes RelationshipChangedEvent the instant it happens, and
the victim's Combat memory feeds the engine's danger-awareness
(FindThreat names the strongest remembered fight as a thing to flee), so
the feud's victim starts avoiding the aggressor — the fight's visible
aftermath is emergent, not scripted. Once per pair per day, gated by a
Combat memory stamped today (co-saved — save/load never double-fights),
mirroring the row's per-day gate.

**Who fights.** Three gates in Fights::RollFight, all must pass: the
pair is Enemy (rivals stay verbal — the verbal-first rule), the
aggressor's temper is at or above sim.fight.temper (the same
JitteredTraits shape as the slight's — the churlish throw the punch),
and the world's coin lands under sim.fight.chance (0.1 default; 1.0
forces every eligible escalation — the test knob). Two trigger sites:
the bench row, and the shut-stall slight against an enemy keeper (the
reliable test path — force the market shut and the slight fires at an
enemy who catches a fist).

**What the game shows.** v1 books the fight with ZERO new game calls —
the words (fight pool), the news, and the emergent avoidance. The
punch/shove animation is the polish step, and the research is honest:
the base game has no playable shove idle (the punches are paired
kill-cams — PairedKillTacklePunchToDeath etc.; the crowd shove is a
sandbox behavior, not a form), and StartCombat is not exposed by
CommonLibF4, not in the runtime address library, and not safely
pinnable without a debugger. So the animation is either a paired-idle
or a byte-verified combat pin, decided after the in-game verification
of the fight's substance. A wrong pin never teleports (the Movement
rule) — a best-effort animation can fail to the sim fight, never the
reverse.

**Addendum (same day).** The punch found a home before the first test:
the user's "Get Out Of My Face" mod (Nexus 20353) confirmed the game's
knockback is the shove — its Papyrus perk pushes via the game's own
physics. CommonLibF4 exposes that machinery as AIProcess::KnockExplosion
(REL::ID; the user's Address Library is version-1-11-221-0 — the exact
runtime, so the lookup is safe). The fight now shoves the victim back
from the aggressor (sim.fight.push, default 3 units — a stagger, not a
ragdoll; 0 turns it off), so the punch is visible in-game while the
sim booking stays the deliverable. Still best-effort: a missing actor
or process skips the shove; the fight is booked either way.

### ADR-0031 — "Today" cannot live in fading memory: the once-per-day gate is a day-scoped map (2026-08-12)

**Context.** The first 0.7.5 test in a restored 610-mind world showed
fights firing in waves — the same pair "come to blows" 141 times in one
session, 445 fights total, everyone shoved repeatedly, both sides
falling over. The once-per-day gates (Rows::AlreadyRowedToday,
Fights::AlreadyFoughtToday) gated on the pair's Wronged/Combat memory:
a memory event stamped today, scanned each crossing. It looked right
and the harness passed — but the harness never runs the engine's fade.

**The bug.** Memory fades (sim.memory.fade 0.2/s, forget below
sim.memory.forget 0.1). A Wronged or Combat event starts at weight 1.0
(WorldFacts::kFactWeight) and erases itself in (1.0 − 0.1) / 0.2 = 4.5
seconds. The waves were 10s apart: by the second wave the gate's
evidence was gone, so the pair re-rowed and re-fought — every ~10s,
forever. "Words once a day" and "blows once a day" are day-scoped
facts; salience is the wrong substrate for them.

**The fix.** The adapter owns a durable ConflictGates map — ordered
pair → { last day rowed, last day fought } — explicit, O(1), rolled by
comparison (a stored day != today means the pair is free), and co-saved
(v7: the gate section rides the record, translated to form ids at the
edges like bonds). The memory events still land (the feud still
deepens, the threat still forms); only the *gate* moved out of memory.
The regression test now erases the pair's memories mid-day and asserts
the gate still holds — the old build fails it.

**The retaliation.** The test also asked the natural question: after the
punch, can the victim push back, hit back, or run away? Running away
already exists — the Combat threat feeds the engine's danger-awareness,
and the victim Flees when Safety is their most urgent need. Pushing
back is now physical too: a victim whose temper is at or above the same
sim.fight.temper line shoves the aggressor back — one exchange, never
a loop (the aggressor threw the first punch, the victim answers once,
and the day-gate holds the pair to a single scene). The temper line is
the knob: raise it and fewer victims answer.

### ADR-0032 — The species split is enforced, not hoped: animals are fed, people feud (2026-08-12)

**Context.** The fight test showed behemoths (Gizmo, Harley) brawling at
the market bench — trading, feuding, shoving each other. The user's
spec is clear: an animal's whole life at the settlement is "get fed at
the workbench, then continue its game routines." Trading, arguing,
fighting, buying, befriending, and marrying are people's business.
Also: the world's unnamed work crews ("Worker" NPCs) showed no name,
and nothing capped how many spouses a mind could hold.

**The behemoth gap.** ClassifySpecies's animal table was missing
SupermutantBehemothRace (0x000BB7D9) — the default fallback is Human,
so every behemoth was seeded as a person: a pouch, a name, enemy
bonds, a feud. The table grew (both the classification and the
known-organic complement), and — the deep fix — **a restored mind's
stored species is no longer trusted**: it is re-derived from the
actor's race the moment the actor loads (ReclassifyLoadedMinds, at
restore and in the per-second sweep), so a behemoth saved as Human
before the fix is corrected in place — pouch dropped — and never
barters again.

**The behavior gates.** Three layers, so an animal can never leak into
people's business even if a table misses a race:
1. The bond book (Reconcile + the event channel) refuses a pair where
   either side is an animal — no friend, no rival, no enemy, no spouse.
   A dog has no feud to row, no fight to throw.
2. The bench crossing skips an animal outright (belt on top of the
   book).
3. Restore prunes an old save's animal bonds and animal stall-keepers
   (the same gates read the corrected tag), so a pre-fix world heals
   itself instead of carrying stale behemoth feuds.
Owned animals keep their names (the owner rule stood); unowned strays
stay nameless — the spec's exact shape.

**The Worker gap.** The game names Sanctuary's work crews "Worker" —
IsGenericName knew "Settler" and "Workshop Worker" but not "Worker",
so the sim kept the bare label. Added; a Worker gets a real name like
any other person.

**The monogamy cap.** The bond book had no limit — the same warm heart
could cross the spouse line with more than one person. ApplyPair now
caps a would-be spouse bond at sweetheart when either side already
holds a spouse bond elsewhere; an existing marriage is never broken by
the cap (only new ones are refused), so a heart can warm twice without
a second marriage forming.

### ADR-0033 — Family is off the menu: the kin gate keeps vanilla families platonic (2026-08-12)

**Context.** The sim derives friends, sweethearts, and spouses from
dispositions — but the world it seeds already contains families. A
father and daughter whose dispositions warm over shared meals must
never cross the sweetheart line. The bond book had no idea who is kin.
The user's spec, verbatim: *"scan for pre existing characters that are
hinted at already being family make sure that pool is fixed unless a
future quest breaks it things like child husband wife brother sister
dont want to accidentally add nasty stuff."*

**The problem with surname rules.** The obvious approach — block any
pair sharing a surname — is wrong twice: it blocks nothing (most
named families don't share a visible surname in-game, and sim-assigned
names collide by design), and it wrongly blocks Jun and Marcy Long, who
*are* married and whose marriage is lore-correct. A surname is a false
signal; the family lines must be named.

**The curated kin table (Kin.h).** The vanilla settler families'
parent-child lines, verified against the Fallout wiki (2026-08-12):
- Abernathy Farm — Lucy (0006B4D2) is Blake's (0006B4D3) and Connie's
  (0006B4D1) daughter.
- Finch Farm — Daniel (0003F22B) is Abraham's (0003F22D) and Abigail's
  (0003F22C) son.
The Longs and the Warwicks are *married couples* — the sim marrying
them is lore-correct, so they are deliberately not in the table. A
future quest or mod that adds a family adds its base pair here.

**The species layer covers every actual child.** A child — game-born or
sim-born — is Child species, and Reconcile refuses any romantic bond
with a Child on either side; the kin table exists for the adult
relatives who would otherwise be eligible. Both layers feed one flag.

**Mechanics.** RebuildKin (per-second sweep, alongside RebuildOwners)
indexes every loaded actor's base form id (low 24 bits — stable
whatever the load order) and folds the curated pairs' entities into
m_Kin as ordered key pairs. The bond gates — Reconcile's pass and the
RelationshipChangedEvent channel — share the same `kin` flag into
ApplyPair, which refuses Sweetheart and Spouse for a kin pair, capping
at Friend. The gate applies to the *current* bond too, so a pre-fix
save's mistake heals on the first pass. Derived, never persisted: a kin
pair only matters while both actors are loaded, and the heal is
self-evident.

**What the user sees.** On load, `kin: N family pairs gated from
romance` in the log; the Abernathy and Finch households never romance
each other, whatever the meals do to their dispositions; and the log's
bond lines never name a family pair as sweethearts or spouses.

### ADR-0034 — Blows are people's business: children row, never fight (2026-08-12)

**Context.** The safety audit after the kin gate asked what else could
trip the sim up. The species belts were thorough — animals are gated
out of the bond book, the bench crossing, trade, and stall-keeping —
but one gap surfaced: the fight escalation (EscalateToFight, entered
from both the bench crossing and the shut-stall slight) had no species
gate. Children pass the row gate (animals are blocked at the cross;
children are not), and a Child with an enemy — possible: the bond book
gates animals, not children, and only *romance* is refused for a child
— could throw and take punches, get shoved back by the KnockExplosion,
and carry a threat memory. A child being physically shoved at the
market is wrong on every level.

**The rule.** Rows are words — a child arguing with an enemy at the
bench is natural, the feud's audible half stays open. Blows are
people's business: both fight participants must be Human species. The
gate lives inside EscalateToFight, the single chokepoint both entry
points share, so a future fight source cannot forget it — the same
belt-and-suspenders pattern the animal gates use.

**What the audit confirmed safe.** Player excluded from seeding and
walks; companions not seeded (settler-faction/role/vendor gate);
robots, turrets, and spotlights device-gated; animals fed, never
feuding; children fed at the bench (the non-Human arrival path),
never trading, never running stalls, never romancing (Child species +
no aging — a sim-born child is a child forever, so siblings can never
become eligible); fights book a sim Combat memory, not game combat
state, so no quest-essential actor is ever forced into a hostile game
state (the future real-combat polish must respect essential flags —
noted for 0.8.x); married couples can dissolve into rivals/enemies
(the family-flip rule) and the household pouch splits — drama, not a
bug; kin pairs can feud (a father-son row is drama) — only romance is
kin-gated.

### ADR-0035 — The companion's dating pool is closed: friends and feuds, never romance (2026-08-12)

**Context.** The safety audit's one open question came back as a
decision: *"yeah we should block companions from dating zone become
freinds with enemy is fine."* A companion dismissed to a settlement
gains the settler faction and becomes a mind — she trades, she names,
she befriends, she feuds — but she must never cross the sweetheart or
spouse line with a settler. Friends and enemies are explicitly fine;
romance is not.

**The signal.** The base-game ESM hides its companion NPCs behind
quest aliases (no static companion faction on the base records — the
ESM forensics proved it: no companion NPC record holds any of the four
companion factions). The reliable truth is runtime: the game applies
**HasBeenCompanionFaction** (FACT 0x000A1B85, verified in
Fallout4.esm) permanently the moment a companion is recruited — and a
companion can only ever reach a settlement (become a mind) by being
dismissed there, which requires recruitment. So the faction is always
set exactly for the minds that must stay out of the dating pool.

**Mechanics.** A new marker component (CompanionTag, co-saved like
SpeciesTag) rides each mind. It is set at seed and re-derived every
second in the same pass that re-derives species (ReclassifyLoadedMinds
— an actor reading HasBeenCompanionFaction gets the tag; a mind whose
tag no longer matches loses it), so a pre-fix save heals the moment
the companion's actor loads. Both bond channels — the reconcile pass
and the RelationshipChangedEvent handler — fold the tag into the same
`kin` flag ApplyPair already refuses romance with (capped at Friend),
so the companion gate and the family gate share one code path. The
kin-gate test grew a companion leg: spouse-grade warmth caps at
Friend, and enemy-grade warmth still forms the feud — the user's exact
shape, "friends with enemy is fine."

**What the user sees.** On load (or when a companion's actor streams
in): `companion: <name> is a companion — friends and feuds, never
romance.` No sweetheart or spouse line ever names a companion, in the
log or on the radio.

### ADR-0036 — Every shove lands a little differently (2026-08-12)

**Context.** The fight test proved the once-per-day gate (ADR-0031),
the KnockExplosion shove (the "Get Out Of My Face" knockback), and the
retaliation all work — but a fight at `sim.fight.push = 3` is always
the same 3, and a settlement that sees the same pair brawl every few
days starts to notice the identical shove. Polish pass.

**The change.** The shove force is now the base `sim.fight.push`
scaled by a deterministic ±25% jitter derived from the *victim's*
entity id (the same FNV-1a IdJitter the needs variance uses — same
pair, same brawl; new victim, new feel). The retaliation shove jitters
off the *aggressor's* id, so the two halves of a scuffle read
differently too. No RNG, no state: a restored fight (co-save) shoves
exactly the way the original did.

**Frequency.** `sim.fight.chance = 0.1` stays the default — the
"rare and earned" rule (with the temper line at 1.0, only the churlish
half of the world is even eligible, so roughly one in twenty enemy
crossings turns to blows). The INI now documents the feel and the
presets so the user can dial without a rebuild: 3 = shove, 5-6 =
solid stagger, 10+ = ragdoll; chance 0.1 = drama, 0.25 = lively,
1.0 = the test knob.

**The session proof.** The gate-holds verification is a log check:
`come to blows` lines, pair-canonicalized, must show each pair at most
once per session — the old build showed Harley/Gizmo 141 times in one
session, the fixed build showed it once.

### ADR-0037 — The test hook: a pinned pair brawls on demand (2026-08-12)

**Context.** Verifying the fight machinery in-game means waiting on the
sim's coin: an enemy pair must cross, the temper must be hot, the
chance must land, and the once-per-day gate means one shot per day.
The user asked for the direct thing: *"a test script force fight loop
between 2 — Sturges and Jun."*

**The hook.** sim.test.forceFight pins two minds by form id (low 24
bits) and brawls them on a loop every sim.test.forceFight.interval
seconds. Each iteration: the pair is pinned to an Enemy bond (the
fight machinery requires a feud; the hook forces its fuel), the
aggressor alternates so the shove lands on both sides, and the fight
books with the once-per-day gate bypassed (BookFight's new a_force) —
the full scene fires: Combat memories, deepened feud, gossip, threat,
shove with its per-victim jitter, the retaliation when the victim is
hot-headed, the fight line, and the news ("... — the feud turns
physical. (test brawl)"). The species belt still holds — a forced
brawl needs two adults like any fight.

**Why a force param, not a bypass.** The gate lives in BookFight, the
single chokepoint; a boolean keeps the loop honest — everything the
real sim does, the loop does, minus the two waits (the coin and the
day). Off by default (both form ids 0); a test line in the log tells
the forced fights apart from the sim's own.

**The harness.** FightsTest grew the loop's core assertion: the same
pair, the same day, forced again — booked (the old behavior would
refuse); the gate still records the forced fight.

### ADR-0038 — The shove is a bench scene: falls need proximity (2026-08-12)

**Context.** The force-test loop (ADR-0037) exposed a real bug the
sim's own coin had been hiding: *"they fall over not being anywhere
near each other."* A fight can book between minds that are far apart —
the loop fires on its timer wherever the pair is, and a restored
keeper may not be standing at her stall yet — and KnockExplosion uses
the aggressor's position as the knockback ORIGIN. A distant origin is
a ghost: the victim is thrown along a vector from a point nowhere
near them, which the player reads as a random fall with no one around.

**The fix.** The physical shove is gated on proximity: within 400
units (the bench scene — the pair who crossed at the market are
adjacent) the shove and its retaliation fire exactly as before;
beyond it, the shove is skipped and the log explains ("brawl at range
(1234 u) — the shove waits for the bench"). The fight still books at
range — the Combat memories, the deepened feud, the gossip, the news
are all sim-level truth and stay. Only the animation is deferred to
when the pair actually meets. The sim's own fights (which require a
bench crossing) are untouched; the loop is now safe to watch from
anywhere — a ranged brawl reads in the log and on the radio, and the
falls only happen when the pair is genuinely face to face.

### ADR-0039 — The shove logs its receipt: force, distance, who (2026-08-12)

**Context.** The loop test's close-range fights showed "both go down at
once, no shove animations" — with zero retaliation lines in 37 fights,
so the double-fall is the game's physics (the victim's ragdoll clips
the adjacent aggressor), not two punches. And a fall with no visible
shove could be a too-weak knock or not ours at all. The shove was
silent — the log only spoke when the shove did NOT fire (the range
defer) or the victim answered. Falls couldn't be matched to pushes.

**The change.** Every physical punch now logs its receipt: `shove:
<pushed> pushed by <puncher> — <force> force at <distance> u.` A fall
can always be matched to its push, or proven not to be one — the
double-fall question resolves from the log, not the screen. The base
force moves 3 → 4 (the crowd mod's proven 5 was a solid shove; 3 read
as a tip-over in the loop test), so the jitter spreads 3–5 — a shove
that visibly registers rather than a collapse.

### ADR-0040 — The scuffle reads as a sequence: push, fall, get up, push back, slink off (2026-08-12)

**Context.** The loop test showed the exchange wrong: "really odd
looking — no pushing at all, both do exact same thing same time and
fall over." Two problems surfaced from the log. First, the punch's
force was reading 2.4–3.0 in the session — the user's INI still had
sim.fight.push = 3 (the shipped default was bumped but the override
won), and at that force KnockExplosion plays a collapse, not a shove.
Second, the retaliation fired in the SAME instant as the aggressor's
punch (when it fired at all — the test pair's temps never passed the
line), so the exchange had no sequence: both bodies hit the deck at
once. And the chain's ending was missing entirely — the engine's Flee
action is a declared-but-unbuilt table stub ("fleeing is unbuilt,
executes nothing in-game yet"), so "decides Flee" never even logged
and nobody ever ran away.

**The sequence.** The scuffle is now three beats:
1. The aggressor's shove lands now — visible push at force 5 (the
   crowd mod's proven default; the shipped INI carries it).
2. A hot-headed victim answers after a beat (sim.fight.retaliation.
   delay, default 4s — the get-up window), via a pending-retaliation
   queue the per-second sweep drains; the forced loop's test pair
   answers always, so the full chain is watchable on demand.
3. The one who threw first slinks off: the first visible flee — a new
   Movement::WalkAwayFrom walks the loser to the cell reference
   FARTHEST from the one who answered (mirrors WanderNear's cell
   enumeration; skips actor refrs; falls back to HoldPlace). The
   engine's real Flee action stays a stub for the engine tab — this
   is the adapter's beat, and the sim's own fights get it too (a
   feud's loser walking off is readable drama, not just a test).

The counter-shove keeps the scene gate (they parted — no ghost
punch). The receipts stay: `shove:` for the first punch, `shoves
back` for the answer, and the range line when either waits.

## ADR-0041 — the shove's default force is 8, and the jitter caps at 1.15×

**Status:** Accepted (0.7.5).

**Context:** Every loop test since ADR-0040 ran with the user's INI still overriding `sim.fight.push = 3` — a force that the game reads as a tip-over, not a shove, so "they go down separately but no push or shove animation" kept recurring no matter what the DLL default said. The shipped default had been bumped once already (3 → 5) but the override masked it.

**Decision:** The default is now `sim.fight.push = 8` — a knock that visibly moves the victim (the crowd mod's own scale: 5 is a shove, 6–8 a solid stagger, 10+ ragdoll). The ±jitter now caps at 1.15× so a strong deterministic draw never crosses into the "insane" 10+ zone. The INI comment documents that an old `sim.fight.push = 3/5` line in the player's file silently overrides the DLL default — delete the line to inherit it.

**Consequences:** The scuffle sequence (ADR-0040) finally reads as intended at default settings: push → fall → get up → push back → slink off. The jitter cap keeps every draw in the 6–9 band. Force is still fully tunable per-install via the INI.

## ADR-0042 — the standing shove is the melee hit-reaction, not the knock force

**Status:** Accepted (0.7.5).

**Context:** Through every loop test the victims "fell over with no push or shove animation" — no matter the force (3, 5, 8), `AIProcess::KnockExplosion` only ever knocked them down in place. The game's visible shove is a different mechanism: the melee hit-reaction — the standing stagger that plays when a punch actually lands (Actor::DoHitMe). The "Get Out Of My Face" crowd mod (which the user enabled as a reference) proved the recipe: its BA2 is pure Papyrus calling the game's `PushActorAway` native (opcode 393, the same one the vanilla PainTrain perk uses) AFTER playing a push idle — but its push idles are its own esp IDLE records (behavior-graph hooks: DNAM = behavior path, ANAM/DATA = animation indices), not vanilla forms. The vanilla ESM has no idle that reads as a shove (verified by scanning all 3025 IDLE records). So the standing push animation can only come from the hit-reaction.

**Decision:** Before the knock fall, play the melee hit-reaction on the victim: a zero-damage `HitData` (built on zeroed raw storage — HitData has no default ctor, and the game's own explosion-stagger path uses exactly this shape: weapon null, zero damage, a stagger magnitude, a push-back) carrying `stagger` (sim.fight.stagger, default 2 = medium) and `pushBack` (sim.fight.pushback, default 25). Nothing is flagged as an attack — the scuffle stays a scuffle, no combat escalation, no damage. Then the existing KnockExplosion fall fires, so the sequence reads: punch → stagger back → fall → get up → answer → slink off. The retaliation beat carries its own flinch.

**Consequences:** The push animation finally shows in-game. `sim.fight.stagger = 0` turns the flinch off (fall only); the magnitude is 1–4 (small → extra-large). One vendored-commonlib quirk surfaced: `BSPointerHandle`'s pointer constructors are broken in this build (they call a `get_handle(Y*)` that exists nowhere), so the handles are built through the game's own `BSPointerHandleManagerInterface<T>::GetHandle` wrapper instead.

## ADR-0043 — the fall is sequenced after the flinch, never same-frame

**Status:** Accepted (0.7.5).

**Context:** ADR-0042 made the punch fire the game's melee hit-reaction (the standing stagger), but in-game it still read as a plain collapse — "one falls, the other falls, no animation." The cause was not the flinch failing to fire (the receipts proved it did): the knock-down (`KnockExplosion`) landed in the same frame, and a same-frame knock overrides the stagger animation before it is ever visible. The flinch was playing — underneath the fall.

**Decision:** Every physical beat of a scuffle is now scheduled, never fired in the same frame as the one before it. The flinch fires at the punch; the fall is queued `sim.fight.fall.delay` (default 0.9s) later; the hot-headed victim's answer (or a forced test pair's) fires after the get-up window with its own flinch, its own fall a beat after, and the loser walks off last. The pending-retaliation queue became a general `PendingShove` queue with four beats (fall, retaliation, counter-fall, walk-off); every beat re-checks the pair is still at the scene (parted → the beat dies, no ghost punches or phantom falls). The chain: flinch → fall → get up → counter-flinch → counter-fall → slink off.

**Consequences:** The stagger finally plays before the fall, so the punch reads as a shove. Each beat logs its own receipt (`shove:` for the flinch, `fall:` for the knock-down), so a visual mismatch can always be matched to which beat did or did not fire.

## ADR-0044 — the shove queue never mutates while iterating

**Status:** Accepted (0.7.5).

**Context:** The first sequenced build (ADR-0043) hit a debug STL assertion at runtime — "vector iterators incompatible" at the MSVC `vector` header. The cause: `ProcessPendingShoves` iterated `m_PendingShoves` and, in the retaliation case, `push_back`'d the counter-fall and walk-off beats while the loop held an iterator; the reallocation invalidated it, and the next iteration dereferenced a dangling iterator. In debug builds the STL asserts; in release it is silent corruption.

**Decision:** New beats are collected in a local `std::vector<PendingShove> additions` and appended with `std::move` after the loop terminates. The other member-vector loops (deaths, stall keepers, bonds, walks) only `erase` — which returns the next valid iterator — so they were audited and left as-is.

**Consequences:** The scuffle's full chain (flinch → fall → counter-flinch → counter-fall → walk-off) runs without invalidating the queue. The assertion is gone.
## ADR-0045 — The paired push (the visible shove)

The crowd mod's push is the VANILLA paired idles: `PairedFrontPushKick`
(0x47FC3, attacker lead) + `PairedFrontPushKick_Human` (0x6571F,
victim half), both on `MeleeBehavior.hkx`, played via
`AIProcess::PlayIdle(actor, idle, target)`. Its ESP ships no animation
files — it only re-exposes these records unconditionally. So the shove
plays the vanilla pair directly: attacker kicks, victim's half syncs,
the engine aligns the two. No ESP, no dependency.

Also: the shove gate tightens 400u → 150u (a table-scene needs the pair
within reach) and a fight booked at range now walks the thrower to the
victim first, so the next beat lands face to face instead of a ghost
push across the square.
## ADR-0046 — The fight's rhythm (retaliation and parting beats)

The exchange was two falls blurring together: the retaliation fired 4s
after the shove (inside the get-up window) and the loser's walk-off
landed the same instant as the counter-fall (a ghost sprint from the
ground). The beats now read as a sequence: kick, fall, get up, PAUSE,
answer, fall, get up, slink off. Two keys: sim.fight.retaliation.delay
(4 → 7s, so the get-up finishes before the answer) and the new
sim.fight.part.delay (3s between the counter-fall and the walk-off).
## ADR-0047 — Loud fight lines (the threats reach the screen)

Say() deliberately never popped the HUD — speech was feed-and-log only
("speech is quieter"). But the fight's threats ("Come on then!", "Put
'em up") are the words before the blows, and the on-screen beat was
just the "come to blows" news. Say now takes a_loud: the fight pool
rides the same throttled HUD pop as news, so the threat lands on
screen before the shove. Ordinary conversation stays quiet.

> Superseded on the same day by ADR-0048: the loud line's home became
> the game's own subtitle queue, not a top-left news pop.

## ADR-0048 — the loud line's home is the game's subtitle queue (0.7.5)

The 0.7.5 fight threats were log-and-feed only, then a top-left news pop.
The player asked for dialogue-style lines: bottom of the screen, only
when close enough to hear. The game's own subtitle display is
`SubtitleManager`'s priority array — the same queue dialogue lines use —
and it is fully wrapped (SubtitleInfo, SUBTITLE_PRIORITY, the manager's
singleton + RW lock). Pushing an entry with the speaker's handle and the
line makes the HUD render it as a bottom subtitle on its next frame;
the `"who: \"line\""` prefix keeps the box self-contained. New key
`sim.subtitle.radius = 500` (game units) gates it: a nearby brawl is
loud, a cross-settlement squabble is not. The feed still carries every
line for the radio's story; the top-left news pop is retired for speech.

## ADR-0049 — subtitles read clean (0.7.5)

The first subtitle pass embedded the log's label, which carries the form
id beside the name ("Jun Long [FF01A4D7]") for the verify channel. On
screen that hex reads as noise. The subtitle now uses a name-only label
(`MindNameOnly`): just "Jun Long" — or the bare species label for an
unclaimed stray. The id-bearing label stays in the log, the feed, and
the receipt; only the screen shows the clean line.

## ADR-0050 — the fall tips, the beats wait (0.7.6)

Two fight-feel bugs from the 0.7.5 field tests. (1) The ghost-push
slide: after the paired push carried the victim back, the fall's
KnockExplosion fired at force 6–9 (sim.fight.push 8 ± jitter) — near
the ragdoll zone — and the horizontal impulse dragged the lying victim
along the ground. The paired push IS the shove now, so the knock only
has to put them DOWN: sim.fight.push default drops 8 → 3, the tip-over
zone measured in the loop tests (2.6–3.5 with jitter), a fall in place
with no slide. (2) The both-fall look: a beat could fire while the
other actor was still mid-knock (down, getting up, a knock queued).
New standing guard `IsDown` (Actor's knockState vs KNOCK_STATE_ENUM:
anything but kNormal is not on its feet): a fall that finds its victim
down — or a retaliation that finds either actor down — waits 500ms and
re-checks instead of firing into the state. The receipts print the
wait ("fall waits — X still on the ground"), so the log proves the
guard fired.

## ADR-0051 — the kick is real now (0.7.6): our own unconditional IDLE

The field receipts were damning: every shove read "flinch fallback" and
"paired push" never once fired — the "kick" the player saw was the
stagger flinch, and the retaliation's flinch didn't read at all. Root
cause, decoded from the ESM: the vanilla PairedFrontPushKick records
carry a CTDA condition and DNAM RaiderRootBehavior.hkx, so PlayIdle
refuses them outside their combat context (settlers run
CharacterRootBehavior, not RaiderRoot). The crowd mod's decoded recipe
is the fix: unconditional IDLE copies with DNAM
Actors\Character\Behaviors\MeleeBehavior.hkx, ENAM dyn_Activation,
ANAM zeroed (no sync partner), and GNAM pointing at the vanilla
Actors\Character\Animations\Paired\PairedFrontPushKick_AttackerLead.hkx
(it ships no animation files — the path is vanilla, which is why it
works). data/TheLivingCommonwealthAnims.esp (380 bytes, generated by
Not-for-github/make_anims_esp.py, byte-verified against the crowd mod's
record) carries LCW_PairedFrontPushKick; the adapter resolves it
load-order independent via TESDataHandler::LookupFormID and plays it on
the attacker — the kick finally plays, on the first shove AND the
retaliation, and the receipt flips to "paired push". Missing ESP falls
back to the vanilla pair, then the flinch — nothing crashes.

### ADR-0053 — The save "CTD" was post-save, not the co-save

**Date:** 2026-08-12

**Symptom:** Saving during the 0.7.5 fight loop crashed to desktop; the
co-save hook's "writing 635 entities..." line was the log's last line, so
the serialization path looked guilty.

**Forensics (not speculation — files, not logs):** the Windows event log
had no Fallout crash entry; the F4SE log's last line was "cleared save
path", which is F4SE's *final* save-phase line — the plugin callbacks,
event registrations, persistent storage, and delay functor all wrote
first. The produced files prove it: Save508's .f4se sidecar is 276,495
bytes (our 275,437-byte co-save record + ~1KB of F4SE overhead, exactly
right) and the .fos is a full 17.25MB with a valid FO4_SAVEGAMEk header
and a footer identical to the previous good save. **The save completed;
the crash landed after it — in the game's post-save work, the known
vanilla crash class when saving mid-knockdown/ragdoll.** The fight loop
leaves actors falling/getting up at the exact moment of the save.

**Decision:** the co-save is exonerated; no serialization change. The
player-facing rule is "don't save mid-fight" — the game itself can crash
writing an actor that is mid-ragdoll, and no plugin code runs there. The
fight-feel fixes (0.7.6) aim to shorten the knockdown window, which
narrows the exposure too. If a save-mid-fight crash reappears *outside*
the fight loop, reopen this ADR.

### ADR-0054 — The kick's ESP must be in the load order

**Date:** 2026-08-12

**Root cause of "one kick, then ghost pushes":** the ESP was never
enabled. The profile's plugins.txt (23:17) lists only
*Baby Sim - Babies That Grow Up.esp; TheLivingCommonwealthAnims.esp sat
in the mod folder but was never added to the load order, so PlayIdle
resolved nothing and every beat fell back to the flinch — the "kick"
was the stagger flinch, and the "ghost pushes" were the knock impulse
(sim.fight.push = 5 in the player's INI, above the 0.7.6 default of 3)
sliding actors without any visible push. The ESP itself is
byte-identical to the crowd mod's proven record (all five subrecords —
DNAM/ENAM/ANAM/DATA/GNAM — SAME), verified against PushAwayCompanions.esp
before install.

**Action:** the player refreshes MO2, adds TheLivingCommonwealthAnims.esp
to the load order, and re-runs the force-fight loop — the receipt should
read "paired push" and a real kick plays on both beats for the first
time.

### ADR-0056: Birth Lifecycle — Pregnancy Window, Gestation, Growth (0.7.7)

**Decision.**
The birth lifecycle is now a journey, not an instant event. Three new
adapter-owned components drive it:

1. **Pregnancy** — conception day, due day, parent IDs. Stored on the
   mother entity; co-save serialized via `MakePregnancySerializer`.
2. **BirthDay** — the sim-day a child was born. Used by growth to
   track age. Co-save serialized via `MakeBirthDaySerializer`.
3. **Growth** — `SpeciesTag` upgrades from `Child` to `Human` after
   `sim.birth.childhood` sim-days, plus a starter `CapPouch`.

The tick sequence each sim-day:
1. `CheckBirths` scans pregnancies at their due day → creates child
   minds, stamps `BirthDay`, removes the `Pregnancy`.
2. `RunBirth` rolls `sim.birth.chance` per eligible couple →
   `TryConceive` creates a `Pregnancy` on the mother.
3. `GrowChildren` upgrades children past `sim.birth.childhood`.

New INI keys: `sim.birth.chance` (0.05), `sim.birth.gestation` (3),
`sim.birth.childhood` (10). Co-save gains two additive named
components (no record-version bump).

**Rationale.**
The instant-birth model was a proof of concept. A pregnancy window
gives the world time to notice (the "expecting" news line fires at
conception), the birth is an event with a receipt, and growth is a
visible transition — the child walks to market as a full mind.

**Trade-offs.**
- Multiple births per day are possible (one roll per couple); the
  chance default (0.05) keeps it rare.
- Growth is purely sim-side — the visible actor half waits on 0.7.8.
- The mother is deterministic (lower entity ID), so save/load is stable.

### ADR-0057: Visible Children — Graceful Degradation (0.7.8)

**Decision.**
0.7.8 adds the infrastructure for visible children without depending on
the external baby mod. A new INI key `sim.birth.visible` (default off)
controls whether grown children pair with real game actors.

The `PairVisibleChild` function in Birth.h is a graceful-degradation
stub: when the baby mod ("Baby Sim - Babies That Grow Up") is absent
or BirthVisible is off, children stay sim-only (0.7.7 behavior). When
the mod is installed and the flag is on, the adapter will search for
unassigned child actors in the settlement and pair them — but the
actual pairing logic is deferred until the mod author's permission
lands and we know the mod's child FormIDs.

The adapter degrades gracefully: no crash, no visible change, no
dependency. The children are born, named, fed, and grow — they just
don't walk the world until 0.7.8's full integration.

**Rationale.**
The baby mod's permission was requested but not yet granted. Shipping
the infrastructure now means: (a) the sim-side journey is complete,
(b) the adapter is ready the moment permission lands, and (c) without
the mod, nothing breaks.

**Trade-offs.**
- Children stay sim-only until the mod is integrated — the player
  sees names in the log but no visible actors.
- The stub is intentional: shipping untested pairing logic without
  the mod's FormIDs would be fragile.
