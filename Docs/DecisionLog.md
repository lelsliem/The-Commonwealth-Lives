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
