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

**Design docs in this repo** (`Docs/Design/`): `Translation.md` (form ↔
entity), `Executor.md` (the tick + intent executor), `Walking.md` (the
walk), `CoSave.md` (the co-save record — v4; v3's stall section
still rides), `Behaviour.md` (the
species split), `Market.h`'s sibling `SettlementMarkets.md` (the census
+ per-settlement markets), `Trade.md` (the stall-keeper trade),
`Economy.md` (cap pouches), `Tuning.md` (the INI), `WorldFacts.md`
(the gates: market hours + weather), `WeatherFacts.md` (weather memory
events), `RealTest.md` (the live log evidence). Each is flipped to
**verified in-game** as its stone lands; the ones that aren't verified
yet say so.

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

## Status

| Version | Stone | Status |
|---------|-------|--------|
| 0.1.0 | Scaffold + heartbeat | ✅ verified in-game |
| 0.2.0 | Translation | ✅ verified in-game — settler-faction actors become minds |
| 0.3.0 | Intent executor | ✅ verified in-game — tick + settlers walk to market |
| 0.4.0 | Co-save | ✅ verified in-game — 637 entities saved and restored |
| 0.5.0 | Living world ("The Settler Goes to Market") | ✅ complete (2026-08-10) — **everything implemented and verified in-game: species split, arrival outcomes, real test, world facts (settlers stop at 22:00), tuning, need-decay tuning, weather memory events, per-settlement markets (persistent-cell census), desync, the trade stone, the economy stone, the engine's per-tick decay jitter wired (`90a9d33` + Rng wiring, co-save v2), stall-keepers persisted (co-save v3), memory world-days persisted (co-save v4), lifecycle + walk-probe hardenings, the tuning INI template (`config/TheLivingCommonwealth.ini`), and the GitHub publish (repo live, tag `0.5.0`, 2026-08-10)** |
| 0.6.0 | Life & Emergent Quests ("The Commonwealth Remembers") | ⬜ in progress — **Stone 1 verified in-game (2026-08-10): the world keeps its books — a pure `Lifecycle::Diff` (arrivals / deaths / departures) on the tick's one-second census; one `SeedMind` path for the wake seed and mid-session arrivals; deaths clean the mind's book entries and write the death fact to every survivor (the core gained `InteractionKind::Death`); faction-left departures get a goodbye. Verified: a real kill books confirming → died one second apart, the dead never restore and never ghost-walk, a fresh new game books zero false deaths, already-dead settlers from old saves are parked and never booked (three hardenings — the 3D gate, two-pass confirmation, the alive-first rule; ADR-0011). Stone 2 VERIFIED in-game (2026-08-11): bonds — the adapter owns an EventBus (Request A, stone 08) subscribed to `RelationshipChangedEvent`; `sim.bond.threshold.*` from the INI (adapter defaults when the file names none); a mutual + sticky derivation in `Bonds.h` (formation at the line, dissolution halfway back — drift is quiet, so the 1-second `ReconcileBonds` pass is the dissolve net and the event channel is the instant formation); co-save v5 (form pair, kind, since-day) so a spouse survives reload; and the two in-game discoveries that made it real — the buyer's half of a trade is `Remember({keeper, Social})` (Trade builds trust, never disposition) and the living drift clock (`sim.drift.rate = 0.0002`, ~1 h half-life, injected as the adapter default). ADR-0012. Remaining stones planned in `Docs/Design/Life.md` (households, gossip, arcs, births)** |

**Build:** `xmake` (one command). Two targets:
`TheLivingCommonwealth` (the DLL) and `TheLivingCommonwealth.Tests`
(the harness — links LCE.Core only, no game; run as
`xmake run TheLivingCommonwealth.Tests`). **9/9 suites green.**

---

## Architecture — how the adapter is wired

```
src/main.cpp        F4SE entry: hooks (Tick), messaging (lifecycle),
                    serialization callbacks (co-save glue)
src/Adapter.h/.cpp  the one world object: registry + translator, the
                    lifecycle, the tick loop (Update → plan → execute → probe),
                    species classification (race → Human/Child/Animal)
src/Translator.h/.cpp  formId ↔ EntityId tables (adapter state, game-free)
src/SimRelevant.h/.cpp the settler predicate (WorkshopNPCFaction 0x000337F3)
src/Behaviour.h/.cpp   Species + BehaviourProfile — who trades, who is fed
                       (Human/Child/Animal); SeededNeeds(Species)
src/Executor.h/.cpp    pure plan builder — refusals are the contract
src/Movement.h/.cpp    WalkTo: the pinned Actor::InitiateCommandModeTravelPackage
src/Market.h           the census (persistent-cell scan) + per-settlement
                       market seeding (nearest workshop within ~140 m)
src/Serialization.h/.cpp  per-type serializers (Needs, Memory, …, SpeciesTag,
                       CapPouch)
src/CoSave.h/.cpp     the durable co-save record (stable names, versioning;
                       v4 — v3's stall section still rides, and the memory
                       payload now carries each event's world day)
src/BlobCodec.h       little-endian byte codec
src/Components.h      FormRef + SpeciesTag + CapPouch (adapter components)
src/Tuning.h          the INI — sim.* decay rates, meal price, market hours
src/WorldFacts.h      the gates: market hours (closed at night) + weather
src/Tick.h/.cpp       the per-frame VM-tick hooks
```

