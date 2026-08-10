# The Real Test — "The Settler Goes to Market"

**Stone:** adapter 0.5.0 (the contract's real test)
**Status:** ✅ **IMPLEMENTED 2026-08-10 — pending in-game verification.**
Build `b7f0aa0` (the hunger write-through); the user verifies the loop
closes in-game before the docs flip fully.
**Related:** core 0.3.0 (Decide), 0.4.0 (snapshot), 0.5.0 stones 01–02
(tuning, ReportOutcome), ADR-0024 (adapters translate, don't simulate).

The contract's guarantee, in one sentence: **a settler goes to market
because they are hungry — no script.** The walk exists. The arrival
exists. What does not exist yet is the *loop*: nothing ever makes the
settler not-hungry, so the trip never pays off.

---

## The loop we are closing

```
hunger decays ──▶ Decide says MoveTo → market ──▶ the adapter walks the settler
        ▲                                                    │
        │                                                    ▼
  hunger restored ◀── the settler arrives and is fed ◀── arrival outcome
```

Today every leg of that loop exists except the bottom-right corner:
needs decay, Decide produces MoveTo, the walk happens, the arrival
reports an outcome (memory + relationship) — and then **hunger stays at
zero and the settler walks again, forever**. The sim has no notion of
eating. This stone adds it, and the observable test is the cycle itself:
a settler who is hungry walks to the market, comes back fed, is not
hungry, and only goes again when hunger rebuilds — no script fired.

## What already exists (verified or built)

- **Needs decay** — the tick (core). Hunger at 0.0f → most urgent.
- **Decide → MoveTo** — the core reads the *most urgent need* and, if the
  mind remembers a food source (Trade-kind memory), targets it. (Note:
  `Decide` reads needs directly — goals are not consulted. The roadmap's
  "needs decay → *goal urgency* grows → MoveTo" is ahead of the code;
  goals today only matter to `ReportOutcome`'s goal-service.)
- **The walk** — the pinned travel package, verified.
- **Arrival outcomes** — `ArrivalOutcome(Species, feeder)`:
  Human → `{market, Trade, Partial}`; Child/Animal → `{feeder, Aid,
  Success}` ("fed, gives nothing in return"). Built, unverified.
- **Food sources** — the per-species resolver (owner else settlement).

## The one missing piece: the sim never eats

No code path restores a need. `Update` only decays; `ReportOutcome`
records memory and adjusts relationships (and serves goals, if any
exist — none are seeded). The trip therefore never satisfies hunger.

## The design (the two halves)

### 1. The adapter's write-through: arrival feeds (adapter-owned)

On arrival, the adapter restores the hunger need — the walk's payoff.
This is the documented translation rule in action ("a settler's
`Hunger` ↔ an ActorValue write through `RE::Actor`"; components ↔ game
data at the edge): *the mind ate* is a game fact, reported by the
adapter, not simulated by the core. Concretely: after `ReportArrival`,
`RestoreHunger` finds the `Needs` component's `Hunger` entry and sets it
back to 1.0 (full), logging the change. It applies to **every** arrival
— the settlement's stores feed arrivals, human and animal alike; the
per-species *outcome* still distinguishes commerce (human Trade/Partial
— nothing bought yet) from being fed (child/animal Aid/Success —
nothing given).

Game knowledge at the edge, sim law in the core — this stays honest with
the boundary. The alternative (the core restoring needs on goal service)
is sim meaning, not game fact, and is noted as an engine option below.

### 2. Goal seeding (adapter-owned) + the Feed-kind question (engine ask)

For the outcome's goal-service to mean anything, minds need goals. The
adapter seeds `Goals{ AcquireFood }` at translation for humans
(Prosper/Socialize later); a Trade Success will clear the ambition when
trading lands, a Partial halves it. Children and animals are seeded with
none — their loop closes on the feed alone. Note: `Decide` does not read
goals yet, so seeding is bookkeeping that becomes decision-relevant when
the engine wires goals into Decide.

**The animal gap:** the core's goal map feeds `AcquireFood` from Trade,
not Aid. A dog reported `Aid, Success` has its memory and disposition
grow, but its AcquireFood goal is untouched. Three ways to close it —
**decision point**:

- **(a) Engine: a `Feed` kind** (or map `Aid` → serves `AcquireFood`).
  Cleanest semantics — the sim understands "fed". Requires the engine
  tab (a small core change + its own stone).
- **(b) Adapter reports Trade-kind for animals.** The memory says Trade
  anyway (the lie); the outcome would grow *trust* with the feeder —
  "the dog trusts its human to feed it", defensible, zero core work. But
  it stains the "no trust ledger" property the species split promised.
- **(c) Leave goals unseeded for animals.** The dog's feed stays
  memory + disposition; its hunger is restored by the write-through; it
  has no ambition to serve. Simplest, and the dog's loop still closes
  observably (hungry → walk → fed → not hungry).

Recommendation: **(a) for the engine, (c) as the adapter's immediate
path** — the adapter ships the closed loop with the write-through and
no animal goals; the engine adds `Feed` when it can, and the adapter
then switches the animal's outcome kind and seeds its goal.

## What is NOT in this stone

- **World facts** (weather, market open/closed) and **tuning from
  Configuration** are separate roadmap items, already designed by the
  core handoff. The real test does not need them.
- **The actual trade interaction** (barter UI, caps) — the human's
  arrival stays `Partial` ("arrived, no trade yet") until a later stone
  makes trading real.

## Verification (in the build)

The log tells the loop:

```
settler 0001CA7D decides MoveTo -> 000250FE (0.16)       ← hungry
walk probe ... min closing ...                           ← walking
settler 0001CA7D arrived — no trade yet (Trade, Partial)  ← arrival outcome
settler 0001CA7D fed: Hunger 0.00 -> 1.00                ← THE payoff line
settler 0001CA7D decides Explore (0.04)                  ← not hungry — no walk
```

The no-script proof: the same mind, watched over minutes, cycles hungry
→ market → fed → idle, driven entirely by the need value — and the
moment the write-through is removed, the cycle collapses back to
walk-forever. That contrast *is* the test.
