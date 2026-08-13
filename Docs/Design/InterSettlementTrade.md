# Inter-Settlement Trade — "The Road Between"

**Candidate stone** (designed 2026-08-13, **not scheduled**) — the
roads verdict from the engine-pattern audit (Run080.md): the game
already gives us road *traders* (0.7.4, done); what the sim lacks is
the connective meaning — settlements that trade with each other. This
doc is the design, ready to weigh at the 0.8.6a audit or build as the
first post-beta stone. It is deliberately **not** part of the beta
run as planned: it adds state and a new walk class, and the beta
scope should not grow for it.
**Status:** DESIGN — nothing built.
**Related:** Economy.md ("a caravan bringing caps is the natural next
stone" — this is that stone), Trade.md, SettlementMarkets.md (the
per-settlement census this rides), Walking.md (long walks already
work — provisioners do them), WorldFacts.md (the gate pattern the
blocked road would extend), Behaviour.md (species split — the courier
must be human), Run080.md (the audit where this gets decided).

---

## The problem

Every settlement's market is an island. A hungry mind walks to its
own bench, trades caps, eats; the stall-keeper's pouch hoards the
settlement's wealth. Nothing moves **between** settlements — a
settlement that can't feed itself (a birth wave, a plague of
shortages, medicine stock sold out in 0.8.3) has no recourse but the
credit path, and its neighbours never notice. The Commonwealth is a
network of roads; the sim doesn't use them.

## The design — one sentence

**When a settlement's market runs dry, the keeper spends the
settlement's hoard to send a provisioner to the nearest fed
settlement, buy food, and bring it home — a starving settlement
imports instead of collapsing.**

Four pieces, all adapter-owned, zero new engine surface:

### 1. The market ledger (new adapter state, co-save additive)

Per workshop, a small day-scoped ledger:
- **The day's food budget** — how many meals the stall can serve
  today (default: the number of minds it fed yesterday, floor 1 —
  `sim.trade.budgetBase` in the INI). Meals draw from it.
- **Ran dry?** — a per-day flag set when the budget hit zero before
  the market closed. This is the shortage signal.
- **In-flight caravan?** — one slot: a short settlement has at most
  one courier on the road at a time (no caravan convoys; simple).

The ledger is day-scoped: on the world turn, yesterday's run-dry flag
decides today's caravan, then resets. Old records simply have no
ledger — first day back-fills from "fed everyone, ran dry never."

### 2. The courier (existing minds, existing walk)

When a settlement is short, the stall-keeper (or a supply-line
provisioner mind if one is home — `sim.trade.courier` = keeper or
provisioner) gets a `CarryFood` walk: a `MoveTo` to the **nearest
settlement whose stall did NOT run dry** (the Market.h census already
enumerates workshops and proximity). On arrival it spends
`sim.trade.caravanCaps` (default 40) from **the keeper's pouch** —
the keeper's hoard *is* the settlement treasury, no new wallet — and
the neighbour's pouch grows. Then it walks home.

### 3. The arrival (existing trade path)

On the return arrival, tomorrow's budget is topped up by
`sim.trade.caravanFood` (default 25 meals) and the shortage clears.
The player reads it in the log and the radio:

```
trade: 000250FE ran dry — Marcy sends Jun Long up the road to Starlight.
trade: Jun Long returns from Starlight with food for the settlement.
```

### 4. The gates (the world facts the roads need)

