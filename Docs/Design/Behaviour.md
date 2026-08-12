# Behaviour — "Different Minds, Different Manners"

**Stone:** adapter 0.5.0 groundwork (species/behaviour split)
**Status:** ✅ **VERIFIED in-game 2026-08-10/11** — species tag, profile
table, and per-species seeding live; 7/7 adapter suites green
(BehaviourTest among them); the split proven in-game across 0.5.0–0.7.0
(humans trade, animals are fed by owner or settlement, children are
sim-only minds).
**Related:** core stays species-agnostic by design (`Behaviour.h`'s
`Decide` reasons over needs and memory, never game facts); ADR-0024
(game knowledge at the edge).

**0.7.5 field find (ADR-0032):** the split is now *enforced*, not
hoped. The fight test showed behemoths brawling at the market —
SupermutantBehemothRace was missing from the animal table, so every
behemoth defaulted to Human: a pouch, a name, enemy bonds, a feud.
The table grew, and a restored mind's stored species is no longer
trusted — it is re-derived from the actor's race the moment the actor
loads (ReclassifyLoadedMinds), pouch dropped, bonds and stall rows
pruned. The bond book now refuses any pair with an animal on either
side (no friend, rival, enemy, or spouse — a dog has no feud to row),
the bench crossing skips animals outright, and restore heals a
pre-fix save. Owned animals keep their names; unowned strays stay
nameless. People (Human and Child) own the feud: rows, fights, trades,
friends, and one spouse each (the monogamy cap — a would-be second
spouse bond caps at sweetheart, and an existing marriage is never
broken).

The problem this stone solves, in the user's words: the junkyard dog
and the brahmin walked to the market — and they should, they're minds —
but a dog must not *trade, buy, or talk*. The core cannot know that; the
adapter must define it.

---

## The split

One new component, one profile table, one classification.

**`SpeciesTag`** (`Components.h`) — the component that says what kind of
mind an entity is. `Human` or `Animal`, set once at translation from the
actor's race, persisted in the co-save (`species` in the record's type
table) so a restored world keeps its dogs dogs. A missing tag reads as
Human — the safe default for a workshop population.

**`BehaviourProfile`** (`Behaviour.h`) — the table the adapter owns:

| | Human | Child | Animal |
|---|---|---|---|
| Market interaction (`MarketKind`) | `Trade` (with a trader) | `Aid` (fed by the settlement) | `Aid` (fed by the settlement) |
| `CanTrade` | ✅ | ❌ — no stall, no barter | ❌ — never buys or sells |
| `CanTalk` | ✅ | ✅ — plays, chats | ❌ — no conversation |
| `NeedsSocial` | ✅ | ✅ | ❌ — no Socialize intents |
| `NeedsComfort` | ✅ | ✅ | ❌ — no Work intents |

`BehaviourFor(Species)` returns the profile (unknown species fall back
to Human). `SeededNeeds(Species)` seeds a fresh mind: Hunger, Fatigue,
and Safety are universal; Social and Comfort belong only to species that
can use them. No social drive → the core never produces a Socialize
intent → an animal never wanders off to "talk." A child keeps the full
need set — it plays, it gets tired, it wants comfort — but its market
interaction is Aid, never Trade.

## The one deliberate lie

The animal's market **memory keeps `InteractionKind::Trade`** — exactly
like a human's — even though the profile says animals don't trade. This
is not a contradiction; it is the engine's plumbing. The core's hunger
branch in `Decide` finds a food source only through Trade-kind memories
(`ChooseTarget(..., Trade)`). If the seed changed to `Aid`, the hungry
dog would stop walking to the market entirely — it would Explore instead,
because the core has no other notion of "where food is." So the *memory*
says "food is over there" (Trade), and the *profile* decides what
arriving means: a human reports a Trade outcome with a trader, an animal
reports an Aid outcome with the settlement — fed, never bartering. The
0.5.0 arrival leg consults `MarketKind`/`CanTrade` when it reports the
outcome; the lie stays inside the sim and is invisible to the player.

## Classification — verified in xEdit (2026-08-10)

`ClassifySpecies(const RE::TESRace*)` in `Adapter.cpp` reads the
actor's race (`Actor::race`, offset 0x418) and switches on FormID. The
FormIDs below were read off Fallout4.esm in xEdit — the verify ritual
caught two wrong guesses (HumanRace is `00013746`, not `13A47`):

```cpp
// Children
case 0x0011D83F:   // HumanChildRace
case 0x0011EB96:   // GhoulChildRace
    return Species::Child;

// Animals (verified)
case 0x0001D698:   // DogmeatRace (junkyard dog)
case 0x0002047E:   // BrahminRace (pack brahmin)
case 0x000C9ACF:   // CatRace
case 0x000D9804:   // GorillaRace
... // 24 animal races total, incl. the wild ones
    return Species::Animal;
```

Everything else defaults to Human. The lists grow as settlement content
grows; a misclassified mind is harmless (it only behaves human until
the table grows) and a re-translate re-tags it.