**Lifecycle mapping (current, verified):**

| Game event | The adapter does |
|------------|------------------|
| `kGameLoaded` (startup) | `StartWorld`: translate loaded sim-relevant actors into entities (tagged by species, seeded per species); seed the market memory |
| `kPostLoadGame` (a save finished loading) | Apply the co-save restore (or start fresh) — **the load's completion event; `kGameLoaded` alone does not fire for real loads** |
| `kPreLoadGame` | `EndWorld` (Clear; serializers survive); arm abort recovery (60s) |
| `kDeleteGame` (a save FILE was deleted) | nothing — a running world survives (an autosave rotation once killed it mid-world; `EndWorld` here is a no-op now) |
| serialization save callback | `CaptureWorld` → `CoSave::Encode` → `WriteRecord` |
| serialization load callback | read record → `CoSave::Decode` → `QueueRestore` (applied on `kPostLoadGame`) |
| new game (revert callback) | `EndWorld` |

**The sim loop** (per frame, game thread): `Update(registry, delta)` →
`BuildPlan` (pure; loaded/available refusals) → `ExecutePlan` (the walk
issue; sessions capped at 16) → `ProbeWalks` (live distance probes).

**The co-save record:** the core's snapshot is process-local (component
keys are `std::type_index`) — the adapter's record translates it to stable
names (`needs`, `memory`, `relationships`, `goals`, `intent`, `formref`,
`species`) with its own version (`kRecordVersion = 1`). Refusals are the
contract: truncated records, unsupported versions, and unknown component
names never half-apply. Save-compatibility is the adapter's job.

**Two environment lessons learned (documented in Walking.md / CoSave.md):**
- The crash saga: a corrupt save, not the plugin — the identical
  fail-fast signature hit no-DLL runs. DisableExitSave prevents the
  exit-save cycle.
- Every "load abort" was our own recovery timer: real loads are slow
  (a 600-actor co-save world takes >12s), and the completion event is
  `kPostLoadGame`.

---

## The species/behaviour split (0.5.0 groundwork — built)

The junkyard dog and brahmin are minds like settlers (they carry
WorkshopNPCFaction too, so they pass the same sim-relevance gate) — they
walk to the market when hungry, and they should. But a dog must not
*trade, buy, or talk*. The core is species-agnostic by design, so the
split lives in the adapter (see `Docs/Design/Behaviour.md`):

- **`SpeciesTag` component** — set at translation from the actor's race,
  persisted in the co-save (`species`), so a restored world keeps its dogs
  dogs. Missing tag → Human.
- **`BehaviourProfile` table** (`Behaviour.h`) — the single source of
  truth:

  | | Human | Child | Animal |
  |---|---|---|---|
  | Market interaction | `Trade` (with a trader) | `Aid` (fed) | `Aid` (fed) |
  | CanTrade / CanTalk | ✅ / ✅ | ❌ / ✅ | ❌ / ❌ |
  | Needs | all 5 | all 5 | Hunger/Fatigue/Safety |

  No Social need → the core never produces a Socialize intent → an animal
  never wanders off to "talk". No Comfort need → no Work intents.
- **Classification** (`ClassifySpecies` in Adapter.cpp) — race FormIDs
  **verified in xEdit 2026-08-10**: children = HumanChildRace
  `0011D83F`, GhoulChildRace `0011EB96`; animals = DogmeatRace
  `0001D698`, BrahminRace `0002047E`, CatRace `000C9ACF`, GorillaRace
  `000D9804`, plus the wild set (24 races total, future-proofing).
  Everything else defaults to Human.
- **The one deliberate lie:** the market *memory* keeps `Trade`-kind for
  every species. The core's hunger branch finds food only through
  Trade-kind memories (`ChooseTarget(..., Trade)`) — change the seed to
  `Aid` and the dog stops walking to the market. The memory says "food is
  over there"; the profile decides what arrival means.
- **Enemies never reach the table:** sim-relevance is WorkshopNPCFaction
  membership, which hostiles don't hold. **Robots/synths:** a synth
  settler is a person (Human is right); a robot (Mr. Handy, Protectron,
  turrets) is a deliberate future species — no biological needs to seed.

---

## The 0.5.0 roadmap — "The Settler Goes to Market"

