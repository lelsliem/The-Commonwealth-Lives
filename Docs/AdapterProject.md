# The Living Commonwealth — Fallout 4 Adapter Project

**Handoff document.** This file exists so a new agent (a new Freebuff tab
rooted at `C:\Fallout4Adaption`) can start this project with full context —
the plan, the conventions, and the contract — without having been part of
the conversation that built the engine.

**Companion documents** (in the LCE core repo, `C:\LivingCommonwealthEngine`):
- `Docs/Architecture/PlatformIntegration.md` — the 0.4.0 boundary design
  (read this first; it is the contract this project implements)
- `Docs/LearningPath.md` — how the engine works and what each piece teaches
- `Docs/ProjectPhilosophy.md` — the Six Design Laws, especially
  Law 001: *simple things; compose the complex*

---

## The Project

**Name:** The Living Commonwealth (the mod). The Fallout 4 adapter for the
Living Commonwealth Engine (LCE). Chosen because Fallout 4's setting *is*
the Commonwealth, and this mod makes it live.

**Repo naming:** `the-living-commonwealth` (or `lce-fallout4` — decide at
scaffold time, then name the repo after the public name).

**License:** GPL — the mod links CommonLibF4 (GPL-3.0 with modding/linking
exceptions). The core stays MIT. This split is deliberate and physical: the
adapter lives in its own repo and never shares a tree with the core.

**Check:** verify the name is not already taken on Nexus Mods before
publishing.

---

## What This Project Is

The adapter is a **client** of LCE.Core — exactly like the engine's test
harness, but living in its own repository and talking to a game. It does
NOT reimplement simulation. Per `PlatformIntegration.md`, the boundary is
the core's public API:

- The adapter **calls**: `CreateEntity`, `DestroyEntity`, `Remember`
  (experiences and world facts), `Update` — and **reads** intents via
  `GetComponent<Intent>`.
- The adapter **guarantees**: an intent is a *hint*, not a command — the
  adapter decides how to walk the settler, and may refuse.
- The core **promises**: no game knowledge, no queries of the world, a
  stateless tick.

The adapter is an F4SE plugin built on CommonLibF4 (`F4SE::Init`,
`GetMessagingInterface()`, `GetSerializationInterface()`, `GetTaskInterface()`,
`RE::` types).

---

## The core's 0.4.0 side is built — what the adapter gains

The core is now `0.4.0-alpha`, 14/14 suites green. Two things changed
that matter to this project:

1. **The boundary is the public API only.** The old
   `Include/LCE/Interfaces/` stubs (`IGameAdapter`, `IWorld`, `IEntity`)
   are **deleted**. The adapter is a client of the core — nothing to
   implement, nothing to include.
2. **Save/load has its substrate.** The core now ships `RegistrySnapshot`
   (`Include/LCE/Simulation/RegistrySnapshot.h`) and four registry
   operations the co-save stone will use:
   - `RegisterSerializer<T>({ serialize, deserialize })` — required for a
     component type to appear in a snapshot. Register once at init for
     every type the adapter uses (Needs, Memory, Relationships, Goals,
     Intent, and any adapter-defined components).
   - `Capture()` — the whole registry as pure data; entity identities
     (index + generation) preserved exactly.
   - `Restore(snapshot)` — rebuilds a registry with identical IDs;
     requires the same serializers to be registered.
   - `Clear()` — blank registry; serializers survive (register once,
     reuse across games).

   Semantics to respect:
   - A component type with **no serializer is not persisted** — omitted
     silently. Data presence decides membership.
   - The snapshot is a **process-local exchange format**. The adapter
     translates it into the F4SE co-save record with its own stable type
     names and its own versioning — save-compatibility is the adapter's
     job (migrate old saves on load).
   - Snapshot components are keyed by `std::type_index` — stable within
     a process, NOT across processes; another reason the durable record
     needs the adapter's own names.

   The lifecycle table below is now implementable: `GameLoaded` → create
   entities; `PreSaveGame` → `Capture()` → co-save record; `PostLoadGame`
   → read record → `Restore()`; `PreLoadGame`/`DeleteGame` → `Clear()`.
   The round-trip is proven by the core's Snapshot suite — the farmer
   still goes to market after a save and a load.

---

## Dependency Wiring

