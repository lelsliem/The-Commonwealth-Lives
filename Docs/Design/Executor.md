# Intent Executor — "The Farmer Walks"

**Stone:** adapter 0.3 (after translation, verified in-game)
**Status:** Design — pending review
**Related:** core ADR-0024 (adapters translate, don't simulate), ADR-0026
(free functions over static classes), Law 001 (simple things; compose the
complex). The contract's guarantee this stone honors: **an intent is a
hint, not a command — the adapter decides how to walk the settler, and may
refuse.**

---

## The Spec Is a Farmer

> A hungry farmer who knows and trusts the merchant decides to move to
> them. The adapter walks the farmer along the road. No quest script
> fired anywhere.

That sentence is the test plan. Translation proved the settlers are minds.
This stone gives the minds hands: the simulation ticks inside the game,
intents are read, and at least one action — `MoveTo` — actually executes.

---

## The Shape: a tick, a read, a table

```
every frame (game thread)
  ├─ Adapter::Tick(delta)          — hooked to the game's main loop
  │    ├─ Update(registry, delta)  — the core's stateless tick: needs
  │    │                             decay, memory fades, Decide runs
  │    └─ Executor::Run()          — read intents, execute via the table
  └─ refuse = drop the intent      — the sim re-decides next tick
```

Three pieces, all inside the adapter (the core stays untouched):

### 1. The tick — a frame hook

The plugin's own heartbeat becomes the simulation's: a **per-frame hook on
`RE::Main::Update`**, installed once at init via the library's own
mechanism (`REL::THook` registered in `FHookStore`, enabled at
`InitHook`; the trampoline comes from `F4SE::Init` with
`{ .trampoline = true }`). The exact hook ID resolves from the address
library at implementation.

- Runs on the **game thread** — zero contention, trivially debuggable
  (the contract's threading decision for 0.4.0).
- `delta` = **real seconds** since the last frame (the game clock). The
  sim's "per second of simulation time" maps to wall time for now;
  time-scale is a future tuning input from Configuration.

### 2. The read — intents are components

After `Update`, every mind has a fresh `Intent` component (or none — a
mind with no decision is left alone). The executor sweeps minds with
`ForEachWithComponent<Intent>` and builds a small action plan — a pure,
testable step:

```
for each (entity, intent):
    actor      = Translator.FormFor(entity)      → the loaded RE::Actor?
    target     = Translator.FormFor(intent.Target) → the target form?
    → plan: (entity, intent.Action, actor, target) or refuse
```

Refusal is the contract, honored cheaply: actor not loaded, target not
loaded (they're in another cell — the merchant hasn't loaded yet), or the
actor is already acting. A refused intent is **dropped**; the sim computes
a fresh one next tick. Nothing is queued, nothing blocks.

### 3. The table — Intent → game action (the adapter's art, ADR-0024)

| Intent | The adapter does | This stone |
|--------|------------------|------------|
| `MoveTo{target}` | walk the actor to the target's location | **implemented** — via the AI process movement planner (see below) |
| `Flee{threat}` | flee the remembered threat | logged, table slot reserved |
| `Rest` | wait / sleep | logged, table slot reserved |
| `Socialize`, `Explore`, `Work` | conversation scene / wander / work | logged, table slot reserved |

**MoveTo, concretely:** FO4's `AIProcess` has a movement planner — the
call every serious mod uses to make an NPC genuinely walk to a point.
It is declared in the adapter via `REL::Relocation` against the address
library (the address resolves at implementation; the fallback, if the
address proves unreliable, is a documented teleport stopgap so the loop
is provable either way). The target position comes from the target form's
`GetPosition()` — resolved through the Translator, never from the core.

Logging discipline: intents are logged when they **change**, not every
frame; a per-settler log line like
`settler 0x12345 decides MoveTo 0x67890 (0.82)` proves the loop without
spam.

---

## What stays OUT

- No game knowledge in the core — every `RE::` touch lives in the
  executor (ADR-0024, ADR-0023 is a build fact).
- No simulation logic in the adapter — the executor never scores a need
  or chooses an action; it only executes what `Decide` produced.
- No new dependencies — the hook, trampoline, and movement planner are
  all already in the library or the address library.

## Dependencies

- **Upward:** nothing.
- **Downward:** CommonLibF4 (`RE::Main`, `RE::AIProcess`, `REL::THook`,
  `REL::FHookStore`, `REL::IDDB`), the core (`Update`, `Intent`,
  `ForEachWithComponent`), our own `Translator`, standard C++.
- **Third-party:** none new.

Files:

```
src/Tick.h/.cpp       — the frame hook (small: install, call Adapter::Tick)
src/Executor.h/.cpp   — the read + the table; pure plan-building, game execution
src/Adapter.h/.cpp    — gains Tick(delta); owns the Executor
src/main.cpp          — install the hook at init; init the trampoline
tests/                — plan-building suites (below)
```

## Test plan

| Where | Proves |
|-------|--------|
| Adapter tests (on every build) | The plan builder: given registry intents + translator + a loaded-actor set, produces the right plan and the right refusals (unloaded actor, unloaded target, already-acting) — pure, no game required. |
| In-game (author) | Needs decay over real time; intents appear in the log (`settler ... decides MoveTo ...`); a `MoveTo` executes — a settler walks. The farmer's road. |

## The four questions

- **Can it be simpler?** The tick could be a Papyrus timer — but that
  adds content files and a script; the frame hook is already in the
  library and keeps the mod code-only. No.
- **Does it belong?** Yes — every `RE::` touch is at the edge (ADR-0024);
  the core gains nothing.
- **Do we need it at all?** Yes — minds without hands are thoughts. This
  stone is the loop that makes the world act.
- **Will this help build living worlds through simulation?** Yes — the
  farmer walks.

## Decisions to review

1. **Tick via a frame hook on `RE::Main::Update`** (trampoline from
   `F4SE::Init`) over a Papyrus timer — code-only, no content files.
2. **MoveTo via the AI process movement planner** — real walking; address
   resolved from the address library at implementation, teleport
   stopgap documented if it fails.
3. **One action end-to-end this stone** — `MoveTo` implemented; the other
   four actions get table slots and log lines. The loop is the proof.
4. **Real-time delta for now** — time-scale becomes a tuning input later.
5. **Refusal = drop the intent** — the hint-not-command guarantee, made
   concrete.
