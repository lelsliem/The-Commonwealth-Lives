# The Living Commonwealth — 0.5.0-beta

**Fallout 4 adapter for the Living Commonwealth Engine (LCE)** — an F4SE
plugin that makes the Commonwealth *live*.

> A settler goes to market because they are hungry — no script.

The settlers aren't on quest scripts. They are **hungry**, they
**remember** where to trade, they walk to **their own settlement's
market**, and the exchange is physical: caps change hands, the
stall-keeper's purse grows, trust is earned. The market has **hours** —
it closes at night and nobody walks to a closed bench — and the day's
**weather** is remembered. The game does nothing but show the result.

In-game verified end to end: hungry settlers decide `MoveTo`, walk to the
bench, arrive, trade, and the whole world survives save/load.

## What's in this release

- **0.1 — The heartbeat.** The plugin loads and breathes.
- **0.2 — The translation.** Settler-faction actors become minds.
- **0.3 — The intent executor.** The sim ticks every frame and settlers walk.
- **0.4 — The co-save.** The world rides inside the save file (record v4:
  entities, the seeded RNG stream, who runs each market's stall, and each
  memory's world day — the timestamp survives save/load).
- **0.5 — The living world.**
  - Species split: children and animals don't barter — a dog gets fed and
    gives nothing in return.
  - World facts: the market's hours push to memory — closed at night,
    nobody walks to a closed bench.
  - Weather memory events: settlers remember the day's sky.
  - Per-settlement markets: a persistent-cell census (FO4 never fills the
    REFR form array) gives every settlement its own bench and its own
    stall-keeper.
  - The trade stone: a stall-keeper per market; the buyer remembers the
    merchant.
  - The economy stone: cap pouches that round-trip through the co-save.
  - Per-mind decay desync (`VaryNeeds`) plus the engine's per-tick decay
    jitter — a seeded RNG, persisted in the co-save.
  - Hardening: `DeleteGame` no longer kills a running world, and the walk
    probe reads the actor's data position instead of a lying 3D transform.
  - Tuning: `Data\F4SE\Plugins\TheLivingCommonwealth.ini` (template in
    `config/`) — hunger rhythm, market hours, meal price, all editable
    without recompiling.

## Install

1. **Requirements:** Fallout 4 1.11.221 (Next-Gen), F4SE 0.7.8.
2. Copy `TheLivingCommonwealth.dll` and `TheLivingCommonwealth.ini` (from
   `config/`) into `Data\F4SE\Plugins\` — the DLL and INI sit side by side.
3. Launch the game through F4SE. The first load greets you:
   `The Living Commonwealth v0.5.0.0 loaded (build <hash>).`

Works with mod managers — the mod folder mirrors the game's `F4SE\Plugins`
structure (`mods\The Living Commonwealth\F4SE\Plugins\`).

## Notes for a beta

- Logs live at `Documents\My Games\Fallout4\F4SE\TheLivingCommonwealth.log`.
  The version banner is stamped with the build's git hash — the log always
  says which DLL ran.
- The tuning INI is optional; without it the sim runs its built-in
  defaults (the fast demo rhythm). With the shipped template, hunger is
  tuned to a few meals a day.
- 9/9 harness suites green (translator tables, seeding, snapshot, co-save
  round trips, and more).

## Links

- Repository: https://github.com/lelsliem/The-Commonwealth-Lives
- Roadmap: `Docs/Roadmap.md` — every stone through 0.5.0 is in and
  verified in-game; 0.6.0 is scoped from the engine's next hand-over.
- The engine: the Living Commonwealth Engine (LCE) — the simulation core
  this adapter is a client of.