- **Radstorm days already keep people in** ("no one gathers while it
  lasts") — the shortage just lasts another day. Free.
- **The blocked road** — a future world fact (`{ invalid, Trade }`
  style, the market-hours gate pattern): while remembered, the
  courier's arrival is delayed. Not built now; the gate machinery
  exists and the caravan is where it would bite first.

## The seams

- **Pure vs. edge** — the ledger math (budget, ran-dry, top-up) is
  pure and **already locked**: `src/TradeLedger.h` + `TradeLedgerTest`
  (0.8.1, 26/26 green) pin the floor, the meal draw, the ran-dry
  read, the one-courier gate, and the never-into-debt cost clamp
  before the stone is ever built — the audit starts from a pinned
  contract, not a blank page. The walk issue and the census read
  live at the edge (Adapter.cpp) when the stone is scheduled.
- **The co-save** — the ledger rides the adapter's record (additive,
  like `cappouch`): a stable name + serializer, no format bump, old
  saves back-fill. In-flight caravan state persists so a caravan
  survives save/load (a courier mid-road returns after the load and
  still tops up).
- **Tunable** — `sim.trade.budgetBase`, `sim.trade.caravanCaps`,
  `sim.trade.caravanFood`, `sim.trade.courier` in the INI; broken
  values keep defaults (the Tuning seam).
- **Species** — only humans carry pouches and walk caravans
  (Behaviour.md). An animal is never a courier; the courier pick
  filters the species split.
- **Never urgent** — the credit path (Economy.md) still catches the
  broke; the caravan is a market feature, not a survival necessity.
  Nobody starves while the courier walks.

## Honest notes

- **No real inventory.** Food is a number on the ledger, not cargo
  the player can see. The *feeling* of the road is what this builds —
  provisioners visibly walking between settlements — not logistics.
- **No route graph.** "The road" is a walk, not a graph. The nearest-
  fed-settlement scan is one hop; no multi-leg routes, no waypoints.
  Blocked roads delay, they don't reroute.
- **One courier at a time** per short settlement — the ledger slot
  is the rate limit, so a famine can't spawn a caravan army.
- **What it needs from the engine: nothing.** Walks, pouches, the
  census, world facts, and the co-save all exist. If it ever needs a
  distinct caravan valence, that's the append-only `InteractionKind`
  precedent — not requested.

## Verification

1. Force a shortage: set a settlement's budget to 0 in a test INI —
   its stall runs dry, the ledger flags it, and a courier walks out
   to the nearest fed settlement and back.
2. Save mid-caravan, reload: the courier returns after the load and
   the budget still tops up (ledger round-trip).
3. Natural session: a birth wave (0.7.7, `sim.birth.enabled`) strains
   a settlement's budget and the first `sends ... up the road` line
   appears in the log.
4. The radio announces the caravan's return; the neighbour's pouch
   actually grew.

---

## Reconciliation — the engine's Roads sample (2026-08-13)

The engine's 0.8.3 Roads sample (SAMPLE 10) proved a better model
for the connective layer than this doc's simple blocked-road gate:
**a road is a LegacyStore fact — its Weight IS its condition.**
Traffic maintains the road it travels (LeaveLegacy with a higher
weight), weather and neglect wear every road (the world's own tick),
caravans read the books and prefer the best-maintained route, and a
wrecked road is eventually forgotten from the world's books.

What this changes in this design, at audit time:

1. **Routes are legacy facts, not a gate flag.** Each settlement-pair
   road (`road:<a>:<b>`) is a fact whose weight is its condition;
   the courier's pick weighs condition against distance instead of a
   binary "blocked or not."
2. **The caravan maintains the road it walks.** The courier's arrival
   re-pushes the route's weight higher — traffic is love, and the
   road remembers it. A neglected settlement's road decays; a busy
   one's stays open.
3. **Weather wears every road** (the world's tick decays weights) —
   the radstorm that already keeps people in also roughens the roads
   that night.
4. **Zero new engine surface stands** — the adapter already owns
   LeaveLegacy/ReadLegacy (0.7.0 death legacies); a road is just
   another legacy fact. Same ledger, same courier, same audit
   decision — this section is the shape it should take if taken.

## The build decision

This stone is **designed and ready, not scheduled**. The 0.8.6a
audit weighs it against the beta scope; if it doesn't fit the beta,
it is the first post-beta stone. Either way, nothing here blocks the
run to 0.9.2c, and the engine needs no change for it.
