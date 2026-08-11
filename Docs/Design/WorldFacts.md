# World Facts — "Markets Close; Memories Fade; Hunger Doesn't"

**Stone:** adapter 0.5.0 (world facts)
**Status:** ✅ **VERIFIED in-game 2026-08-11** — market-hours gate live
(no pins), radstorm gate live (CommonwealthGSRadstorm `001C3D5E` pinned
from the xEdit dump — see `Docs/WeatherForms.md` for the full catalog
and the exclusion decisions); the suite harness pins it (WorldFactsTest
among them). The transition lines fire (`world fact: the market is
closed (10:00) — trade unavailable until 24:00`), settlers stop their
market walks at night, and 0.7.0 proved the gate's flip side: the
closed-market fact gating the hungry walk is exactly why the feud
needed the engine's desperate-hunger gate.
**Related:** the core's `Remember` world-fact channel (`Simulation.h`:
a memory event with an *invalid* Other is a fact — "the market is closed
today" is `{ invalid, Trade, weight }`; while remembered, the core's
`Decide` treats that kind as unavailable; when it fades below the forget
threshold, the door reopens). ADR-0024 (game knowledge at the edge), the
market stone (`Market.h`), the tuning stone (the hours become
`market.open.hour` / `market.close.hour` in the user-editable file).

---

## The mechanism

The core never queries the world (ADR-0024); a *world fact* is how the
world reaches a mind. The adapter reads the game's clock and sky at the
edge, then pushes a memory event with an **invalid Other** — nothing to
meet, no one to trust, so no relationship is shaped (the core's
`Remember` returns before the trust/disposition switch for invalid
Others). While a mind remembers `{ invalid, Trade }`, the core's hunger
branch sees `IsUnavailable(Trade)` and **explores instead of moving to
the market**; `{ invalid, Social }` shuts the gatherings the same way.
The tick fades facts at 0.2/s and erases them below 0.1 — that fade is
how a door reopens.

Two facts this stone, both pushed to **every** mind (world facts are
global knowledge — unlike the market-location seed, which is
radius-filtered):

| Fact | Kind | Source of truth | Effect while remembered |
|---|---|---|---|
| The market is closed | `Trade` | game hour vs. trading hours | hungry minds Explore — no night walks |
| Radstorm | `Social` | `Sky::currentWeather` FormID table | no Socialize intents — nobody gathers |

## The refresh pattern

A pushed fact dies in ~4.5 s (weight 1.0, fade 0.2/s, forgotten ≤ 0.1).
A market that closes at 20:00 must stay closed all night, so the adapter
**refreshes** active facts: every second, `WorldFacts::ApplyFact` finds
the mind's fact of that kind and tops its weight back to full — or pushes
it if the core erased it. Consequences:

- **No flicker** — a shut door never fades open; the tick's erase is
  pre-empted every second.
- **No memory growth** — one fact event per kind, refreshed in place,
  never duplicated (the MarketTest/WorldFactsTest idempotence pins).
- **Prompt reopen** — when the condition flips (08:00), refreshing
  stops and the tick fades the fact out in ~4.5 s: the close-down the
  transition line promised.

The transition lines (the only log lines the stone emits — the push is
silent):

```
world fact: the market is closed (23:41) — trade unavailable until 08:00.
world fact: the market is open (08:01) — trade available.
world fact: a radstorm rolls in — no one gathers while it lasts.
world fact: the radstorm passes — gatherings resume.
```

## The hour gate

`WorldFacts::IsMarketClosed(hour, open, close)` is pure and handles
overnight wraps (open 20, close 8 trades through midnight). Defaults
08:00–20:00, open-inclusive / close-exclusive. The hours are the first
**tuning keys** — constants in `WorldFacts.h` until the tuning stone
wires the user-editable file. `SeedMarket`'s announce became hour-aware:
the classic "market is open" line only prints when the market actually is
open; at night it says "remembered but closed (23:41): trade resumes at
08:00". The location memory is still seeded at night — minds learn
*where* the market is even when the door is shut.

The gate applies to every species uniformly: at night the settlement
sleeps — no trading, no feeding at the bench; a hungry animal explores
until dawn. (An animal exemption — "a dog is fed at home whenever it's
hungry" — would be one predicate in the push; deliberately not built
yet, because a dog walking to the bench at night would read as the gate
being broken.)

## The radstorm gate

`Adapter::IsRadstorm` classifies `Sky::GetSingleton()->currentWeather`
by FormID. The table is **pinned** (2026-08-10) from the xEdit dump:
only CommonwealthGSRadstorm (`001C3D5E`) matches. The NoHazard variant
is excluded by design — no hazard, and the gate is the green air, not
the sky — and the Old/Backup records are editor-only. See
`Docs/WeatherForms.md` for the full catalog. (commonlibf4 does not model
TESWeather's flag storage, so there is no pin-free signal to lean on —
the FormID table is the honest route.)

## What is deliberately not here

- **No weather effects on Trade** — rain does not close the market;
  only radstorms shut the gatherings. The facts are chosen for a clear,
  observable story; more facts grow the table, they don't change the
  mechanism.
- **No per-mind facts** — every mind hears the same doors. Per-mind
  facts (a feuding settler refuses the market) belong to the
  relationship stones.
- **No eager erase on reopen** — the fact fades on the core's clock
  (the designed reopen). A ~4.5 s close-down after opening hour is
  flavor, not a bug.
