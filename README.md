# The Living Commonwealth — Fallout 4 Adapter

**Fallout 4 adapter for the [Living Commonwealth Engine (LCE)](https://github.com/lelsliem/Living-Commonwealth-Engine-LCE-) — an F4SE plugin that makes the Commonwealth *live*.**

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0--or--later-emerald.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-emerald.svg)](https://en.cppreference.com/w/cpp/23)
[![Version](https://img.shields.io/badge/Version-0.6.0-emerald.svg)](Docs/Roadmap.md)

The settlers aren't on quest scripts — they're **hungry**, they **remember**
where to trade, they walk to **their own settlement's market** when they're
hungry, and the exchange is physical: caps change hands, the stall-keeper's
purse grows, trust is earned. The market has **hours** — it closes at night
and nobody walks to a closed bench — and the day's **weather** is
remembered. The game does nothing but show the result.

> A settler goes to market because they are hungry — no script.

That sentence is the test plan. In-game verified end to end: hungry
settlers decide `MoveTo`, walk to the bench, arrive, trade, and the world
survives save/load.

## Roadmap

Where this project is and where it's going: `Docs/Roadmap.md`. Every stone
through **0.6.0** is in and verified in-game; **0.7.0** (identity & the
player window) is next:

- **0.1** the heartbeat — the plugin loads and breathes.
- **0.2** the translation — settler-faction actors become minds.
- **0.3** the intent executor — the sim ticks every frame and settlers walk.
- **0.4** the co-save — the world rides inside the save file (record **v4**:
  entities, the Rng stream, who runs each market's stall, and each
  memory's world day — the timestamp survives save/load).
- **0.5** the living world — species split (children and animals don't
  barter), world facts, market hours, weather memory events,
  per-settlement markets (a persistent-cell census — FO4 never fills the
  REFR form array), the trade stone (a stall-keeper per market, the
  buyer remembers the merchant), the economy stone (cap pouches that
  round-trip), per-mind decay desync (`VaryNeeds`) plus the engine's
  per-tick decay jitter (a seeded `Rng`, persisted in the co-save), and
  hardenings: `DeleteGame` no longer kills a running world, and the walk
  probe reads the actor's data position instead of a lying 3D transform.
- **0.6** the Commonwealth remembers — the world keeps its books
  (arrivals wake mid-session, deaths and departures leave it, every
  survivor remembers who is gone — record **v5**), and settlers **bond**
  from how they treat each other: friends, sweethearts, and spouses
  emerge from shared meals and survive save/load. Couples become
  **households** (one pouch, one bench, one bed) and the **sleep cycle**
  closes the loop — a fed mind rests, recovers its Fatigue
  (`sim.rest.recovery`), and walks again, so the same settlers keep
  meeting and their bonds deepen. Settlers **mill around** between meals
  (the wander stone — Rest/Explore command a bounded walk to a real
  nearby reference, furniture preferred), **gossip** spreads every death
  to the settlement, **grief** is real (a widowed settler drains its
  Social seeking company — `arcs: settler X grieves for Y`), and
  **children** are born: a spouse household's sim-only child, fed and
  bonded, living in the co-save like any mind (`sim.birth.enabled`).

**Live on GitHub:** [lelsliem/The-Commonwealth-Lives](https://github.com/lelsliem/The-Commonwealth-Lives) —
releases published: `0.5.0-beta` and **`0.6.0`** (2026-08-11, notes in
[RELEASE_NOTES.md](RELEASE_NOTES.md)). Nexus comes later.

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
                   — stamps every build with the git short hash (the banner
                   line says "loaded (build <hash>)" so the log always names
                   the DLL that ran)
src/               the plugin: main (lifecycle), Adapter (the world object),
                   Translator (form ↔ entity), Serialization + CoSave (the
                   durable record), Behaviour (species rules), Market
                   (census + seeding), Movement (the walk), SimRelevant
                   (the settler predicate), Tuning (the INI), WorldFacts
                   (weather + market hours), Components, BlobCodec
tests/             the adapter's test harness (links LCE.Core only, no game)
Docs/              handoff doc, decisions, design, roadmap
Depends/           local third-party clones — study/build inputs, not committed
Build/             build output (gitignored)
```

## Build

Requirements: xmake 3.0+, CMake 3.28+, Visual Studio 2022 (MSVC v143),
and a checkout of the core at **0.5.0+** at `C:\LivingCommonwealthEngine`
(override with the `LCE_CORE_PATH` environment variable; the build refuses
a stale core with a clear message).

```bat
xmake f -m debug
xmake
```

The build does three things:

1. Builds **CommonLibF4** from the local clone in `Depends/`.
2. Builds **LCE.Core** with its own CMake into `Build/core` (the core repo's
   own `Build/` is never touched).
3. Links the plugin DLL.

Output: `build/windows/x64/debug/TheLivingCommonwealth.dll`.

## Run (in-game)

1. Install **F4SE** and the **Address Library for F4SE** (the plugin uses
   the address library; `F4SEPlugin_Version` declares runtime 1.11.221).
2. Put `TheLivingCommonwealth.dll` in `Data/F4SE/Plugins/` (or install via
   your mod manager).
3. Launch through F4SE. The log is
   `My Games/Fallout4/F4SE/TheLivingCommonwealth.log`. The first lines
   prove which build ran:

   ```
   The Living Commonwealth v0.5.0.0
   The Living Commonwealth v0.5.0.0 loaded (build 43c4e4f).   ← the git short hash
   ```

   Then the world wakes and the sim lives:

   ```
   tuning: no config file (...) — defaults. Create it to change the sim's feel.
   settlement census: 13 worldspaces, 28 workshops known (base 0xc1aeb) — markets are per settlement.
   The Commonwealth wakes up: 11 settlers became minds.
   settler 0001CA7D decides MoveTo -> 000250FE (0.20)
   LCE: settler 0001CA7D sets up the stall at market 000250FE — trade begins when customers come.
   LCE: settler 0001A4D7 trades with settler 0001CA7D at market 000250FE — fed, 5 caps change hands (38 left, 27 now).
   co-save: writing 665 entities (169877 bytes).
   co-save: read 665 entities, 1 stall-keepers — the world will be restored on load.
   ```

   Distances in walk probes are **game units** (`u`), not meters (200 u ≈ 2.8 m).

## Tuning

One text file next to the DLL, `Data\F4SE\Plugins\TheLivingCommonwealth.ini`.
The template ships in this repo at `config/TheLivingCommonwealth.ini` — copy
it alongside the DLL and edit to taste. Unknown keys are ignored; missing,
empty, or unparsable values keep the default — a broken line never breaks the
world.

```ini
; The Living Commonwealth tuning
sim.memory.fade = 0.2        ; core key: memory fade rate
sim.jitter = 0.15            ; core key: per-tick decay spread (0 = off)
sim.hunger.decay = 0.002     ; a few meals a day instead of a constant stream
sim.fatigue.decay = 0.002
sim.safety.decay = 0.002
sim.social.decay = 0.002
sim.comfort.decay = 0.002
sim.rest.recovery = 0.2      ; rest restores fatigue, safety, comfort (/s)
sim.walk.cap = 16            ; how many settlers may walk at once
sim.sale.warmth = 0.1        ; how much a stall-keeper warms to a customer
sim.meal.price = 5           ; caps per meal (a broke buyer is still fed)
market.open.hour = 8         ; the market's hours — closed at night
market.close.hour = 20
```

The code's own built-in defaults are the *fast demo* (e.g. `sim.hunger.decay =
0.1/s` — a meal every few seconds); the shipped template above is the
living-Commonwealth rhythm — a few meals a day. The log prints every live
value at launch (`tuning: loaded …` / `tuning: needs — hunger …`), so the
banner always says which rhythm actually ran.

## Test

```bat
xmake run TheLivingCommonwealth.Tests
```

Runs the adapter's harness (translator tables, seeding, snapshot and
co-save round-trips — including the v3 stall-keepers section and the
migration paths, plan builder, market decision, species rules, pouch
economy, tuning, lifecycle, bonds, households, sleep cycle) — links
LCE.Core only, no game required. **16/16 suites green.**

## License

GPL-3.0-or-later — the adapter links CommonLibF4 (GPL-3.0 with modding and
linking exceptions). The core stays MIT. This split is deliberate and
physical: the adapter lives in its own repo and never shares a tree with the
core.
