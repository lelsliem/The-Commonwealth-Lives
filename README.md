# The Living Commonwealth — Fallout 4 Adapter

**Fallout 4 adapter for the [Living Commonwealth Engine (LCE)](https://github.com/) — an F4SE plugin that makes the Commonwealth *live*.**

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0--or--later-emerald.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-emerald.svg)](https://en.cppreference.com/w/cpp/23)
[![Version](https://img.shields.io/badge/Version-0.2.0--alpha-emerald.svg)](Docs/Roadmap.md)

The setters aren't on quest scripts — they're **hungry**, they **remember**
who cheated them, they **flee** when raiders come, and the market closes on
rainy days. The game does nothing but show the result.

> A settler goes to market because they are hungry — no script.

That sentence is the test plan.

## Roadmap

Where this project is and where it's going: `Docs/Roadmap.md`. Four
stones are in: the heartbeat (verified in-game), the translation (settlers
become minds — verified at Sanctuary), the snapshot substrate, and the
intent executor — built and tested; the sim ticked in-game and all 11
settlers at Sanctuary logged intents, but the per-frame hook is still
being attributed (the proof logging couldn't name the path, so the
candidates carry fire counters). Open items: the frame hook and the
game's walking call (the movement planner). Next: the co-save, where the
world survives a save and a load.

## What this is

The adapter is a **client** of `LCE.Core` — exactly like the engine's test
harness, but living in its own repository and talking to a game. It does
**not** reimplement simulation:

- The adapter **calls**: `CreateEntity`, `DestroyEntity`, `Remember`
  (experiences and world facts), `Update` — and **reads** intents via
  `GetComponent<Intent>`.
- The adapter **guarantees**: an intent is a *hint*, not a command.
- The core **promises**: no game knowledge, no queries of the world, a
  stateless tick.

The contract lives in the core repo:
`Docs/Architecture/PlatformIntegration.md`. Start there.

## Repository map

```
xmake.lua          build (xmake; drives the core's CMake via the lce.core rule)
src/               the plugin: main (lifecycle), Adapter (the world object),
                   Translator (form ↔ entity), Serialization, SimRelevant
                   (the settler predicate), Components, BlobCodec
tests/             the adapter's test harness (links LCE.Core only, no game)
Docs/              handoff doc, decisions, design
Depends/           local third-party clones — study/build inputs, not committed
Build/             build output (gitignored)
```

## Build

Requirements: xmake 3.0+, CMake 3.28+, Visual Studio 2022 (MSVC v143),
and a checkout of the core at **0.4.0+** at `C:\LivingCommonwealthEngine`
(override with the `LCE_CORE_PATH` environment variable; the build refuses
a stale core with a clear message).

```bat
xmake f -m debug
xmake
```

The build does three things:

1. Builds **CommonLibF4** from the local clone in `Depends/` (xmake fetches
   its own spdlog via xrepo — first build needs network).
2. Builds **LCE.Core** with its own CMake into `Build/core` (the core repo's
   own `Build/` is never touched).
3. Links the plugin DLL.

Output: `build/windows/x64/debug/TheLivingCommonwealth.dll`.

## Run (in-game)

1. Install **F4SE** and the **Address Library for F4SE** (the plugin uses
   the address library; `F4SEPlugin_Version` declares runtime 1.11.221).
2. Put `TheLivingCommonwealth.dll` in `Data/F4SE/Plugins/` (or install via
   your mod manager — `xmake install` with `XSE_FO4_GAME_PATH` /
   `XSE_FO4_MODS_PATH` does this directly).
3. Launch through F4SE. The heartbeat:
   `My Games/Fallout4/F4SE/TheLivingCommonwealth.log`
   → `The Living Commonwealth heartbeat: the world is awake.`
   ✅ Verified in-game 2026-08-09 (F4SE 0.7.8, runtime 1.11.221).
   On GameLoaded the translation stone logs
   `The Commonwealth wakes up: N settlers became minds.`
   ✅ Verified in-game 2026-08-09: 10 settlers at Sanctuary.
   Every frame after that, the executor ticks the simulation and logs
   intents as they change, e.g.
   `settler 0008F3A1 decides MoveTo -> 0008F3B2 (0.82)`.
   (In verification: 11 settlers logged `decides Explore` in-game, but
   the per-frame hook is still being attributed via fire counters —
   see `Docs/Design/Executor.md`.)

## Test

```bat
xmake run TheLivingCommonwealth.Tests
```

Runs the adapter's harness (translator tables, seeding, snapshot
round-trip, plan builder) — links LCE.Core only, no game required.
4/4 suites green.

## License

GPL-3.0-or-later — the adapter links CommonLibF4 (GPL-3.0 with modding and
linking exceptions). The core stays MIT. This split is deliberate and
physical: the adapter lives in its own repo and never shares a tree with the
core.
