# Behaviour — "Different Minds, Different Manners"

**Stone:** adapter 0.5.0 groundwork (species/behaviour split)
**Status:** ✅ **IMPLEMENTED 2026-08-10** — species tag, profile table,
and per-species seeding live; 7/7 adapter suites green (BehaviourTest
among them). In-game verification pending a full 0.5.0 pass.
**Related:** core stays species-agnostic by design (`Behaviour.h`'s
`Decide` reasons over needs and memory, never game facts); ADR-0024
(game knowledge at the edge).

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

| | Human | Animal |
|---|---|---|
| Market interaction (`MarketKind`) | `Trade` (with a trader) | `Aid` (fed by the settlement) |
| `CanTrade` | ✅ | ❌ — never buys or sells |
| `CanTalk` | ✅ | ❌ — no conversation |
| `NeedsSocial` | ✅ | ❌ — no Socialize intents |
| `NeedsComfort` | ✅ | ❌ — no Work intents |

`BehaviourFor(Species)` returns the profile (unknown species fall back
to Human). `SeededNeeds(Species)` seeds a fresh mind: Hunger, Fatigue,
and Safety are universal; Social and Comfort belong only to species that
can use them. No social drive → the core never produces a Socialize
intent → an animal never wanders off to "talk."

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

## Classification — FORMID TO VERIFY IN FO4EDIT

`ClassifySpecies(const RE::TESRace*)` in `Adapter.cpp` reads the
actor's race (`Actor::race`, offset 0x418) and switches on FormID:

```cpp
case 0x0001D246:   // DogRace     — FORMID TO VERIFY IN FO4EDIT
case 0x0002A6A4:   // BrahminRace — FORMID TO VERIFY IN FO4EDIT
    return Species::Animal;
```

Everything else defaults to Human. The lists grow as settlement content
grows (cats, robots, synths — each decides where it belongs). A
misclassified mind is harmless: it only behaves human until the table
grows, and the classification runs at translation, so a re-translate
re-tags.

## What changed

- `src/Behaviour.h/.cpp` — Species, BehaviourProfile, BehaviourFor,
  SeededNeeds(Species) (moved from Components.h, which keeps FormRef
  and gains SpeciesTag).
- `src/Serialization.cpp` — SpeciesTag serializer (registered at init).
- `src/CoSave.cpp` — `species` in the stable type-name table.
- `src/Adapter.cpp` — ClassifySpecies + per-species tag/seed at
  translation.
- `tests/main.cpp` — BehaviourTest: both profiles, animal seeding
  (exactly three needs, no Social/Comfort), and the SpeciesTag
  round-trip through capture/restore.

## What the 0.5.0 arrival leg does with this

When a walk completes and the adapter reports the outcome
(`ReportOutcome`, core stone 02), it consults the profile: a Human at
the market reports `{ trader, Trade, Result }` — trust with the merchant
scales by result; an Animal reports `{ settlement, Aid, Result }` — fed,
disposition toward the settlement warms, no trust ledger, no barter UI.
That leg is the rest of 0.5.0; the table it reads is this stone.