The core has shipped everything the adapter needs (see the canonical
handoff — tuning stone 01, outcome channel stone 02, seeded RNG stone 05
all live). Adapter progress:

1. ✅ **World facts** (verified in-game 2026-08-10 — the settlers stopped
   their market walks around 22:00 and only the dog and a couple of
   stragglers stayed visible) — the market's trading hours gate the
   hungry walk (a remembered `{ invalid, Trade }` fact makes Decide
   explore; the refresh pattern keeps the door shut at night with no
   flicker) and a radstorm shuts the gatherings (`{ invalid, Social }`;
   weather forms pinned from the xEdit dump — only CommonwealthGSRadstorm
   `001C3D5E` matches, see Docs/WeatherForms.md). See
   Docs/Design/WorldFacts.md.
2. ✅ **Tuning from Configuration** (implemented 2026-08-10, in-game
   verification pending) — one text file next to the DLL
   (`Data\F4SE\Plugins\TheLivingCommonwealth.ini`): the `sim.*` keys
   feed `SimulationTuning::FromConfiguration`; the adapter's own keys
   (`market.open.hour` / `market.close.hour`) ride in the same file and
   drive the world-facts hours gate. Missing/broken lines keep defaults.
   See Docs/Design/Tuning.md.
3. ✅ **Arrival outcomes + the real test** (verified in-game 2026-08-10)
   — walks report per species: Human → `Trade` (the trade stone: Success
   with the stall-keeper or remembered merchant, Partial only for the
   stall-keeper setting up), Child/Animal → `{ feeder, Aid, Success }`
   (fed, gives nothing in return); the hunger write-through closes the
   loop (fed:
   Hunger X -> 1.00; the fed dog decided Rest, not MoveTo, 6 ms later;
   19 feeds across the session).
4. ✅ **Migration** (implemented 2026-08-10) — old saves load forward:
   `Decode` accepts version ≤ current (a pre-species save restores with
   no tag → Human) and skips unknown component names (a removed type is
   dropped, never fatal); a newer version is refused. Pinned by
   CoSaveTest's crafted fixtures.
5. ✅ **Weather memory events** (implemented 2026-08-10, in-game
   verification pending) — the live sky classifies into six categories
   (verified forms) pushed as day-stamped world facts (`{ invalid,
   WeatherRain, 1.0, day }`); today's categories refresh all day,
   yesterday's fade, the day-turn logs. The engine grew the `Weather*`
   kinds (append-only, save-safe). Re-derived at the edge — never
   co-save state. See Docs/Design/WeatherFacts.md.
6. ✅ **Per-settlement markets** (implemented 2026-08-10, in-game
   verification pending) — a census of the worldspaces' **persistent
   cells** (base `000C1AEB`) finds every settlement market; each mind
   remembers the nearest within ~140 m instead of one global Sanctuary
   bench. FO4 never fills `formArrays[REFR]` — the original flat-array
   census always read 0 in-game (the live log showed `0 workshops
   known` every session, so the stone was silently running its
   fallback), and settlement workbenches are all persistent refs, so
   the persistent-cell scan is the correct FO4-native enumeration. The
   legacy single-bench fallback still covers an empty census, and the
   census RETRIES when it finds nothing — with a one-time base-form
   probe so the next log says what the filter actually saw. No new
   pins. See Docs/Design/SettlementMarkets.md.
7. ✅ **Desync the herd** (implemented 2026-08-10, in-game verification
   pending) — `VaryNeeds` jitters each mind's seeded needs
   (deterministic per entity id): hunger arrives at different times, and
   the decay-rate jitter is a metabolism that persists after every feed.
   The co-save already serializes the rate — no engine change, no new
   pins. See the Behaviour.md desync section.
