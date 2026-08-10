# Walking — "The Farmer Walks to Market"

**Stone:** adapter 0.3.1 (after the executor)
**Status:** ✅ **VERIFIED in-game 2026-08-10** — every MoveTo issued the
command-mode travel package and live probe distances closed steadily
(min 85.6→47.8, 118.1→60.9, 722.7→524.7, …); settlers, traders included,
walked to the Sanctuary workshop (000250FE), observed in-game, and the
session ended with no crash. The pin is byte-verified statically (name
anchors + disassembly against Fallout4.exe 1.11.221, described below;
the first pin, 0xD77440, was wrong — the runtime byte check refused it
in-game and disassembly confirmed the error). The crashes blamed on the
call were proven to be a **corrupt save** (identical signature across
builds where the call never executed and a run with the DLL removed);
DisableExitSave prevents the exit-save cycle that produced it. The
decision half is unit-tested (MarketTest among the 9/9 suites).
**Related:** core ADR-0024 (adapters translate, don't simulate), ADR-0026.
The contract's guarantee this stone honors: **an intent is a hint, not a
command — the adapter decides how to walk the settler, and may refuse.**

---

## The Spec Is a Farmer

> A hungry farmer who knows where the market is decides to move to it.
> The adapter walks the farmer along the road. No quest script fired
> anywhere.

The executor stone proved the loop: frame hook → `Update` → `Decide` →
plan → execute → log. Every settler decided `Explore` (empty memory — no
destination to choose). This stone gives the decision a *destination* and
gives the destination *legs*:

1. **The market** — every mind is seeded with a Trade memory pointing at
   the settlement's workshop (REFR 000250FE, the Sanctuary workbench).
   A hungry mind that knows the market decides `MoveTo`; one that doesn't
   explores (MarketTest proves both halves).
2. **The road** — `MoveTo` executes through `Movement::WalkTo`, which now
   calls the game's own walk-to-point machinery.

---

## The walking call, pinned to 1.11.221

The earlier design assumed a single function `AIProcess::CreateMovementPlanner`.
The RE says otherwise: FO4's movement is **message/controller-driven**, and
the "walk to a point" primitive is on the NPC movement controller.

**The planner destination alone is not enough** — the probe proved it: the
planner activated every run, yet the settler stood still (the user's eyes
agree — nobody ever walked). A bare planner destination is a *hint*; the
settler's sandbox package keeps overriding the movement mode. The game
honors commands over packages, and "move here" is the command system — so
`WalkTo` now drives the real thing: **the command-mode travel package**,
the game's own "make this actor walk to that refr".

### What was found (the evidence)

- **The function is named twice.** The older name-bearing address database
  and the Fallout 4 public PDB (via the FO4VR address-library repo) both
  name it `Actor::InitiateCommandModeTravelPackage(TESObjectREFR*, COMMAND_TYPE)`
  — `this` = the actor being commanded, arg1 = the destination refr,
  arg2 = the command type — with `Actor::InitiateCommandModeActivatePackage`
  exactly 0x240 bytes later in both.
- **The first pin was wrong — the runtime byte-check caught it.** The old
  CSV gave the pair's *old-version* RVAs (0xD82300/0xD82540), and an early
  RE guessed 0xD77440/0xD77680 in 1.11.221 from a coincidental 0x240
  spacing. The in-game run refused at the prologue check (`pin mismatch at
  0x7ff679c27440`) and disassembly confirmed why: 0xD77440 is a
  stats/report loop, not a command initiator. The check exists exactly for
  this — a wrong pin refuses (never teleports) and logs the truth instead
  of crashing.
- **The real 1.11.221 address was located by anchoring, then verified by
  disassembly.** The old CSV's ID era doesn't bridge to the current
  address library (its numbering changed), so: names in CommonLibF4's
  IDs.h resolve to current-era IDs (verified: `SetCommandType` 2231826 →
  0xD00890 — the same RVA RE'd independently), the old database gives the
  same names' 1.10.163 RVAs, and the local old→new shift interpolates
  travel's old 0xD82300 to ~0xC6BD00. Scanning that window for real
  function starts (post-0xCC padding) and matching the old build's exact
  557-byte size found **0xC6BE90** (535B) — the only candidate that (a)
  takes `(actor, target refr, command type)` — rcx saved as `this`, rdx
  saved as the target, r8d as the type; (b) reads the target's position
  and writes it into the actor's AI process as the travel destination
  (`[process+0x28]`-relative, the tell that this is *travel*, not
  activate); (c) calls `SetCommandType` (0xD00890) with the passed type;
  and (d) sets the commanding flag and evaluates the package. Its twin
  0xC6AA80 (551B) is `Actor::InitiateCommandMode` — the "enter command
  mode" guard that bails unless `this == player`; travel is the
  self-contained "command this actor to go to that refr".
- `this` = the actor that walks. Every package-building call reads
  `[this+0x300]` (the actor's `currentProcess`) and the destination lands
  in *its* process; the other actor (read from a global) is the
  commander, used for the command-relationship markers. So the adapter
  calls it on the **settler**, with the market refr and `kMove` — the same
  shape the game uses when the player sends someone somewhere.

So `WalkTo`'s intended call is exactly one: the command-mode travel
package (0xC6BE90, the order — sandbox cannot override it) with `kMove`;
the function itself sets the command state, writes the destination, and
evaluates the package.

### The crash was the save — the call is re-enabled for its real test

The first in-game run with the real pin crashed the game on load
(`CTD upon trying to load`), and the initial diagnosis blamed the call:
heap corruption (`ucrtbase.dll`, `0xC0000409` — the CRT's fail-fast) at
a stable offset, with a theory that travel reads a global
commander/commanded-actor pointer the game only establishes in its
command-mode flow (`Actor::InitiateCommandMode`, 0xC6AA80), and that
calling it cold wrote through a stale pointer.

**That attribution was wrong, and the evidence is unambiguous.** The
identical crash signature hit:

- builds where the travel call **never executed** (the refusal build),
- a run with **our DLL removed entirely** (A/B test — plugin log
  unwritten, same crash at ~150s),
- a session with **only our plugin** loaded (survived 184s+,
  past every crash window, no crash event),
- a session with **zero plugins** (survived cleanly).

Every crash correlated with the **full plugin loadout**, and the user
confirmed the root cause independently: **the save was corrupt** (the
exit-save load-abort cycle left the world half-loaded with 610 temp
actors). Rolling back to a stable save, the game runs smooth with our
plugin — and the travel call itself was **never tested on a stable
save**. The command-state prerequisite theory is unproven.

**Current state: `WalkTo` issues the call** — one invocation of the
command-mode travel package (0xC6BE90, the order — sandbox cannot
override it) with `kMove`, byte-verified. This is the call's real test:
if it walks on the stable save, the stone is verified; if it still
crashes (now on a clean save), the command-state theory is real and the
pivot is either (a) the full command sequence — command-mode entry with
the settler as target, then travel — or (b) a pathing primitive that
needs no command state: compute a path with the game's pathing API
(`BSPathingRequest` → waypoints) and feed the waypoints into the
movement planner (`DoSetPlannerDirectControl`, 0xDC92F0) — the planner
calls never crashed; they just lost to the sandbox package, and the
waypoint path is the piece that was missing.

### The guard

`WalkTo` byte-verifies the pinned travel package (prologue
`48 89 5C 24 18 57 41 54` — `mov [rsp+0x18],rbx; push rdi; push r12`)
against the pinned value at runtime before refusing, so the refusal is
grounded in the real exe. A wrong pin → refuse (never teleport) + an
ERROR line printing the truth, so a layout surprise is diagnosed in one
in-game session (the tick stone's lesson: attribution, not assumption).

### The walk session

While the Trade memory lasts (~4.5s at the core's fade), the intent stays
`MoveTo` and would re-issue the planner every frame. The executor tracks a
per-entity walk session (target + when issued) and issues each walk once
(120s window). The game's planner then walks the settler to the
destination on its own — the walk completes even after the memory fades.

The session **outlives the intent**: the memory fades long before the
walk finishes, so the session is kept after the mind moves on, and
`ProbeWalks` measures it in its own pass — distance (in game units) as
the settler moves (≥1 u of progress, 2s apart; **silence means standing
still** — the sandbox-override verdict), closest approach, and a
one-time "reached" line. (First probe build ran the probe inside the
MoveTo case, so it died with the memory after ~4.5s — one line, no
trend; that's fixed.) A refused walk, a logged arrival, 120s passing, or
a world end clears the session.

The walker's position is read from its **data position** (`GetPosition()`
— `data.location`). The 3D node's world transform was the original
source and it lied: after a fast travel, streaming actors reported
positions 120,000+ units from where they stood (a walker ~700 units from
the bench "moved" 1.7 km in a frame), so arrivals never registered and
sessions died at the timeout. The data position tracks the actor while
loaded and stays sane (last known) when the cell unloads. A sanity gate
skips any reading beyond ~8× the session's first reading plus a 5,000
unit margin — a stream artifact is never counted as progress and never
corrupts the closest-approach minimum. The timeout is 120s so a slow
walk across the market radius (≈10,000 units ≈ 140 m) can finish; the
old 60s killed walkers mid-path.

---

## The market

- `kMarketFormId` = 0x000250FE — the placed reference
  `SanctuaryWorkshopREF` (base form: the vanilla `WorkshopWorkbench`
  "Workshop", 000C1AEB). **Verified from Fallout4.esm**: the record's
  EDID is literally `SanctuaryWorkshopREF`, and its DATA position
  (−79048, 89587 — far northwest, the northernmost workshop) matches the
  canonical settlement table; the whole 28-workshop scan lines up with
  known geography. The REFR, not the base: walking needs a placed object
  with a position (and `EnsureMarket`'s loaded check requires a REFR).
  The settlement's trade/food hub is its market for this stone.
- At `StartWorld`, if the form is loaded it becomes an entity (FormRef +
  translator entry), and every mind *within walking distance* of it
  (~10,000 units ≈ 140 m — all of Sanctuary village, excluding Red
  Rocket's ~13,000 and Abernathy's ~22,000) has its Memory seeded with
  `{ Trade, market, 1.0f }` (`SeedMarketMemory` with an include
  predicate; pure, in `src/Market.h`).
- The radius is the probe's lesson, not a guess: the first probe run
  showed walk orders issued to settler-faction actors standing at
  settlements kilometers away (Abernathy 22,017 units, Warwick 215,004 —
  each matched to its own workbench within meters), and all of them were
  being sent to the Sanctuary bench. A settler in Sanctuary knows the
  Sanctuary market; a settler at Warwick doesn't walk 3 km to trade
  (per-settlement markets are the refinement).
- If the form is not loaded (different settlement), the log says so and
  settlers explore until it is — the TargetLoaded refusal already handles
  the rest.
- The seed fades like any memory; arrival → re-remember (the trade that
  satisfies hunger) is the next stone's reinforcement work.

## Files

```
src/Movement.h/.cpp  — WalkTo: the pinned InitiateCommandModeTravelPackage
                       call (0xC6BE90, kMove) with the runtime byte guard
src/Market.h         — kMarketFormId + pure SeedMarketMemory
src/Adapter.h/.cpp   — EnsureMarket + seed at StartWorld; the walk session
tests/               — MarketTest (among the 9/9 suites): seeded mind
                       decides MoveTo; bare mind explores
```

## Test plan

| Where | Proves |
|-------|--------|
| Adapter tests (on every build) | **MarketTest** — a hungry settler seeded with the market memory decides `MoveTo` after `Update`; the same mind without it explores. The decision half of the farmer's road. |
| In-game (author) | ✅ **Verified 2026-08-10**: deploy the DLL, load a stable save, stay in-game. Only nearby minds get the market memory (radius-scoped). The log shows `LCE: WalkTo — command-mode travel package issued` for every MoveTo, then `walk probe settler X -> 000250FE d = ... m (min ... m) [live]` lines with **d closing** — verified live: min 85.6→47.8, 118.1→60.9, 722.7→524.7, … — and settlers, traders included, walked to the Sanctuary workshop (observed in-game, no crash). A `reached` line awaits an arrival-radius refinement (closest was ~48 units when the session ended). Markers on the latest build: `Tick: called before the world started` and `Tick: first pass complete (N intents, M walks)`. **The game's exit-save reload kills the sim**: a PreLoadGame fires ~0.1s after the world wakes (loading the Exitsave left by the previous quit) with no GameLoaded following — EndWorld runs and the sim stays dead, which read as "nobody walks". The adapter revives the world when a pending load never completes, and the **DisableExitSave mod now prevents the cycle at the source** (the corrupt exit save was the crash: identical ucrtbase 0xC0000409 in runs without the DLL). The recovery window is 60s: real loads are slow (a 600-actor co-save world can take 15-30s — the old 12s window fired mid-load and its revival killed the load), and only a load that outlives the window is treated as dead. The revival world after an abort can seed 600+ settler-faction actors — the executor caps concurrent walks (16) and a refused walk erases its session. Failure modes are logged: `market not loaded`, the pin-mismatch ERROR, or `WalkTo refused — no actor or no AI process` (000B0EEE/050049D9 refuse every session — persistent null AI process, logged but harmless). |

## Decisions (resolved)

1. **The walking call is the command-mode travel package, not a planner
   call** — the earlier design (a bare `CreateMovementPlanner`-style
   destination) lost to the sandbox package; the probe proved it. The
   game honors commands over packages, so `WalkTo` issues the real
   "move here": `Actor::InitiateCommandModeTravelPackage` (0xC6BE90,
   kMove), found by anchoring (IDs.h × old database × current bin) and
   verified by disassembly, with a runtime byte guard.
2. **The market is seeded memory, not a quest** — the adapter reports
   "the settler knows where to trade"; the core reasons over it
   (ADR-0024). No scripts, no content files.
3. **One walk per session** — the executor's walk session keeps the
   planner from being re-issued every frame while the memory lasts.
4. **Never teleport, never fake it** — a refused walk logs the truth and
   the sim re-decides; the byte guard keeps a wrong pin harmless.
