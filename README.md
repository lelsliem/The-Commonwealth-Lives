# The Living Commonwealth

**An F4SE plugin that makes the Commonwealth *live* — settlers get hungry, tired, and sick; they trade, argue, fight, fall in love, feud, and die; children are born and grow up. No scripts — the game just shows the result.**

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0--or--later-emerald.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-0.8.12-emerald.svg)](Docs/Roadmap.md)

> A settler goes to market because they are hungry — no script.

Settlers aren't on quest scripts. They are **hungry**, they **remember** where to trade, and they walk to **their own settlement's market** when they need to. The exchange is physical: caps change hands, the stall-keeper's purse grows, trust is earned. The market has **hours** — it closes at night and nobody walks to a closed bench — and the day's **weather** is remembered.

They have **names** on the people themselves (Sturges stays Sturges; everyone else gets a name from the author's curated pools). They **talk** to each other using the game's own voice lines, **argue**, and **fight** — the feud begins on its own from a slight. They **fall in love**, become **households** (one pouch, one bench, one bed), and **children are born**: a mother visibly carries a swaddled bundle, and after a couple of days a real child of the Commonwealth takes its place — dressed, named, and growing up. The wastes can make them **sick** — they buy medicine when they can afford it, rest when they can't, and a severe untreated illness can end them. The dead are **grieved** and **buried** by the settlement that mourned them. The player **hears** the world through radio captions and, close up, the words themselves.

The whole world rides inside your save file — save, quit, reload, and everyone remembers who fed them, who they traded with, and who they married.

## Requirements

- **Fallout 4 1.11.221 (Next-Gen)** — the current Steam/GOG build
- **F4SE** (Next-Gen)
- **Address Library for F4SE Plugins**

## Install

1. Install the three requirements above.
2. Extract the release archive into your `Fallout 4/Data/` folder (the plugin lands at `Data\F4SE\Plugins\TheLivingCommonwealth.dll`). Using **Mod Organizer 2**, install it as a normal mod and enable it.
3. Copy `TheLivingCommonwealthAnims.esp` into `Data/` and **enable it in your load order** — it delivers the fight kick animation. Without it fights fall back to a stagger (nothing crashes, it just looks tamer).
4. The INI (`Data\F4SE\Plugins\TheLivingCommonwealth.ini`) is created with sane defaults on first run — every number in it is tunable. Your old saves just work: a 0.5/0.6/0.7/0.8-era save migrates forward cleanly.

Everything is vanilla — no external dependency beyond the three requirements. The visible birth journey (the mother's carry, the child that appears after a save/load) uses the game's own bundle and the game's own child pool.

## Optional

- **MCM (Mod Configuration Menu)** — a full settings page (Life, Interactions, Relationships, Illness, Economy, Birth & Fights, About) to tune the world in-game. Changes hot-apply within a second and survive a restart. No MCM? The INI alone rules — MCM is a soft dependency.
- **Realistic Conversations** — its 33 GMST overrides are re-delivered as a compatibility tuning file next to the DLL; install the ESP yourself and the file applies its settings at load.

## Tune it

The MCM page covers the player-facing knobs. Everything else lives in the INI under `Data\F4SE\Plugins\TheLivingCommonwealth.ini`:

- **The rhythm of life** — `sim.hunger.decay`, `sim.fatigue.decay`, `sim.rest.recovery`, market hours (`market.open.hour` / `market.close.hour`)
- **Relationships** — bond thresholds (`sim.bond.threshold.*`), the feud's temper line, the daily fight gate
- **Birth** — `sim.birth.enabled`, `sim.birth.chance`, `sim.birth.gestation` (pregnancy length), `sim.birth.childhood`, `sim.baby.holdDays`, `sim.baby.visualChild`
- **Illness & medicine** — the whole curve (`sim.illness.*`), the medicine price, the cough cadence, per-stall daily stock
- **Economy** — the daily stipend (`sim.economy.stipend`, default off — opt in via the MCM slider), who pays, owned-only
- **Names** — `names.first.male`, `names.first.female`, `names.first.animal`, `names.last` — swap in your own pools freely
- **Dialogue** — `dialogue.greet`, `dialogue.trade`, etc. — the game's own curated lines, editable per pool

A missing or broken line keeps the default — a broken line never breaks the world.

## What it does — the honest list

- **Hunger, fatigue, safety, social, comfort** — real needs that decay and desync per settler, so the crowd doesn't march in lockstep.
- **Per-settlement markets** — every settlement has its own bench and its own stall-keeper; the market closes at night and reopens on time.
- **A real economy** — cap pouches that round-trip save/load; sellers earn, buyers spend, medicine is a stocked shelf that can sell out.
- **Names, bonds, and households** — friends, sweethearts, and spouses emerge from how people treat each other; couples share a pouch, a bench, and a bed.
- **Talk, rows, and fights** — using the game's own voice lines and the game's own kick animation; subtitles when you're close enough to hear.
- **Birth, growth, grief, and burial** — the full life cycle, from a carried bundle to a dressed child to a remembered death.
- **Illness** — radstorms, shared meals, wounds, and contagion; treatable at the market when you can afford it.
- **Weather memory** — settlers remember the day's sky, and a radstorm day is a real event.
- **Road life** — provisioners and caravan guards eat on the road, and the friends they made travel with them.

## Known limits (honest)

- **No audio layer.** Speech is captions, subtitles when you're near, and the game's own voiced dialogue where its voices support it. The settlement radio announces news as captions. There is no custom voice acting — nothing can be done if the game simply doesn't provide a line for a voice.
- **A newborn child is invisible until the next save/load.** The one spawn route that doesn't crash or half-birth the child deliberately leaves it un-initialized; the game's own load routine completes it — head, movement, name, clothes — the next time you load the save.
- **The fight kick needs the Anims ESP.** Without it fights still happen but read as a stagger. The known presentation bugs (both actors collapsing, a push with no visible shove) are documented and deferred to the post-beta animation pass.
- **By design:** animals get fed at the market but never trade or talk; robots talk but never eat, sleep, or fall in love; companions never enter the dating zone (friendship only); only human couples conceive; unowned settlements still have living settlers; provisioner first names never surface in-game.
- **MCM is optional** — without it the INI alone rules.
- The F4SE serialization UID is a placeholder; it only matters if the mod later joins the F4SE plugin registry.

## Compatibility

Tested and working with: **Address Library** (required), **MCM** (optional), **Realistic Conversations** (compat tuning file included), **Sim Settlements** and **Sim Settlements 2** (recommended for players, no interaction), and **Settler and Companion Dialogue Overhaul** (no interaction). The voice-aware rule is the one real constraint: a named voice (Sturges, Marcy, companions) has no recording of the generic settler lines, so those minds stay mute rather than say a line their voice can't speak.

Live on GitHub: [lelsliem/The-Commonwealth-Lives](https://github.com/lelsliem/The-Commonwealth-Lives) — releases and the full changelog live there. **This is a beta — save often, and report what you see.**

---

## For developers

The adapter is a **client** of the [Living Commonwealth Engine (LCE)](https://github.com/lelsliem/Living-Commonwealth-Engine-LCE-), exactly like the engine's test harness, but living in its own repository and talking to a game. It does **not** reimplement simulation:

- The adapter **calls**: `CreateEntity`, `DestroyEntity`, `Remember` (experiences and world facts), `Update` — and **reads** intents via `GetComponent<Intent>`.
- The adapter **guarantees**: an intent is a *hint*, not a command.
- The core **promises**: no game knowledge, no queries of the world, a stateless tick.

The contract lives in the core repo: `Docs/Architecture/PlatformIntegration.md`.

### Repository map

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
                   Birth (pregnancy/growth/visible children), Names (pools
                   + role names), Dialogue (the curated talk pools), Rows,
                   Gossip, Arcs (mediation + grief), Fights, Bonds,
                   ConflictGates, Households (shared wallet), Kin (family
                   gate), Subtitles (on-screen lines)
tests/             the adapter's test harness (links LCE.Core only, no game)
Docs/              handoff doc, decisions (DecisionLog), design, roadmap
Depends/           local third-party clones — study/build inputs, not committed
Build/             build output (gitignored)
```

### Building

```bash
# one command — builds the DLL + the test harness
xmake -y

# run the tests (no game required) — 27 suites, all green before anything ships
xmake run TheLivingCommonwealth.Tests
```

### License

GPL-3.0-or-later (it links CommonLibF4). The core it depends on is MIT.