**Enemies never reach this table**: sim-relevance is
`WorkshopNPCFaction` membership, and hostiles (feral ghouls, super
mutants, deathclaws as enemies) do not hold it — they never become
minds, so they never get market intents. The wild-animal entries are
future-proofing: if a mod ever makes one a settler, it is fed, not
trading.

**Robots and synths are deliberately absent.** A synth settler is a
person — Human is right. A robot (Mr. Handy, Protectron, Assaultron,
SentryBot, turrets) is its own species for a later stone: no biological
needs to seed, its own market rule. Noted here so the table grows
intentionally, not by accident.

## What changed

- `src/Behaviour.h/.cpp` — Species, BehaviourProfile, BehaviourFor,
  SeededNeeds(Species), ArrivalOutcome (moved from Components.h, which
  keeps FormRef and gains SpeciesTag).
- `src/Serialization.cpp` — SpeciesTag serializer (registered at init).
- `src/CoSave.cpp` — `species` in the stable type-name table.
- `src/Market.h` — SeedMarketMemory takes a per-mind food-source
  resolver (pure, idempotent).
- `src/Adapter.cpp` — ClassifySpecies + per-species tag/seed at
  translation; OwnerEntityFor (GetOwner → sim entity); the feeder
  resolver in SeedMarket; ReportArrival on walk arrival.
- `tests/main.cpp` — BehaviourTest: profiles, seeding, the SpeciesTag
  round-trip, and ArrivalOutcome per species; MarketTest: resolver
  targeting, idempotence, and the grazer (no valid source → not
  seeded).

## Food sources and arrival outcomes (0.5.0, built 2026-08-10)

The market is no longer everyone's food source. The seed resolves **who
feeds this mind** per species (`SeedMarketMemory`'s per-mind resolver):

| Species | Food source (the Trade-kind memory's target) | Arrival outcome |
|---|---|---|
| Human | The market (trader at the workshop) | `{market, Trade, Partial}` — arrived, no trade yet |
| Child | The settlement (or its owner if the game assigns a settler one) | `{feeder, Aid, Success}` — fed, nothing in return |
| Animal | **Its owner** (`actor->GetOwner()` → if the owner is a translated settler entity, the dog walks to its human) else the settlement — home | `{feeder, Aid, Success}` — fed, gives nothing in return |

The memory kind stays Trade for every species (the engine's hunger
branch only finds food through Trade memories — the lie, again); the
*target* is now species-correct, and `ArrivalOutcome(Species, feeder)`
decides what getting there means: a human reports `Trade`/`Partial`
(got there, didn't trade — the honest result per the outcome contract),
a child or animal reports `Aid`/`Success` — disposition toward the
feeder warms, no trust ledger, no barter UI. The dog trusts no one's
books; it just likes whoever feeds it.

The game-side owner readout is logged once per animal per world —
`animal 0001CA7D is fed by its owner ... (a settler)` or `has no sim
owner ... — fed by the settlement` — the in-game proof of who feeds
whom (the junkyard dog's owner is likely the player, which is no
entity, so the dog comes home to be fed).

## The goals wrinkle (for the real-test stone)

Nothing restores needs yet, and the core's goal map feeds `AcquireFood`
from Trade, not Aid. So today an arrival outcome is memory +
relationship (no goals exist to serve) — the feed is real in the
relationship, not yet in the hunger gauge. When the real test seeds
goals, a dog's hunger loop needs either a Feed kind or a core mapping
change — an engine ask, noted in the roadmap.

## Desync the herd (0.5.x, built 2026-08-10)

Every mind's needs are born slightly different: `VaryNeeds` applies a
deterministic per-entity jitter (FNV-1a of the entity id) to each need's
initial value (±0.10) and decay rate (×[0.6, 1.4]). The core decays
`need.Value -= need.DecayRate * dt`, and the co-save serializes the
rate — so the metabolism survives restore with no engine change and no
new pin.

The point is rhythm: all minds used to share one hunger clock, get
hungry on the same tick, and march to the market in a wave. Now hunger
arrives at different times for different minds, the wave becomes a
trickle, and because the decay-rate jitter is a per-mind metabolism the
stagger persists after every feed instead of re-synchronizing at
`fed: Hunger 0.00 -> 1.00`. The jitter is shared across a mind's needs
(one id, one temperament), so each mind's internal urgency ordering
stays sensible — a mind never becomes "fast-hunger, slow-fatigue."

**The engine's complement (core 0.5.0 stone 07, `90a9d33`, wired into
the adapter 2026-08-10):** on top of the adapter's seeded metabolism,
the engine jitters *each tick's* decay per entity — `DecayRate *
Derive(id).NextFloat(1 ± sim.jitter)` (default 0.15) — the herd broken
at the source. Two layers, two purposes: `VaryNeeds` sets a mind's
*base* rate (persistent, serialized, works with no engine support); the
engine's spread re-rolls a small per-tick personality on top. The
adapter owns a seeded `Rng` and passes it to `Update`; the co-save
persists `rng.State()` (record v2) so a restored world resumes the same
stream, and a v1 record reseeds fresh.
