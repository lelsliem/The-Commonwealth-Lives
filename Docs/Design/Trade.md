# Trade — "Every Market Has a Stall-Keeper"

**Stone:** adapter 0.5.x (the trade stone)
**Status:** ✅ **IMPLEMENTED 2026-08-10** — a human arrival at the market
now resolves a trader and the exchange is real: `Trade, Success` (the
core serves AcquireFood and earns trust), instead of the placeholder
`Trade, Partial`. The suite harness pins it (BehaviourTest — the
outcomes + RecordSale; TuningTest — `sim.sale.warmth`). In-game
verification pending — the log lines to watch are below.
**Related:** the species split (`Behaviour.h`), per-settlement markets
(`SettlementMarkets.md`), the outcome channel (`Outcome.h`), ADR-0024
(game knowledge at the edge).

---

## The problem

The walking stone closed the loop with a placeholder: when a human
arrived at the market, the outcome was `Trade, Partial` — "arrived, but
no trade happened yet" — with the *workbench* as the counterparty. The
fed line proved the trip paid off, but nothing was exchanged and no one
was on the other side of the bench.

## The design

**A market is people, not a bench.** Three rules, all at the edge:

1. **The stall-keeper.** The first human to arrive at a market sets up
   its stall (`m_StallKeepers`: market entity → stall-keeper mind). They
   are fed by the settlement and their outcome is the honest `Trade,
   Partial` — arrived, no customers yet. Every later human who walks to
   that bench trades with them.

2. **The real exchange.** A human who finds a trader reports `Trade,
   Success` with the trader as the counterparty. The core does the rest
   (`ReportOutcome`): the memory is recorded, trust in the trader
   rises, and the AcquireFood ambition is served (cleared). The buyer
   still eats (`RestoreHunger` — the food the trade bought).

3. **The trader's half.** `RecordSale` (pure, Behaviour.h) — the
   stall-keeper remembers the customer (a Trade memory) and warms
   toward them: `sim.sale.warmth` (default 0.1, matching the core's
   disposition gain) on their disposition. Repeat customers are good
   for business.

**The emergent second visit.** The buyer's new memory of the *trader*
(weight 1.0 + trust) out-scores the bench (weight 1.0) in the core's
`ChooseTarget`, so the next hungry walk resolves to the person, not the
bench — and the arrival path trades with them directly (the walk target
is already a translated mind). The market becomes a place people trade
at; roles flip organically when two settlers who remember each other
meet.

## The arrival resolution

`ReportArrival` discriminates the walk target by its components: a
**market** has `FormRef` only (no `SpeciesTag`); a **person** is a
translated mind with a `SpeciesTag`.

| walk target | resolution | outcome |
|---|---|---|
| bench, stall-keeper known | trade with the stall-keeper | `Trade, Success` |
| bench, no stall-keeper | this mind sets up the stall | `Trade, Partial` |
| bench, the stall-keeper themself | no customers yet — the stall stands, no re-claim (also the restored case: the keeper comes home to their own bench) | `Trade, Partial` |
| a human mind (remembered merchant) | trade with them | `Trade, Success` |
| a child/animal mind (unreachable — defensive) | no trade | `Trade, Partial` |
| child/animal arrival | fed by owner or settlement | `Aid, Success` |

The stall-keeper map is per-world edge state, but it **is persisted**:
the co-save record's v3 stall section carries each (market, keeper) as
form-id pairs — stable across sessions, unlike the session-local
entity ids — and ApplyRestore rebuilds the map from the restored
FormRefs. A saved market reopens under the same keeper instead of
whoever happens to arrive first; a pre-v3 save (no stall section)
restores with no keepers and each market's stall re-derives on the
first arrival, exactly like a fresh world.

## The log lines

```
LCE: settler 0001CA7D sets up the stall at market 000250FE — trade begins when customers come.
LCE: settler 0001A4D7 trades with settler 0001CA7D at market 000250FE — fed, trust earned.
LCE: settler 0001A4D7 fed: Hunger 0.02 -> 1.00
```

## The seams

- **Pure vs. edge** — the outcome table and `RecordSale` are pure
  (Behaviour.h, testable — BehaviourTest); the stall-keeper map and the
  game reads live in `ReportArrival` at the edge.
- **The core is untouched** — `Trade, Success` is exactly what
  `ReportOutcome` already knew how to do; the stone only stops lying
  about the result. No engine change, no new pins.
- **The trader's ledger is adapter-side** — the core shapes the buyer's
  feelings and goal; the seller's warmth is a game fact at the edge,
  the same pattern as `RestoreHunger` ("the dog ate").
- **Tunable** — `sim.sale.warmth` in the INI; missing/broken keeps the
  default.

## Honest notes

- **The exchange is abstracted.** The buyer gives no physical goods —
  the sim's ledger is needs, trust, memory, and ambition. "Actually
  exchange something" means: hunger served, trust earned, goal cleared,
  the seller's disposition warmed, and both sides remember it. A
  physical economy (caps, goods, inventory) is a future stone.
- **The trader side needs a core feature eventually.** `RecordSale`
  mutates the seller's components directly because the core has no
  "outcome that shapes the other party's feelings without serving their
  goal" — reporting a Trade on the seller would wrongly clear their
  AcquireFood. When the engine grows that shape, `RecordSale` becomes a
  second `ReportOutcome`. (Engine ask, future.)
- **A settlement of one never trades.** Its lone settler sets up the
  stall and is fed by the settlement — honest: one person is not a
  market economy.
