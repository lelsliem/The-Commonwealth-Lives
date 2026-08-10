# Co-save — "The World Remembers"

**Stone:** adapter 0.4.0
**Status:** ✅ **VERIFIED in-game 2026-08-10** — a save wrote 637
entities (105 KB) and the load restored them (`The Commonwealth wakes
up: 637 minds restored from the co-save`), the restored world ticking
its first pass immediately after
**Related:** core 0.4.0-alpha snapshot substrate (`RegistrySnapshot.h`),
ADR-0024 (adapters translate, don't simulate).

The contract's guarantee this stone honors: **the simulation rides inside
the game's save files** — a settler who was hungry last session is still
hungry when you load the save.

---

## The two layers

The core ships the *process-local* substrate: `EntityRegistry::Capture()`
returns a `RegistrySnapshot` (every live entity, its components as opaque
blobs, identities preserved exactly), and `Restore(snapshot)` rebuilds a
registry with identical IDs. Two properties make the snapshot unusable
as-is on disk, by design:

1. **Component keys are `std::type_index`** — addresses, meaningful only
   inside one process. The durable record must use the adapter's own
   stable names.
2. **Versioning is the adapter's job.** The core layers its own
   `kSnapshotVersion` underneath, but save-compatibility (migrating old
   saves forward) belongs to the adapter, which names the types.

So the co-save is two layers:

```
game save file (.f4se co-save record)
    └─ TLC::CoSave — the adapter's durable record:
         record version + core snapshot version + entities
         (each: id, components with STABLE NAMES + blobs)
             └─ LCE snapshot — pure data, process-local by contract
```

`TLC::CoSave::Encode`/`Decode` (`src/CoSave.h/.cpp`) are pure — no game
types cross them, so they are testable without the game (CoSaveTest).

## The record

- **Type / UID:** `'LCEW'` (0x4C434557) — the UID is a placeholder for
  the F4SE-assigned one (the author-handle TODO in `xmake.lua` is the
  same open item).
- **Version:** `kRecordVersion = 3`, the adapter's schema version. Bumped
  only on a breaking change to the record *format* (the header or a
  trailing section); old versions are migrated forward, never dropped.
- **Layout (v3), all little-endian:**

  ```
  u32 recordVersion
  u32 coreSnapshotVersion
  u32 entityCount
  u64 rngState                  ← v2 (the decay-jitter wiring): the world's
                                  randomness, resumed on restore
  per entity:
      u64 id (index + generation packed)
      u32 componentCount
      per component:
          u8  nameLength, name bytes   ← the stable names below
          u32 blobLength, blob bytes   ← the core's opaque component data
  u32 stallCount                ← v3 (the stall-keepers stone): who runs
  per stall:                       each market's stall, in form ids —
      u32 marketFormId             stable across sessions, unlike the
      u32 keeperFormId             session-local entity ids
  ```

  Decode reads `rngState` only when the record version is ≥ 2; a v1
  record leaves the caller's pre-seeded default stream untouched (that
  world never had a saved stream — it is reseeded fresh, which is
  honest). The stall section is read only when the record version is ≥ 3;
  older records end after the entities, so a restored market's stall
  re-derives on the first arrival (a safe default, like a missing
  component).

- **Stable names** (chosen once, never renamed — a rename is a schema
  change, migrate instead):

  | component | name |
  |-----------|------|
  | `Needs` | `needs` |
  | `Memory` | `memory` |
  | `Relationships` | `relationships` |
  | `Goals` | `goals` |
  | `Intent` | `intent` |
  | `FormRef` (adapter) | `formref` |
  | `SpeciesTag` (adapter) | `species` |
  | `CapPouch` (adapter) | `cappouch` |

- **Refusal is the contract:** `Decode` returns false — the load is never
  half-applied — when the record version is unsupported, a component name
  is unknown, or the bytes are truncated (every read is bounds-checked
  against the record length).

## The lifecycle

```
PreSaveGame ──► CaptureWorld() ──► CoSave::Encode ──► WriteRecord
load         ──► ReadRecord ──► CoSave::Decode ──► QueueRestore(snapshot, rngState)
kGameLoaded  ──► pending? ApplyRestore(snapshot, rngState) : StartWorld() fresh
PreLoadGame / new game ──► EndWorld() (Clear; serializers survive)
DeleteGame (a save FILE was deleted) ──► nothing — the world keeps
running. EndWorld here killed the sim mid-world once: an autosave
rotation deleted a save, the world ended, and no pending load existed to
revive it — later saves wrote 0 entities until a full restart.
```

The F4SE serialization callbacks (`SetSaveCallback` / `SetLoadCallback` /
`SetRevertCallback`) live in `main.cpp` — the only F4SE touch in the
chain. The adapter stays game-free: it exposes `CaptureWorld()` and
`QueueRestore(snapshot)`, and `GameLoaded()` applies a pending restore —
or starts a fresh world when the co-save held nothing (a new game, or a
save made while the sim was not running).

