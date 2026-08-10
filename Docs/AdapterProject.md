# The Living Commonwealth — Fallout 4 Adapter Project

**Handoff document.** This file exists so a new agent (a new Freebuff tab
rooted at `C:\Fallout4Adaption`) can start this project with full context —
what exists, how it is wired, and what is next — without having been part
of the conversation that built it.

**Companion documents** (in the LCE core repo, `C:\LivingCommonwealthEngine`):
- `Docs/Architecture/PlatformIntegration.md` — the boundary contract this
  project implements (read this first)
- `Docs/ProjectPhilosophy.md` — the Six Design Laws, especially
  Law 001: *simple things; compose the complex*
- `Docs/LearningPath.md` — how the engine works

**Design docs in this repo** (`Docs/Design/`): `Executor.md` (the tick +
intent executor), `Walking.md` (the walk), `CoSave.md` (the co-save
record). Each is flipped to **verified in-game** as its stone lands.

---

## The Project

**Name:** The Living Commonwealth (the mod). The Fallout 4 adapter for the
Living Commonwealth Engine (LCE). An F4SE plugin (GPL — it links
CommonLibF4) that is a *client* of LCE.Core (MIT). It does NOT
reimplement simulation; per `PlatformIntegration.md` the boundary is the
core's public API: the adapter calls `CreateEntity` / `DestroyEntity` /
`Remember` / `Update` and reads intents via `GetComponent<Intent>`.

**The contract's guarantee:** *an intent is a hint, not a command* — the
adapter decides how to walk the settler, and may refuse. The core never
names a game action and never appears in the adapter's game-facing code.

**The game setup:** Fallout 4 1.11.221 (Next-Gen), F4SE 0.7.8, run through
MO2 (`B:\Modding\MO2`). The plugin logs to
`C:\Users\mrlma\Documents\My Games\Fallout4\F4SE\TheLivingCommonwealth.log`.

---

## Status — four stones, all verified in-game

| Version | Stone | Status |
|---------|-------|--------|
| 0.1.0 | Scaffold + heartbeat | ✅ verified in-game |
| 0.2.0 | Translation | ✅ verified in-game — settler-faction actors become minds |
| 0.3.0 | Intent executor | ✅ verified in-game — tick + settlers walk to market |
| 0.4.0 | Co-save | ✅ verified in-game — 637 entities saved and restored |
| 0.5.0 | Living world ("The Settler Goes to Market") | ⬜ next |

**Build:** `xmake` (one command). Two targets:
`TheLivingCommonwealth` (the DLL) and `TheLivingCommonwealth.Tests`
(the harness — links LCE.Core only, no game; run as
`xmake run TheLivingCommonwealth.Tests`). **6/6 suites green.**

---

## Architecture — how the adapter is wired

```
src/main.cpp        F4SE entry: hooks (Tick), messaging (lifecycle),
                    serialization callbacks (co-save glue)
src/Adapter.h/.cpp  the one world object: registry + translator, the
                    lifecycle, the tick loop (Update → plan → execute → probe)
src/Translator.h/.cpp  formId ↔ EntityId tables (adapter state, game-free)
src/SimRelevant.h/.cpp the settler predicate (WorkshopNPCFaction 0x000337F3)
src/Executor.h/.cpp    pure plan builder — refusals are the contract
src/Movement.h/.cpp    WalkTo: the pinned Actor::InitiateCommandModeTravelPackage
src/Market.h           the market seed (Sanctuary workshop REFR 000250FE)
src/Serialization.h/.cpp  per-type serializers (Needs, Memory, …)
src/CoSave.h/.cpp     the durable co-save record (stable names, versioning)
src/BlobCodec.h       little-endian byte codec
src/Components.h      FormRef + SeededNeeds (adapter-defined components)
src/Tick.h/.cpp       the per-frame VM-tick hooks
```

**Lifecycle mapping (current, verified):**

| Game event | The adapter does |
|------------|------------------|
| `kGameLoaded` (startup) | `StartWorld`: translate loaded sim-relevant actors into entities; seed the market memory |
| `kPostLoadGame` (a save finished loading) | Apply the co-save restore (or start fresh) — **the load's completion event; `kGameLoaded` alone does not fire for real loads** |
| `kPreLoadGame` | `EndWorld` (Clear; serializers survive); arm abort recovery (60s) |
| `kDeleteGame` | `EndWorld` |
| serialization save callback | `CaptureWorld` → `CoSave::Encode` → `WriteRecord` |
| serialization load callback | read record → `CoSave::Decode` → `QueueRestore` (applied on `kPostLoadGame`) |
| new game (revert callback) | `EndWorld` |

