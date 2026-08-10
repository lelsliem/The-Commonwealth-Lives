# Walking — "The Farmer Walks to Market"

**Stone:** adapter 0.3.1 (after the executor, verified in-game pending)
**Status:** Implemented — the walking call is **pinned and verified
statically** (RTTI chain against Fallout4.exe 1.11.221, described below);
the decision half is unit-tested (MarketTest, 5/5 suites green); the walk
itself is **pending in-game verification** — the same discipline as the
tick stone: a runtime vtable check guards the call, and one log line tells
the truth.
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
honors commands over packages ("move here" is the command system), so
`WalkTo` now also marks the walk as a command —
`AIProcess::SetCommandType(COMMAND_TYPE::kMove)`, wrapped with a current
address-library ID — before driving the planner. Command + destination =
the walk the game itself would produce.

### What was found (the evidence)

- `Actor::movementController` (CommonLibF4, +0x318) is a `MovementControllerNPC`:
  a 0x1A8-byte object — the AI base (`MovementControllerAI`, holding the
  active-arbiter set) at +0x00, the NPC subobject at +0x138 (RTTI chain:
  `.?AVMovementControllerNPC@@` → 7 COLs → 7 vtables; destructor frees
  exactly 0x1A8).
- The NPC subobject's vtable (**0x2567B68**) carries the `DoSet*` family —
  the movement-mode switches (`DoSetPlannerDirectControl`, etc. — the
  names come from the older name-bearing address database; the functions
  themselves are confirmed by structure).
- Slot [2] (**0xdc92f0**) is the one we want: it calls the "activate
  planner" virtual, finds the planner arbiter in the controller's active
  set, and sets the destination NiPoint3 on it. Its setter writes the
  arbiter's destination fields; the arbiter then drives the DirectControl
  agent — the game's real pathing.
- The slots compute their AI base as `this - 0x138`, which lands on the
  active-arbiter container the AI ctor initialized — confirming `this` is
  the NPC subobject at controller+0x138.

Not in CommonLibF4, not in the address library — pinned by the RTTI chain
against the user's exe, the same discipline F4SE uses for its offsets.

### The guard

`WalkTo` verifies the subobject's vtable against the pinned value at
runtime before calling. A wrong pin → refuse (never teleport) + an ERROR
line printing the actual vtable, so a layout surprise is diagnosed in one
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
src/Movement.h/.cpp  — WalkTo: the pinned DoSetPlannerDirectControl call
                       (vtable 0x2567B68, slot [2], this = controller+0x138)
                       with the runtime vtable guard
src/Market.h         — kMarketFormId + pure SeedMarketMemory
src/Adapter.h/.cpp   — EnsureMarket + seed at StartWorld; the walk session
tests/               — MarketTest (5/5 suites green): seeded mind decides
                       MoveTo; bare mind explores
```

## Test plan

| Where | Proves |
|-------|--------|
| Adapter tests (on every build) | **MarketTest** — a hungry settler seeded with the market memory decides `MoveTo` after `Update`; the same mind without it explores. The decision half of the farmer's road. |
| In-game (author) | Pending: load Sanctuary, stay in the game ~30s, paste the tail. Only nearby minds get the market memory (radius-scoped: 10 of 11 settlers correctly Explore, one decides MoveTo). Expected: `walk probe settler X -> 000250FE d = ... m (min ... m)` lines as the settler moves (≥1 m of progress, 2s apart) with **d closing** — then one `settler X reached the market (d = 1.2 m)` line. Closing d → the pinned planner drives the walk; **silence after the first probe line** → standing still (sandbox override), fix is the command system. Markers on the latest build: `Tick: called before the world started` and `Tick: first pass complete (N intents, M walks)`. **The game's exit-save reload kills the sim**: a PreLoadGame fires ~0.1s after the world wakes (loading the Exitsave left by the previous quit) and aborts ~9s later without a GameLoaded — EndWorld runs and the sim stays dead, which read as "nobody walks". The adapter now revives the world when a pending load never completes (`lifecycle: the pending load aborted — reviving the world.` ~12s after PreLoadGame), and every GameLoaded rebuilds fresh. Failure modes are logged: `market not loaded`, the vtable-mismatch ERROR, or `WalkTo refused — no actor or no AI process` (000B0EEE/050049D9 refuse every session — persistent null AI process, logged but harmless). |

## Decisions (resolved)

1. **The walking call is the movement controller's DoSetPlanner slot, not
   a function named `CreateMovementPlanner`** — the earlier name was an
   assumption; the RTTI chain found the real machinery. Pinned by vtable
   + slot index (0x2567B68, slot [2] = 0xdc92f0), with a runtime guard.
2. **The market is seeded memory, not a quest** — the adapter reports
   "the settler knows where to trade"; the core reasons over it
   (ADR-0024). No scripts, no content files.
3. **One walk per session** — the executor's walk session keeps the
   planner from being re-issued every frame while the memory lasts.
4. **Never teleport, never fake it** — a refused walk logs the truth and
   the sim re-decides; the vtable guard keeps a wrong pin harmless.
