# Economy — "Every Mind Carries a Few Caps"

**Stone:** adapter 0.5.x (the economy stone) + 0.8.6b (the income half)
**Status:** ✅ **IMPLEMENTED 2026-08-10** — the trade exchange is now
physical: a buyer pays caps for the meal and the seller's pouch grows.
`CapPouch` is a co-save component (serializer + stable name `cappouch`),
so a saved purse restores exactly. The suite harness pins it
(BehaviourTest — SeedPouch + PayForMeal; SerializationTest and
CoSaveTest round-trip the pouch; TuningTest pins `sim.meal.price`).
**Verified in-game 2026-08-10/11** — the trade log lines name the caps
and the pouches round-trip the co-save (restored stall-keepers with
their wallets intact across loads).
**Income half (0.8.6b): DESIGNED 2026-08-14** — the settlement
stipend closes the "no income yet" field gap (see the Income section
below). Not yet built.
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

## Income — "The settlement pays its people" (0.8.6b, the earn-caps economy)

**Status: DESIGNED 2026-08-14** — the field gap closed. The 0.8.3 test
showed the honest flaw: pouches only spend, so every non-keeper runs
dry, and a broke sick mind rests instead of buying medicine — illness
becomes a death sentence for the poor. The audit (0.8.6a) made it the
one real "missing feature." This section is that design.

**The decision: a settlement stipend — the flat daily wage.** Every
human mind draws a small stipend from their settlement's workshop once
per world-day. Not job wages (most settlers have no job), not
scavenging (random, can't guarantee a sick mind can afford a dose). A
flat stipend is the only shape that guarantees a broke sick mind can
reach the medicine price within a few days — which is the entire point.

- **The fiction is already there.** The settlement already feeds the
  broke on credit at the market; the workshop is the settlement's
  treasury, and it produces wealth (crops, water, scrap). The stipend
  is the settler's share of that — minted at the edge, never touching
  real workshop resources (deducting from the game's workshop inventory
  is real-mod territory, collision-prone, and not the show).
- **The keeper role still means something.** Keepers earn sales *on
  top of* the stipend, so wealth still concentrates at the stalls —
  but the floor is no longer zero. The stipend is a floor, not a
  leveler.
- **Household-shared.** A married couple draws one stipend, not two —
  it lands in the shared household wallet (`Households::PouchOf`), the
  same invariant the trade stone already uses. One wallet, one wage.

**The seam (co-save):** a new named component `StipendMark` — one
`std::uint64_t Day` — recording the last world-day the mind was paid.
Rides the co-save like BirthDay/Health (additive: a pre-income save
restores with no mark → Day 0 → the first tick pays everyone once, the
same back-fill spirit as `SeedPouch`). The sweep reads `CurrentDay()`;
any mind whose mark < today gets paid and its mark advances. Whole
caps; children and animals never carry one (they never barter).

**The cadence:** once per world-day, checked in the per-second tick
(the same block as `KeepBooks`/`BurialSweep`). One summary line per
settlement per day, never per-mind — the log stays hygienic:

```
LCE: economy — Sanctuary paid its 47 people 5 caps each (235 caps out).
```

**INI keys:**

```
sim.economy.stipend = 5        ; caps per mind per world-day (0 = the
                               ; stipend is off; the credit path alone
                               ; keeps the hungry fed)
```

**Honest notes**

- **The stipend mints caps.** The caps come from the settlement's
  implied production, not from any ledger — the same abstraction that
  already lets a seller's pouch grow from thin air at the market. The
  alternative (a real treasury that can run dry) is a later economy
  stone, not this one.
- **The stall-keeper never pays for their own meal** — they set up the
  stall and the settlement feeds them (they are the market's first
  customer). Their pouch only grows from sales — and now the stipend.