`C:\Fallout4Adaption\Depends\` holds the clones the build needs:
`commonlibf4` (the static dependency, built via `includes`) and `spdlog`
(offline source for the core's spdlog 1.16). The original six were trimmed
in 2026-08 — see `Depends/README.md` for what was removed and why.

- **CommonLibF4** — the mod's game API + plugin contract. Built from the
  local clone; its `RUNTIME_LATEST` (1.11.221) matches the game.
- **F4SE** — runtime-only. The plugin does not link the F4SE source;
  CommonLibF4 replaces it as the static dependency (its README says so).
  The `f4se_1_10_*.dll` runtime is a download, installed via the mod
  manager.
- **LCE.Core** — linked statically, built by its own CMake via the
  `lce.core` rule into `Build/core` (never touching the core repo's
  `Build/`). Points at the local checkout; override with `LCE_CORE_PATH`.
- **spdlog** — the local clone feeds the core build's `v1.16.0` (matching
  the plugin's xrepo spdlog, `std::format` mode) so one spdlog serves the
  DLL. The mod itself logs through LCE's API and `REX::LOG`, never spdlog
  directly.

---

## Lifecycle Mapping (the mod's heartbeat)

| Game event | The adapter does |
|------------|------------------|
| `GameLoaded` | Create the registry; translate each sim-relevant Actor into an entity with components |
| game tick | `Update(registry, delta)`; read intents; execute via `RE::` |
| world events | `Remember` — experiences and world facts ("the market is closed" = `{ invalid, Trade, weight }`) |
| `PreSaveGame` / `PostSaveGame` | Snapshot the registry into the co-save record; release |
| `PreLoadGame` / `PostLoadGame` | Clear the registry; restore from the co-save record |
| `DeleteGame` | Clear everything |

Translation rules: components ↔ game data (a settler's `Hunger` ↔ an
`ActorValue` write through `RE::Actor`); intents ↔ game actions (`MoveTo`
→ a movement AI package; `Flee` → a flee package; `Rest` → wait/sleep);
locations stay out of the core — intents target entities, the adapter
resolves the road.

---

## Working Conventions (how the engine was built — follow these)

1. **The build rhythm, one stone at a time:**
   design document → headers → source → tests → docs → green build.
   A milestone is not done until the docs claim it truthfully.
2. **The four questions** for every piece of design:
   *Can it be simpler? Does it belong? Do we need it at all? Will this help
   build living worlds through simulation?*
3. **The quote ritual:** every new `.h`/`.cpp` gets a banner with a line
   reserved for a joke or quote from the author. Leave the slot; the author
   fills it. Tests get compact headers, no banners.
4. **Version ritual:** on milestone completion, bump the version everywhere
   (`Version.h`, `CMakeLists.txt`, README badge) and commit after the
   author approves. The author reads and questions everything — no question
   is too basic; explain the *why*, not just the *what*.
5. **Commit style:** concise messages that say *why*, one coherent change
   per commit, no stray files, never commit `Build/` artifacts or tool dirs
   (`.gitignore` first).
6. **No global state** (ADR-0014). Time and tuning are inputs.

---

## First Stones (the initial commit list)

1. `git init` + `.gitignore` (learn from the core's: `/Build/`, `.vs/`,
   `*.user`, tool dirs).
2. Project scaffold: `CMakeLists.txt` (C++23, `/W4 /WX`, static runtime —
   match the core's compiler config), dependency wiring (CommonLibF4 +
   F4SE + LCE.Core), `F4SEPlugin_Load` entry point with version banner and
   quote slot.
3. A hello-world plugin that loads in Fallout 4 and logs through LCE —
   the "first heartbeat" of the mod.
4. Then the real stones: entity ↔ form translation, intent executor,
   co-save, and the real test: *a settler goes to market because they are
   hungry — no script.*

---

## Milestones

| Version | Stone | Status |
|---------|-------|--------|
| 0.1.0 | Scaffold + heartbeat | ✅ verified in-game |
| 0.2.0 | Translation | ✅ verified in-game |
| 0.3.0 | Intent executor | implemented — tick verified in-game; walking pinned, in-game walk pending |
| 0.4.0 | Co-save | ⬜ |
| 0.5.0 | Living world (the market test) | ⬜ |

See `Docs/Roadmap.md`.

## Scaffold Decisions (2026-08) — the open questions, answered

- **Name:** project and DLL are `TheLivingCommonwealth`; display name
  "The Living Commonwealth". Repo slug (when published):
  `the-living-commonwealth`. Nexus check still pending.
- **Build system:** xmake (CommonLibF4 2.x has no CMake; its
  `commonlibf4.plugin` rule generates the plugin contract). The core stays
  CMake and is driven from the `lce.core` rule into `Build/core`.
- **Dependencies:** the local `Depends/` clones (gitignored; provenance in
  `Depends/README.md`). The vendored CommonLibF4's `RUNTIME_LATEST` is
  **1.11.221** — the game's runtime. `Depends/spdlog` confirmed redundant
  and unused by the build.
- **Target:** next-gen runtime 1.11.221, Address Library required in-game.
- **CRT:** static (`/MT(d)`) everywhere, matching the core.
- **Logging:** through `LCE::Logging` (the contract) + `REX::LOG` (the
  visible file, `My Games/Fallout4/F4SE/TheLivingCommonwealth.log`).
- **Versioning:** adapter versions independently from 0.1.0.
- Every decision is recorded as an ADR in `Docs/DecisionLog.md`.

- **First heartbeat verified in-game (2026-08-09).** F4SE 0.7.8 loads the
  plugin (handle 1) on runtime 1.11.221; the `GameLoaded` event fires and
  the heartbeat lands in
  `My Games/Fallout4/F4SE/TheLivingCommonwealth.log`.
- **Translation stone implemented and verified in-game (2026-08-09).** On
  GameLoaded the adapter translates every loaded settler into an entity
  (FormRef + seeded Needs + empty Memory/Relationships) and registers the
  serializers for the 0.4.0 snapshot. Adapter tests 3/3 green; in-game
  log confirms: `The Commonwealth wakes up: 10 settlers became minds.`
- **Intent executor implemented, tick verified in-game (2026-08-10).**
  The simulation ticks inside the game: a per-frame hook on
  `ProcessVMTick` (address library ID 2251368 — the budget-ticked
  Papyrus VM queue F4SE itself hooks for its delay functors), two of
  whose four call sites (`0x010E9F7E`, `0x010EA08E`) fire once per
  frame, `Update(registry, delta)` on the game thread, then the pure
  plan builder executes intents through the `Movement::WalkTo` seam —
  refusing rather than teleporting. Verified in-game by per-hook fire
  counters: the two sites fired once per frame for 12,600+ frames, and
  9 settlers at Sanctuary logged clean `decides Explore (0.5x)` lines
  within 88ms of the world waking. The route taught a lesson: the
  first in-game test was misread as a dead tick (its log was
  truncated), sending a driver-hook detour that per-hook fire counters
  ended in one session — attribution counters are the tool. Also
  fixed from the first run: targetless intents (Explore) were wrongly
  refused "target not loaded" — the plan builder now treats a
  targetless intent's target as loaded by definition (tested).
  Adapter tests 4/4 green.
- **Walking stone implemented, pending in-game verification
  (2026-08-10).** The walking call is pinned against Fallout4.exe
  1.11.221 — and the earlier assumption of a function named
  `AIProcess::CreateMovementPlanner` was wrong: FO4's walk-to-point is
  the movement controller's `DoSetPlannerDirectControl` (the NPC
  subobject vtable 0x2567B68, slot [2] = 0xdc92f0, `this` =
  controller+0x138 — RTTI chain: name → type descriptor → COL →
  vtable → slot). `Movement::WalkTo` calls it with a runtime vtable
  guard that refuses (never teleports) and prints the real vtable on a
  mismatch. The market half: every mind is seeded with a Trade memory
  pointing at the Sanctuary workshop (REFR 000250FE, verified from
  Fallout4.esm — the record's EDID is SanctuaryWorkshopREF and its
  position cross-checks the canonical settlement table; base 000C1AEB),
  so hungry settlers decide `MoveTo -> 000250FE`; a per-entity walk
  session issues each walk once while the memory lasts. MarketTest proves the decision half
  (seeded mind → MoveTo; bare mind → Explore). Adapter tests 5/5
  green. A distance probe (per-entity, every 5s) was added so arrival
  is proven in the log, not by eye — and its first run found the walk
  orders were going to settler-faction actors at settlements
  kilometers away (Abernathy, Warwick — each matched to its own
  workbench within meters), so the market seed is now radius-scoped
  (~10,000 units: only minds whose settler is within walking distance
  remember the market). Open: the walk itself in-game — see
  `Docs/Design/Walking.md`.

Open items for the author: the plugin author handle (TODO in `xmake.lua`),
the banner quote slots, and the Nexus name check.