`ApplyRestore` (adapter side):
1. `EndWorld()` — Clear keeps the serializers (registered once at init).
2. `m_Registry.Restore(snapshot)` — identical IDs, same components.
3. Rebuild the **translator** from the restored `FormRef` components —
   the edge's form↔entity memory is adapter state, never part of the
   snapshot.
4. `EnsureMarket()` — the market entity was saved with the world (it owns
   a FormRef); no-op when already known.
5. Resume ticking — needs keep decaying from where they were.

Restored settlers whose forms aren't loaded in the new session sit idle
and are refused by the plan builder ("actor not loaded") until they load
— the same path as any unloaded actor, no special case.

## The restore re-seeds the market (a walking bug, fixed 2026-08-10)

**Verified in-game:** after the fix, a restored world's junkyard dog
walks to the workbench when hungry — restored worlds walk again.

A restored world was **market-blind**: the restore rebuilt entities and
the translator but never re-ran the market seed, and the seed itself is a
fading memory event (weight 1.0, `MemoryFadeRate` 0.2/s — forgotten in
seconds). A save made long after its world woke contains minds that have
already forgotten the market; restoring them produced a world where
starving minds Explore instead of deciding `MoveTo`.

Two fixes, layered:

1. **`SeedMarket` is a shared helper** called from **both** `StartWorld`
   and `ApplyRestore` — the market is open on every world start, fresh
   or restored. (The log tells it: restored worlds never printed
   "market is open" before; now they do.)
2. **The seed is periodic and idempotent.** A restore brings 637
   entities back, but their actors load gradually — the one-shot seed
   at the restore instant missed everyone whose actor wasn't loaded
   yet. The tick re-pushes the fact every second (`SeedMarket(false)`,
   silent), and `SeedMarketMemory` skips minds that already remember
   (the core erases faded events, so a missing event means truly
   forgotten) — memory never grows, and late-loading minds learn where
   to trade. MarketTest pins both properties.

## Migration

**IMPLEMENTED 2026-08-10.** The record is self-describing — every
component rides under its stable name — so most schema changes need no
byte translation at all, and the seam is two small rules in `Decode`:

1. **`version > kRecordVersion` refuses** (a future format is not ours
   to guess — never half-apply); **`version <= kRecordVersion` loads.**
   An older record simply decodes without the components the newer build
   added, and the safe default applies (a pre-`species` save restores
   minds with no tag, which reads as Human).
2. **A component name this build does not know is skipped and dropped**
   — a type a later build removed. Its bytes are consumed (the record
   stays aligned) and the entity keeps everything else. This is how a
   breaking *removal* migrates.

The first real changes in the wild were additive — `species` (the
species/behaviour stone) and `cappouch` (the economy stone) — which is
why the version never bumped and old saves kept loading: the format
didn't change, the contents did. The version bumped only when the
*format* itself changed: v2 (the decay-jitter wiring) added the `Rng`
state to the header, and v3 (the stall-keepers stone) added the stall
section after the entities.

**Tested:** CoSaveTest crafts a v0 record (no `species`, plus a `legacy`
component) → decodes, drops `legacy`, restores with the tag absent, the
caller's default Rng stream untouched, and the stall list empty; a v1
record (no `Rng` state) → decodes with the default stream standing and
no stall section; a v4 record → refused; the round-trip record with a
`needs` name patched to unknown → decodes with exactly that one
component dropped; the v3 round-trip → the encoded Rng state handed
back exactly and the (market, keeper) stall pair round-tripping as
form ids.

## Files

```
src/CoSave.h/.cpp — the durable record codec (stable names, versioning)
src/BlobCodec.h   — U8/Raw/Remaining added to the little-endian codec
src/Adapter.h/.cpp— CaptureWorld, QueueRestore, ApplyRestore; GameLoaded
                    applies the pending restore
src/main.cpp      — F4SE serialization callbacks (Save/Load/Revert glue)
tests/main.cpp    — CoSaveTest (6/6 suites green)
```

## Test plan

| Where | Proves |
|-------|--------|
| Adapter tests (on every build) | **CoSaveTest** — the full durable round-trip (world → snapshot → record → bytes → back → restore, stable names literally in the bytes), plus the refusal paths (truncated, newer version) and the migration paths (older version loads forward; a removed component is skipped and dropped). |
| In-game (author) | ✅ **Verified 2026-08-10**: save during play → `co-save: writing N entities (M bytes)`; load that save → `co-save: read N entities — the world will be restored on load` then `The Commonwealth wakes up: N minds restored from the co-save` — the load's completion event (`kPostLoadGame`; `kGameLoaded` alone does not fire for real loads, which is why the completion event is the trigger) applies the restore, and the settlers resume where they left off. |
