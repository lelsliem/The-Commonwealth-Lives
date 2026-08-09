# Entity ↔ Form Translation — "The Commonwealth Wakes Up"

**Stone:** adapter 0.2 (after the proven heartbeat)
**Status:** Design — pending review
**Related:** core ADR-0024 (adapters translate, don't simulate), ADR-0014
(no global state), ADR-0023 (the core never knows the game), Law 001
(simple things; compose the complex). Foundation: core 0.4.0 snapshot
substrate (proven by the Snapshot suite, 14/14 green).

---

## The Spec Is a Settler

> On GameLoaded, the settlers of the Commonwealth wake up. Each becomes an
> entity — an ID with needs, memories, and relationships — inside the
> registry the adapter owns. The game is none the wiser; the core never
> hears the word "settler".

That sentence is the test plan. The heartbeat proved the hooks fire. This
stone proves the registry lives — the first real translation across the
seam (ADR-0024).

---

## The Shape: one world object, one edge, one predicate

Per Law 001, this stone is deliberately small. Three pieces:

```
Adapter (the plugin's one world object, owned by main.cpp)
 ├─ EntityRegistry   — from the core; the simulation's home
 ├─ Translator       — the edge (ADR-0024): form ↔ entity
 └─ Serializers      — registered once at init, per persisted type
```

### 1. Adapter — the world object

A single object holding the registry and the translator. It is **file-local
in `main.cpp`** — the plugin's one root object, deliberately owned by the
module, not a reachable global in the core's sense (the core's ADR-0014
governs engine state; a plugin's single world is its root).

```
Adapter::StartWorld()   — kGameLoaded / kNewGame
Adapter::EndWorld()     — kPreLoadGame / kDeleteGame
Adapter::Tick()         — deferred to the executor stone
```

- `StartWorld`: fresh registry, translate every sim-relevant actor, log the
  summary ("N settlers entered the Commonwealth").
- `EndWorld`: `Clear()` the registry, drop the translation tables. **No
  co-save yet** — that is its own stone (substrate already proven); for now
  the world is rebuilt from the game on every load.

### 2. Translator — the edge

The only place a `RE::Actor` and an `EntityId` meet. Two tables, both owned
by the translator:

```
FormID   → EntityId     (who is who)
EntityId → FormID       (what the sim is talking about)
```

The predicate — **which actors become entities**:

```cpp
// A settler is sim-relevant. Membership in the workshop settler faction.
// FORMID TO VERIFY IN FO4EDIT AT IMPLEMENTATION (commonly cited 0x0001F3A4).
bool IsSimRelevant(const RE::Actor* a_actor)
{
    return a_actor && !a_actor->IsPlayerRef()
        && a_actor->IsInFaction(settlerFaction());
}
```

The player is excluded. That is the whole rule for this stone — no race
checks, no keywords, no .esp. If the first in-game test over- or
under-captures, the predicate is one named function to change.

### 3. Components — what a settler is made of

Core components (seeded, all satisfied; the sim decays them over time):

- `Needs` — Hunger, Fatigue, Social, Safety, Comfort, all `Value = 1.0f`.
- `Memory` — empty. A settler knows nobody yet; the world fills the mind
  through `Remember` (world facts are the proven channel, 0.3.1).
- `Relationships` — empty. Trust is earned, not given.

Adapter-defined component (the entity knows its game form):

```cpp
struct FormRef
{
    RE::FormID FormId;
};
```

### 4. Serializers — the 0.4.0 substrate, used for the first time

Registered once in `F4SEPlugin_Load`, before any world exists, for every
component type the adapter persists:

`Needs`, `Memory`, `Relationships`, `Goals`, `Intent`, `FormRef`.

- `FormRef` serializes as its 4-byte FormID — stable within a load order
  (the co-save record will version it later).
- Core components serialize their plain-data fields.
- Semantics respected (per the core's handoff): a type with no serializer
  is not persisted; `Restore` needs the same registrations; `Clear` keeps
  them.

---

## The translation table — what this stone does and does NOT do

Translation happens **at the edge, in the translator** (ADR-0024):

| Direction | This stone | Later stones |
|-----------|-----------|--------------|
| Actor → entity | create entity, attach `FormRef` + seeded components | — |
| entity → Actor | `Translator::ActorFor(id)` — the tables already answer it | the executor walks the actor |
| `Hunger` ↔ ActorValue | **not yet** — the sim owns needs; nothing is written to the game this stone | executor writes through `RE::Actor` |
| `Memory.Other` ↔ merchant form | resolved via the tables (id → FormID) when logged | co-save + Remember pushes |

The discipline: **the game is read once (translate), never written** in
this stone. The write side is the executor's art.

---

## Dependencies

- **Upward:** nothing (the plugin is the top).
- **Downward:** `LCE.Core` (registry, simulation, snapshot — already
  linked), CommonLibF4 (`RE::` types), standard C++.
- **Third-party:** none new.

Files (new, each with the quote-banner ritual):

```
src/Adapter.h/.cpp      — the world object: registry + lifecycle
src/Translator.h/.cpp   — the edge: tables + IsSimRelevant + seeding
src/Components.h        — adapter-defined components (FormRef)
src/Serialization.h/.cpp— RegisterAll(registry) — one call at init
src/main.cpp            — wire lifecycle into the messaging listener
tests/                  — the adapter's first test harness (below)
```

---

## Test plan

| Where | Proves |
|-------|--------|
| Adapter tests (new, on every build) | `FormRef` serializer round-trip; translator table insert/lookup/remove with fake FormIDs; seeding produces the expected component set. Mirrors the core's harness (bool-returning suites, no framework — ADR-0010/0029) and links LCE.Core only — no game required. |
| In-game (author) | On GameLoaded the log shows the registry summary; the count matches the settlers at Sanctuary. |
| Core (already green) | The Snapshot suite proves the round-trip the serializers feed. |

---

## The four questions

- **Can it be simpler?** No — this is the minimum: one object, two tables,
  one predicate, one registration call. Anything less is not translation.
- **Does it belong?** Yes — in the adapter, at the edge (ADR-0024). The
  core gains nothing.
- **Do we need it at all?** Yes — nothing else works until settlers are
  entities. The heartbeat was the hook; this is the payload.
- **Will this help build living worlds through simulation?** Yes — the
  registry becomes real in the game, and the next stone (the executor)
  has something to act on.

---

## Decisions to review

1. **Settler = WorkshopSettlerFaction membership** (FormID to verify in
   FO4Edit; one named function). Alternatives (keyword, form list in an
   .esp) are later stones if the first test mis-captures.
2. **No game writes this stone** — translation is read-once; the
   value↔ActorValue write-through lands with the executor.
3. **Adapter test harness added** — the adapter's first tests, running on
   every build, no game required.
4. **World rebuilt on every load** — co-save is its own stone; the
   substrate (Capture/Restore) is proven and waiting.