7b. ✅ **Per-tick decay jitter** (engine core 0.5.0 stone 07 SHIPPED
   `90a9d33`, WIRED 2026-08-10, in-game verification pending) — the
   engine jitters each tick's decay per entity (`DecayRate *
   Derive(id).NextFloat(1 ± sim.jitter)`), and the adapter now owns a
   seeded `Rng`, passes it to every `Update`, and persists `rng.State()`
   in the co-save (record v2 — the Rng header is the record's first real
   format bump; a v1 record reseeds fresh). See Docs/Design/Tuning.md
   (sim.jitter) and Docs/Design/CoSave.md (the v2 header).
8. ✅ **Seeded need decay rates in the INI** (implemented 2026-08-10,
   in-game verification pending) — sim.hunger.decay / .fatigue /.safety
   /.social /.comfort ride in the same file, so the whole rhythm tunes
   to "a few meals a day" without a recompile. Rates thread StartWorld
   → SeededNeeds, still jittered per mind by VaryNeeds, serialized by
   the co-save. Missing/broken values keep the defaults. See
   Docs/Design/Tuning.md.
9. ✅ **The trade stone** (implemented 2026-08-10, in-game verification
   pending) — a human arrival at the market is a real exchange: the
   first human at each market sets up its stall, every later
   bench-arrival trades with them (Trade, Success — the core serves
   AcquireFood and earns trust), and the trader's half lands too
   (RecordSale: the stall-keeper remembers the customer and warms
   toward them; sim.sale.warmth). The buyer's memory of the merchant
   then beats the bench, so the next hungry walk resolves to the
   person. No engine change, no new pins. See Docs/Design/Trade.md.
10. ✅ **The economy stone** (implemented 2026-08-10, in-game
    verification pending) — the exchange is physical: every human mind
    carries a CapPouch (40 ± 20 caps, deterministic per id); a buyer
    pays what they can afford up to sim.meal.price (default 5) and the
    seller's pouch grows; a broke buyer is fed on the settlement's
    credit. The pouch is a co-save component (stable name cappouch) —
    a saved purse restores exactly, pre-economy saves are back-filled
    on restore. No engine change, no new pins. See Docs/Design/Economy.md.
11. ✅ **Stall-keepers survive save/load** (implemented 2026-08-10,
    in-game verification pending) — who runs each market's stall rides
    the co-save record's v3 section as (market FormID, keeper FormID)
    pairs — form ids, stable across sessions, unlike the session-local
    entity ids — and ApplyRestore rebuilds the map from the restored
    FormRefs. A saved market reopens under the same keeper; a pre-v3
    save re-derives each stall on first arrival. Record v3, the second
    real format bump (after v2's Rng header). See Docs/Design/CoSave.md.
11b. ✅ **Memory world-days survive save/load** (implemented 2026-08-10,
    verified by the harness) — engine stone 06 (WorldTime +
    MemoryEvent::Day) shipped in the canonical handoff with the ⚠
    note: the memory serializer must carry the world day, or a saved
    weather fact loses the day it happened. The adapter's serializer
    was dropping `Day` (the weather facts stamp it — WorldFacts.h — so
    save/load really did lose it). Fixed: the serializer writes/reads
    `event.Day`, and the record bumped to **v4** (third real format
    bump). Pre-v4 records migrate at decode: memory events read back
    with `Day = 0` — "time immemorial", honest for facts saved before
    the calendar. Proven by CoSaveTest: a crafted v3 record with an
    old-format memory blob loads forward, and the round-trip keeps
    `Day = 42`.
12. ✅ **GitHub publish** (plan change 2026-08-10 — Nexus comes
    later): repo live at
    `github.com/lelsliem/The-Commonwealth-Lives` — author `lelsliem`
    in xmake.lua, hygiene (`.gitignore` / `README` / `LICENSE`) done,
    pushed, tag `0.5.0`. The release page (DLL + INI from `config/`)
    and the Nexus upload remain; the F4SE serialization UID is only
    needed if the mod reaches the F4SE plugin registry.

---

## Dependency Wiring

- **CommonLibF4** — the game API + plugin contract, from the local clone
  (`Depends/commonlibf4`); its `RUNTIME_LATEST` (1.11.221) matches the game.
- **F4SE** — runtime-only; CommonLibF4 replaces it as the static dependency.
- **LCE.Core** — linked statically, built by its own CMake via the
  `lce.core` rule into `Build/core` (never touching the core repo's
  `Build/`). The rule pins the core version (refuses below 0.5.0).
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
3. **The quote ritual:** every `.h`/`.cpp` gets a banner with a reserved
   line for a joke or quote; the author fills it. Tests get compact
   headers, no banners.
4. **Commit style:** concise messages that say *why*, one coherent change
   per commit, never commit `Build/` artifacts.
5. **No global state.** Time and tuning are inputs.
6. **Test rhythm:** pure pieces are unit-tested without the game
   (translator, serializers, record codec, plan builder, market seed,
   behaviour table); game-facing pieces are verified in-game via log
   lines, then the docs flip to verified.
7. **The verify ritual:** any hardcoded FormID (races, factions, REFRs)
   is marked and confirmed in xEdit before it is trusted — the race
   table caught two wrong guesses this way.

---

## Open Items (author-side)

- The F4SE-assigned serialization UID — placeholder `'LCEW'` in
  `src/CoSave.h` (`kSerializationUid`); only needed if the mod later
  reaches the F4SE plugin registry (the Nexus release).
- A `Robot` species (no biological needs, its own market rule) — noted in
  Behaviour.md as the next category to grow.

---

## Canonical Copy

This file is a snapshot. The Living Commonwealth Engine repo owns the
living document — read it from `C:\LivingCommonwealthEngine\Docs\AdapterProject.md`,
the single source of truth. When the core ships a new stone, that
document grows first; re-sync this copy from it.
