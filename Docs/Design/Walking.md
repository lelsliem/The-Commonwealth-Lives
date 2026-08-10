# Walking — "The Farmer Walks to Market"

**Stone:** adapter 0.3.1 (after the executor, verified in-game pending)
**Status:** Blocked on a game-side prerequisite — the walking call is
**pinned and verified statically** (name anchors + disassembly against
Fallout4.exe 1.11.221, described below; the first pin, 0xD77440, was
wrong — the runtime byte check refused it in-game and disassembly
confirmed the error), but **calling the verified function cold crashed
the game** (heap corruption, 0xC0000409 — it needs the game's
command-mode state, which the adapter doesn't drive). `WalkTo` now
refuses with a logged reason; the next stone is the command sequence or
a pathing primitive. The decision half is unit-tested (MarketTest, 5/5
suites green).
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

### The call crashed — the game's command state is a prerequisite

The first in-game run with the real pin crashed the game on load
(`CTD upon trying to load`, every run). Two pieces of evidence pin the
cause:

- **The plugin log stops mid-first-pass.** The plan always orders the
  MoveTo (0001CA7D) right after the Explore lines; the log ends on the
  last Explore with no `decides MoveTo`, no `first pass complete` — the
  crash is inside the travel call.
- **The crash signature is heap corruption, not a null deref.** Windows
  Event Viewer: `ucrtbase.dll`, `0xC0000409` (STATUS_STACK_BUFFER_OVERRUN
  — the CRT's fail-fast, raised on detected heap corruption) at the same
  offset in both the flood crash and the load crash.

Why: the travel function reads a global commander/commanded-actor pointer
and writes through it (`mov edx,[rbx+0x20]` / `mov [rbx+0x20],edx`),
state the game only establishes in its command-mode flow
(`Actor::InitiateCommandMode`, 0xC6AA80 — the twin function the dispatch
uses before travel). Calling travel cold, without that state, wrote
through a stale pointer and corrupted the heap.

**Decision: `WalkTo` now refuses** (with a logged reason) until the
adapter either (a) drives the full command sequence — player command-mode
entry with the settler as target, then travel — which needs internal
command-target state we can't safely set, or (b) switches to a pathing
primitive that needs no command state: compute a path with the game's
pathing API (`BSPathingRequest` → waypoints) and feed the waypoints into
the movement planner (`DoSetPlannerDirectControl`, 0xDC92F0) — the
planner calls never crashed; they just lost to the sandbox package, and
the waypoint path is the piece that was missing. Refusing is the
contract: never crash, never teleport, and the log names the blocker.

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
(60s window). The game's planner then walks the settler to the destination
on its own — the walk completes even after the memory fades.

The session **outlives the intent**: the memory fades long before the
walk finishes, so the session is kept after the mind moves on, and
`ProbeWalks` measures it in its own pass — distance as the settler moves
(≥1 m of progress, 2s apart; **silence means standing still** — the
sandbox-override verdict), closest approach, and a one-time "reached"
line. (First probe build ran the probe inside the MoveTo case, so it died
with the memory after ~4.5s — one line, no trend; that's fixed.) A
refused walk, a logged arrival, 60s passing, or a world end clears the
session.

The walker's position is read live from its **3D node** (`Get3D()` →
`GetWorldTransform().translate`) when available, falling back to
`GetPosition()` (the stored `data.location`) and **tagging which one was
used** (`[live]` / `[stored]`). A hard skip on a missing 3D node once
silenced the probe entirely — the fallback guarantees a reading every
tick. (`GetPosition()` is the save-time position and never changes while
an actor moves; it's also why the cross-map distances matched settlement
workbenches so exactly — those were saved positions of settlers standing
at their own workbenches.)

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
tests/               — MarketTest (5/5 suites green): seeded mind decides
                       MoveTo; bare mind explores
```

## Test plan

| Where | Proves |
|-------|--------|
| Adapter tests (on every build) | **MarketTest** — a hungry settler seeded with the market memory decides `MoveTo` after `Update`; the same mind without it explores. The decision half of the farmer's road. |
| In-game (author) | Pending: deploy the DLL, load Sanctuary, stay in the game ~30s, paste the tail. Only nearby minds get the market memory (radius-scoped: 10 of 11 settlers correctly Explore, one decides MoveTo). Expected (once a safe walk mechanism is in): `LCE: WalkTo — command-mode travel package issued` then `walk probe settler X -> 000250FE d = ... m (min ... m) [live]` lines as the settler moves (≥1 m of progress, 2s apart) with **d closing** — then one `settler X reached the market (d = 1.2 m)` line. **Current state: `WalkTo` refuses by design** — the travel call crashed the game (heap corruption, 0xC0000409) because it needs the game's command-mode state; the next stone is the command sequence or the pathing waypoint approach (see above). Markers on the latest build: `Tick: called before the world started` and `Tick: first pass complete (N intents, M walks)`. **The game's exit-save reload kills the sim**: a PreLoadGame fires ~0.1s after the world wakes (loading the Exitsave left by the previous quit) and aborts ~9s later without a GameLoaded — EndWorld runs and the sim stays dead, which read as "nobody walks". The adapter now revives the world when a pending load never completes (`lifecycle: the pending load aborted — reviving the world.` ~12s after PreLoadGame), and every GameLoaded rebuilds fresh. **The revival world after an abort can seed 600+ settler-faction actors** (the aborted load's temp actors), all within the market radius — a flood of walks. The executor caps concurrent walks (16) and a refused walk now erases its session (the old reset left a zombie session that ProbeWalks logged as an instant "ended" every frame — the flood that preceded the crash). Failure modes are logged: `market not loaded`, the pin-mismatch ERROR, or `WalkTo refused — no actor or no AI process` (000B0EEE/050049D9 refuse every session — persistent null AI process, logged but harmless). |

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