**The sim loop** (per frame, game thread): `Update(registry, delta)` →
`BuildPlan` (pure; loaded/available refusals) → `ExecutePlan` (the walk
issue; sessions capped at 16) → `ProbeWalks` (live distance probes).

**The co-save record:** the core's snapshot is process-local (component
keys are `std::type_index`) — the adapter's record translates it to stable
names (`needs`, `memory`, `relationships`, `goals`, `intent`, `formref`)
with its own version (`kRecordVersion = 1`). Refusals are the contract:
truncated records, unsupported versions, and unknown component names
never half-apply. Save-compatibility is the adapter's job.

**Two environment lessons learned (documented in Walking.md / CoSave.md):**
- The crash saga: a corrupt save, not the plugin — the identical
  fail-fast signature hit no-DLL runs. DisableExitSave prevents the
  exit-save cycle.
- Every "load abort" was our own recovery timer: real loads are slow
  (a 600-actor co-save world takes >12s), and the completion event is
  `kPostLoadGame`.

---

## The 0.5.0 roadmap — "The Settler Goes to Market"

The core is at `0.5.0` stone 01 (15/15 suites green): tuning ergonomics —
`SimulationTuning::FromConfiguration(config)` — is live. Known keys
(`sim.memory.fade`, `sim.goal.urgency`, …) override defaults; broken or
unknown keys never break the world. The adapter should feed tuning from a
text file users can edit (its own keys ride in the same file).

Remaining stones for the adapter, and what they need from the core:

1. **World facts via `Remember`** — shipped and proven in the core
   (0.3.1). Weather, market open/closed, road conditions: push as memory
   events with an *invalid* Other. While remembered, the interaction is
   unavailable to the mind; when it fades, it reopens. The adapter
   controls duration by re-pushing.
2. **Tuning from Configuration** — ✅ core stone 01. One call, one file.
3. **The real test — a settler goes to market because they are hungry.**
   Needs decay → goal urgency grows → a `MoveTo`-class intent → the
   executor walks them (proven). Locations stay out of the core; the
   adapter resolves "which trader".
4. **The outcome channel** (core stone 02, not yet built). After the
   walk, the adapter reports what *actually* happened — trade done,
   robbed en route, road blocked — and the sim turns it into memory and
   relationship changes. The loop's final leg; not a blocker for the
   first test.

---

## Dependency Wiring

- **CommonLibF4** — the game API + plugin contract, from the local clone
  (`Depends/commonlibf4`); its `RUNTIME_LATEST` (1.11.221) matches the game.
- **F4SE** — runtime-only; CommonLibF4 replaces it as the static dependency.
- **LCE.Core** — linked statically, built by its own CMake via the
  `lce.core` rule into `Build/core` (never touching the core repo's
  `Build/`). The rule pins the core version (refuses below 0.4.0).
  Point at another checkout with `LCE_CORE_PATH`.
- **spdlog** — the local clone feeds the core build's v1.16.0; the mod
  logs through LCE's API and `REX::LOG`, never spdlog directly.

---

## Working Conventions (follow these)

1. **One stone at a time:** design document → headers → source → tests →
   docs → green build. A milestone is not done until the docs claim it
   truthfully — and not verified until proven in-game.
2. **The four questions:** *Can it be simpler? Does it belong? Do we need
   it at all? Will this help build living worlds through simulation?*
3. **The quote ritual:** every new `.h`/`.cpp` gets a banner with a
   reserved line for a joke or quote. Leave the slot.
4. **Commit style:** concise messages that say *why*, one coherent change
   per commit, never commit `Build/` artifacts.
5. **No global state.** Time and tuning are inputs.
6. **Test rhythm:** pure pieces are unit-tested without the game
   (translator, serializers, record codec, plan builder, market seed);
   game-facing pieces are verified in-game via log lines, then the docs
   flip to verified.

---

## Open Items (author-side)

- The F4SE-assigned serialization UID — placeholder `'LCEW'` in
  `src/CoSave.h` (`kSerializationUid`) until assigned.
- The plugin author handle (TODO in `xmake.lua`).
- The banner quote slots (every file's reserved line).
- The Nexus name check before publishing.
