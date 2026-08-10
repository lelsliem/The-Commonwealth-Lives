# Per-Settlement Markets — "Trades by Their Own Bench"

**Stone:** adapter 0.5.0 (per-settlement markets)
**Status:** ✅ **IMPLEMENTED 2026-08-10** — the settlement census +
per-mind nearest-workshop resolver live; 9/9 adapter suites green
(MarketTest covers NearestWorkshop, SeedMarketMemory, and the species
resolver). In-game verification pending: the `settlement census:` log
line, and each settler walking to its own settlement's bench.
**No new pins:** the workshop base form (`000C1AEB`) was already
verified as part of the Sanctuary market pin's comment in Market.h.

## Why the single market was wrong

One market entity (`000250FE`, Sanctuary workshop). Every mind that knew
it walked to Sanctuary — a settler in Tenpines, 22,000 units away,
decided MoveTo for the Sanctuary bench. That was the walking stone's
behavior: a proof of concept that proved *the loop* but also proved *the
problem*. Real settlements don't trade at a single workbench.

## The census

`Adapter::RefreshWorkshops` enumerates the worldspaces' **persistent
cells**: `TESDataHandler::GetFormArray<RE::TESWorldSpace>()` → each
worldspace's `persistentCell` → `ForEachReference`, keeping every ref
whose base form matches the vanilla workshop workbench (`000C1AEB`).
Position comes from the ref's data record (`data.location`), valid even
for refs in unloaded cells — one pass, no cells loaded, complete
census.

Why not the flat REFR array? FO4 does not store REFRs in
`TESDataHandler::formArrays` the way Skyrim does —
`GetFormArray<RE::TESObjectREFR>()` is always empty in-game (verified:
`0 REFRs probed` across retries, two world starts). Settlement
workbenches are all *persistent* refs, and every persistent ref lives
in its worldspace's persistent cell, which is loaded at world start —
so the persistent-cell scan is the correct FO4-native enumeration.

Static per load order, so a **non-empty** result is final (found once,
it survives world clears). An **empty** result is not pinned: the
census can run before the game's worldspace data is fully populated, so
`RefreshWorkshops` retries on the seed cycle (throttled to ~every 5 s)
while the fallback below covers the world — a false 0 would lock the
whole session into the single-bench mode. Modded workshops that share
the `000C1AEB` base join the census; custom-base workshops (Vault 88,
the Mechanist's lair) are missed — documented and benign (their
settlers explore toward the nearest standard market).

## The per-mind resolver

`SeedMarket` (with a non-empty census) calls `SeedMarketMemory` with a
per-mind lambda instead of a global market entity. For each mind:

1. **Load checkpoint** — the mind's actor must be loaded; restored minds
   whose actors haven't streamed in yet are skipped and caught by the
   periodic seed a second later.
2. **Nearest workshop within range** — `NearestWorkshop(actorPos, census,
   kMarketRadius)` returns the closest market within ~140 m (10000
   units), or 0 if none is close enough (in the wastes → no market →
   explore). Squared distances, no `sqrt`.
3. **Species resolution** — humans trade at their settlement's market;
   children and animals follow the owner-or-settlement rule (the same
   resolver from the food-source stone, now per-workshop).

## Log lines

```
settlement census: 4 worldspaces, 42 workshops known (base 0xc1aeb) — markets are per settlement.
settlement census: 4 worldspaces, 0 workshops known (base 0xc1aeb) — will retry.   ← when nothing matched yet
census probe: world 0x3c persistent REFR 0x250fe base 0xc1aeb.   ← once per session, the diagnostic that says what the filter saw
The market is open: every mind remembers where its own settlement trades (42 workshops).  ← seed announce
```

A 0-result census logs the worldspace count and — once — the first
base forms actually in the persistent cells, so a miss tells us whether
the scan found nothing or the workbench's real base differs from the
pinned `000C1AEB`.

A settler at Tenpines shows `decides MoveTo -> 00080F7B` instead of the
old `Decides MoveTo -> 000250FE` (000250FE is Sanctuary's form). The
formid in the MoveTo log is your verification that per-settlement
mapping works.

A settler at Tenpines shows `decides MoveTo -> 00080F7B` instead of the
old `Decides MoveTo -> 000250FE` (000250FE is Sanctuary's form). The
formid in the MoveTo log is your verification that per-settlement
mapping works.

## The fallback

When the census is empty (interior cells, the pre-world state, or a bare
world with no REFR array — or the array not yet populated at wake), the
`will retry` census keeps scanning while `SeedMarket` degrades to the
legacy single-bench behavior exactly as the walking stone had it:
`EnsureWorkshop` for `000250FE`, radius filter, the old announce lines.
A lone known market beats no market — the sim never forgets to eat —
and the moment the census finds its workshops, the next seed goes
per-settlement.