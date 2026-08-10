# Co-save — "The World Remembers"

**Stone:** adapter 0.4.0
**Status:** Implemented (record + lifecycle + tests) — in-game save/load
verification pending
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
- **Version:** `kRecordVersion = 1`, the adapter's schema version. Bumped
  only on a breaking change; old versions are migrated forward, never
  dropped.
- **Layout (v1), all little-endian:**

  ```
  u32 recordVersion
  u32 coreSnapshotVersion
  u32 entityCount
  per entity:
      u64 id (index + generation packed)
      u32 componentCount
      per component:
          u8  nameLength, name bytes   ← the stable names below
          u32 blobLength, blob bytes   ← the core's opaque component data
  ```

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

- **Refusal is the contract:** `Decode` returns false — the load is never
  half-applied — when the record version is unsupported, a component name
  is unknown, or the bytes are truncated (every read is bounds-checked
  against the record length).

## The lifecycle

```
PreSaveGame ──► CaptureWorld() ──► CoSave::Encode ──► WriteRecord
load         ──► ReadRecord ──► CoSave::Decode ──► QueueRestore(snapshot)
kGameLoaded  ──► pending? ApplyRestore(snapshot) : StartWorld() fresh
PreLoadGame / DeleteGame / new game ──► EndWorld() (Clear; serializers survive)
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

## Migration

The seam is the `kRecordVersion` switch in `Decode`. Version 1 is the
first; when the schema changes, `Decode` gains a case that translates old
records into the current shape instead of refusing them. Old saves load
forward; nothing is dropped silently.

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
| Adapter tests (on every build) | **CoSaveTest** — the full durable round-trip (world → snapshot → record → bytes → back → restore, stable names literally in the bytes), plus the refusal paths (truncated, unsupported version, unknown component name all refuse). |
| In-game (author) | Save during play, load the save, and watch the log: `co-save: writing N entities (M bytes)` on save, `co-save: read N entities — the world will be restored on load` + `The Commonwealth wakes up: N minds restored from the co-save` on load — and the settlers resume with their needs where they left off. |
