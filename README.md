# The Living Commonwealth — Fallout 4 Adapter

**Fallout 4 adapter for the [Living Commonwealth Engine (LCE)](https://github.com/lelsliem/Living-Commonwealth-Engine-LCE-) — an F4SE plugin that makes the Commonwealth *live*.**

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0--or--later-emerald.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-emerald.svg)](https://en.cppreference.com/w/cpp/23)
[![Version](https://img.shields.io/badge/Version-0.7.8-emerald.svg)](Docs/Roadmap.md)

The settlers aren't on quest scripts — they're **hungry**, they **remember** where to trade, they walk to **their own settlement's market** when they're hungry, and the exchange is physical: caps change hands, the stall-keeper's purse grows, trust is earned. The market has **hours** — it closes at night and nobody walks to a closed bench — and the day's **weather** is remembered. They have **names**, they **argue**, they **fight**, they **bond**, and **children are born**. The game does nothing but show the result.

> A settler goes to market because they are hungry — no script.

That sentence is the test plan. In-game verified end to end: hungry settlers decide `MoveTo`, walk to the bench, arrive, trade, and the world survives save/load.

## Roadmap

Where this project is and where it's going: `Docs/Roadmap.md`. Every stone through **0.7.8** is in and verified in-game (2026-08-13). The staged run to **1.0.0** is planned (`Docs/Design/ReleasePlan.md`): 0.7.9 bugs & polish → 0.8.0 Illness & Medicine → 0.9.0 the release gate → 1.0.0 freeze and ship. The milestone stones so far:

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
- **0.7** the player listens — settlers have **names** (the game's own
  names win; the author curates the rest in the INI — gender-split
  pools plus a separate animal pool; owned animals are named, strays
  stay nameless), relationships can go **bad** (a hungry arrival at a
  closed market blames the keeper — `ReportOutcome({keeper, Social,
  Failure})` — the settlement's echo agrees, and rival/enemy bonds
  form so **feuds begin on their own**), and the player **hears** the
  world: events become one-line news (throttled HUD notifications) and
  a settlement radio speaks them as captions. The engine's Legacy
  stones ride the death and birth paths (a death bequeaths its
  memories and leaves its name as a legacy; a child inherits the
  parents' memories of the people). Record **v6**.
- **0.7.1–0.7.5** the world talks, trades with anyone, and fights —
  conversation pools (the good greet/gossip/family, the bad
  trade/row, the ugly grief/fight/feud — a seeded picker, one line
  per mind per day), **names for everyone** (role titles gain the
  person: "Provisioner Daisy"; game names win), **trade with anyone
  who sells** (the vendor census — a hungry walk resolves to a
  person, not only the bench), and the **physical feud**: temper +
  chance book a fight (once per day, co-saved), the victim takes the
  game's own paired-push kick, the exchange runs on beats (kick →
  fall → get-up → retaliation → slink-off), and the threats ride the
  game's own subtitle queue as bottom-of-screen subtitles only when
  the player is close enough to hear (`sim.subtitle.radius`). Record
  **v7**.
- **0.7.6** the kick is real — an unconditional IDLE clone
  (`TheLivingCommonwealthAnims.esp`) delivers paired-push animation on
  both beats; the fall tips instead of sliding; the IsDown guard waits
  for actors to be on their feet before the retaliation.
- **0.7.7** babies — the full birth lifecycle: conception → pregnancy
  window → birth event → named child → fed by household → growth to
  Human after `sim.birth.childhood` days. Only Human×Human pairs
  conceive; the species gate enforces it. Co-save serialized.
- **0.7.8** visible children — runtime pairing scans for child actors
  from the external Baby Sim mod and connects them to sim-only children
  after they grow. Graceful degradation when the mod is absent
  (`sim.birth.visible`). The adapter owns the pairing; no patch ESP
  needed.

**Live on GitHub:** [lelsliem/The-Commonwealth-Lives](https://github.com/lelsliem/The-Commonwealth-Lives) —
releases published: `0.5.0-beta`, **`0.6.0`**, and **`0.7.0`**
(2026-08-11, notes in [RELEASE_NOTES.md](RELEASE_NOTES.md)). The
0.7.x run ships **local only** — no release package until **0.8.0**.
Nexus comes later.

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
                   (weather + market hours), Components, BlobCodec,
                   Birth (pregnancy/growth/pairing), Names (pools + role
                   names), Dialogue (talk pools), Rows (verbal altercations),
                   Gossip, Arcs (mediation + grief), Fights (physical
                   escalation), Bonds (relationship states),
                   ConflictGates (once-per-day fight/row gate),
                   Households (shared wallet), Kin (family gate),
                   Subtitles (on-screen fight lines)
tests/             the adapter's test harness (links LCE.Core only, no game)
Docs/              handoff doc, decisions, design, roadmap
Depends/           local third-party clones — study/build inputs, not committed
Build/             build output (gitignored)
```

## Building

```bash
# one command — builds the DLL + the test harness
xmake -y

# run the tests (no game required)
xmake run TheLivingCommonwealth.Tests
```

## Installing

1. Build the DLL (or grab it from a release)
2. Copy `TheLivingCommonwealth.dll` to your F4SE plugins folder
   (`Data/F4SE/Plugins/`)
3. Copy `config/TheLivingCommonwealth.ini` to the same folder
4. For fights: enable `TheLivingCommonwealthAnims.esp` in your load order
5. For visible children: install Baby Sim - Babies That Grow Up and set
   `sim.birth.visible = 1` in the INI

## INI tuning

The `TheLivingCommonwealth.ini` file controls the simulation:

- **Need decay rates**: `sim.hunger.decay`, `sim.fatigue.decay`, etc.
- **Market hours**: `market.open.hour`, `market.close.hour`
- **Fight tuning**: `sim.fight.chance`, `sim.fight.push`, `sim.fight.stagger`
- **Birth lifecycle**: `sim.birth.enabled`, `sim.birth.chance`,
  `sim.birth.gestation`, `sim.birth.childhood`, `sim.birth.visible`
- **Name pools**: `names.first.male`, `names.first.female`,
  `names.first.animal`, `names.last`
- **Dialogue pools**: `dialogue.greet`, `dialogue.trade`, etc.

Missing or broken lines keep defaults — a broken line never breaks the world.

## License

GPL-3.0-or-later (it links CommonLibF4). The core it depends on is MIT.
