# Economy — "Every Mind Carries a Few Caps"

**Stone:** adapter 0.5.x (the economy stone)
**Status:** ✅ **IMPLEMENTED 2026-08-10** — the trade exchange is now
physical: a buyer pays caps for the meal and the seller's pouch grows.
`CapPouch` is a co-save component (serializer + stable name `cappouch`),
so a saved purse restores exactly. 9/9 adapter suites green
(BehaviourTest pins SeedPouch + PayForMeal; SerializationTest and
CoSaveTest round-trip the pouch; TuningTest pins `sim.meal.price`).
In-game verification pending — the trade log lines now name the caps.
**Related:** the trade stone (`Trade.md` — this stone makes its exchange
physical), the species split (only humans carry pouches), the co-save
(`CoSave.md`), the desync stone's `IdJitter` (the pouch is seeded the
same way), ADR-0024 (game knowledge at the edge).

---

## The problem

The trade stone made the exchange real in the sim's ledger — hunger
served, trust earned, the goal cleared — but nothing *physical* changed
hands. The user's ask: a buyer pays a few caps for the meal, the
seller's pouch grows, and it survives save/load.

## The design

**A human mind always has a cap pouch.** `CapPouch` (Components.h) is a
component like FormRef or SpeciesTag — adapter-side (caps are game
knowledge, ADR-0024), persisted in the co-save under the stable name
`cappouch`.

- **Born with a few caps.** A fresh human is seeded `SeedPouch(id)`:
  40 ± 20 caps, deterministic per entity id (the IdJitter span pattern)
  — a saved mind's purse restores exactly, and the herd isn't born in
  lockstep poverty. Children and animals never carry one: they never
  barter.
- **Back-filled on restore.** A restored human without a pouch predates
  the economy (the record had no caps to carry) — `ApplyRestore` seeds
  one so a pre-economy save wakes into a living market instead of a
  world where everyone is broke.
- **The exchange.** At the trade (ReportArrival), `PayForMeal` moves
  what the buyer can afford up to `sim.meal.price` (default 5) from the
  buyer's pouch to the seller's. A broke buyer pays what they have —
  never into debt — and is still fed: the settlement covers the rest.
  Nobody starves, nobody goes negative.

## The log lines

```
LCE: settler 0001A4D7 trades with settler 0001CA7D at market 000250FE — fed, 5 caps change hands (38 left, 27 now).
LCE: settler 0001A4D7 trades with settler 0001CA7D at market 000250FE — fed on the settlement's credit (no caps).
```

The first line shows the market working; the second shows a broke
customer fed anyway. Wealth concentrates at the stalls over a session —
buyers spend down, sellers earn — which is exactly how markets behave.

## The seams

- **Pure vs. edge** — `SeedPouch` and `PayForMeal` are pure
  (Components.h, testable — BehaviourTest); the pouch reads, the price
  clamp, and the restore back-fill live at the edge (Adapter.cpp).
- **The co-save** — the pouch is a named component: serializer in
  Serialization.cpp, stable name `cappouch` in CoSave.cpp. Old saves
  (no `cappouch` blobs) restore with no pouch → 0 caps → the back-fill
  seeds one. The record format never changed — no version bump, the
  migration seam already handles it.
- **Tunable** — `sim.meal.price` in the INI; missing/broken keeps the
  default. Whole caps, minimum 1 (a free meal is not a market).

## Honest notes

- **No income yet.** Pouches only spend — nothing earns except the
  stall-keeper's takings — so wealth concentrates until buyers run
  dry and the settlement feeds the broke on credit. Wages, scavenging,
  or a caravan bringing caps is the natural next stone; the credit
  path keeps the hunger loop alive until then.
- **The stall-keeper never pays for their own meal** — they set up the
  stall and the settlement feeds them (they are the market's first
  customer). Their pouch only grows from sales.
