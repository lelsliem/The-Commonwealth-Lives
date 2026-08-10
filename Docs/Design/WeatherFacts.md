# Weather Memory — "Every Mind Knows the Day's Sky"

**Stone:** adapter 0.5.0 (weather memory events)
**Status:** ✅ **IMPLEMENTED 2026-08-10** — classification + day-stamped
world facts live; 9/9 adapter suites green (WorldFactsTest covers
classification, the kind mapping, and the day stamp). In-game
verification pending: the sky-turn and world-turns lines.
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
world fact: the world turns — day 12 begins with a clear sky.
world fact: the sky turns rain (day 12) — the day's weather is remembered.
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
