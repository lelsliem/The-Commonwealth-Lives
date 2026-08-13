# Weather Memory — "Every Mind Knows the Day's Sky"

**Stone:** adapter 0.5.0 (weather memory events)
**Status:** ✅ **VERIFIED in-game 2026-08-11** — classification +
day-stamped world facts live; the suite harness pins it
(WorldFactsTest covers classification, the kind mapping, and the day
stamp). The sky-turn and world-turns lines fire in the live log
(`world fact: the sky turns rainy (day 269)`, `the world turns — day
270 begins`).
**Engine prerequisite:** `InteractionKind` grew the weather kinds
(`WeatherClear` … `WeatherRadstorm`, appended at the end — save-safe
for the ordinal-based co-save). Engine commit `3909049`.
**Related:** `Docs/Design/WorldFacts.md` (the gates), `Docs/WeatherForms.md`
(the verified form catalog), the core's `WorldTime.Day` stamp (0.5.0).

## The idea

The gates (market hours, radstorm) are *doors*: while remembered, an
interaction kind is unavailable. The day's weather is the opposite — a
*label* the sim remembers, never a door. Rain must not close the market,
and nobody should forget it rained the day the caravan arrived. So the
weather fact is a day-stamped memory event with an invalid Other:
`{ invalid, WeatherRain, 1.0, day }` reads "day 12 was rainy". Decide
never gates the weather kinds — they are pure facts.

## Why the engine grew six kinds

`InteractionKind` was `{ Trade, Combat, Aid, Social, Wronged }` — all
interaction kinds. A weather fact needs a label, and reusing an existing
kind was a trap:

- **Trade / Social** gate the market walk and the gatherings — rain
  would close the market. Forbidden by design (WorldFacts.md keeps rain
  out of the gates).
- **Combat / Aid / Wronged** are memory-only today, but they are
  *interaction* categories: a fact labeled Combat would read as a wrong
  to future recall, and any of them could gain gating semantics as the
  core grows.

One kind per category (`WeatherClear` … `WeatherRadstorm`) makes a fact
a label. The kinds are appended at the end of the enum, so the adapter's
ordinal-based co-save stays save-safe: old saves have no such events,
and new events decode to valid values.

## The classification

`WorldFacts::ClassifyWeather(formId)` maps the verified live-weather
forms (xEdit dump 2026-08-10) to six categories; everything else —
interiors, editor backups, FX, modded skies — is `Unknown` and leaves
no fact ("we do not remember what we do not know"). The full catalog is
in `Docs/WeatherForms.md`; the live forms are all `00`-prefixed vanilla,
stable across load orders.

| Category | Live forms |
|---|---|
| Clear | CommonwealthClear, ClearestSkies, SanctuaryClear |
| Overcast | CommonwealthOvercast, GSOvercast |
| Rain | CommonwealthRain |
| Fog | CommonwealthFoggy, GSFoggy |
| Misty | CommonwealthMisty, MistyRainy |
| Radstorm | CommonwealthGSRadstorm |

## The refresh pattern, day-scoped

The gates refresh while active and fade when the condition flips. The
weather adds the *day* to that pattern:

- `m_WeatherSeen` is a bitmask of the categories today's sky has shown.
- Every second, each category seen today is refreshed in place with
  **today's day stamp** — "it rained this morning" stays remembered
  until the world turns.
- When the game day rolls over (`Calendar::gameDaysPassed`), the seen
  set resets; yesterday's categories are no longer refreshed, and the
  core's tick fades them out (~4.5 s). The day-turn gets a log line.

One fact per category per mind, so memory never grows beyond six weather
facts.

## Re-derived, never persisted

Weather is the live sky — edge-derived state, not accumulated
experience. After a load, `PushWorldFacts` re-seeds today's sky within a
second, so weather facts are deliberately **not** co-save state (no
record-version bump; the `Day` field stays 0 on restore and the re-push
re-stamps). The seen-set is session state: a save/load within the same
day forgets the morning's rain and remembers only what the sky is now.

## The lines the player reads

```
world fact: the world turns — day 12 begins, the sky clear.
world fact: the sky turns rainy (day 12) — the day's weather is remembered.
world fact: the sky is unclassified — no weather memory.   (interiors etc.)
```

Force the sky with `fw 1ca7e4` (rain), `fw 2b52a` (clear), `fw 1c3d5e`
(radstorm — which also fires the existing gate lines).

## Deliberately not here

- **Day-scale recall** ("did it rain on day N?") — the Day stamp is the
  substrate (0.7.0 Legacy), but today the tick fades uniformly, so
  yesterday's weather leaves memory within seconds of the day turning.
  Age-aware fade is the Legacy stone's job.
- **Weather-gated behaviour** — rain changes nothing a mind *does* yet;
  it only remembers. A future stone can read the weather fact and e.g.
  prefer Rest indoors when it rains (a new gate kind, engine-side).
- **The `Feed` engine ask** (from the real test) is unchanged.

## Seasons — the audit candidate (2026-08-13)

The engine exposes `SeasonOf(day)` (four 90-day seasons, derived never
stored — core stone 06) and the adapter does not use it yet. The
candidate, if the 0.8.6a audit takes it: **a season that bites**.

- The adapter reads the season in the tick and scales need decay and
  the illness vectors by INI multipliers — `sim.season.hungerMult`
  (defaults 1.0, winter 1.15), `sim.season.illnessMult` (winter
  raises the food/contagion chances). A harsher winter makes
  settlements leaner and sickness likelier; summer stays the default
  world.
- Pure tuning: a multiplier applied to existing decay, derived from
  the day — no new state, no co-save, no engine change. The same
  `SeasonOf` read that drives it costs nothing when the multiplier
  is 1.0.
- The gate (if any) is deliberate: a season should never close the
  market (WorldFacts.md keeps rain out of the doors) — it leans on
  the same needs, it doesn't lock them.

Verdict captured in Run080.md's engine-pattern table: covered, one
candidate — this is it. Decision point: the 0.8.6a audit.
