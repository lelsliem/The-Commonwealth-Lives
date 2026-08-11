# Illness & Medicine — "The Commonwealth Gets Sick"

**Milestone:** 0.8.x (after 0.8.0 Trade with anyone; the engine names it
the 0.8.3 Disease & Medicine pattern)
**Status:** PLAN (2026-08-11) — design doc written, no code.
**Related:** Trade.md (the stall exchange), Economy.md (cap pouches),
Identity.md (feuds, the conflict source), WorldFacts.md (weather gates),
ReleasePlan.md (the staged run to 1.0.0), the engine's endgame cuts
(Core `Docs/AdapterProject.md` — disease is the one accepted new
surface, hold-then-recover locked).

---

## The vision

Life in the wasteland has a price beyond hunger. Settlers get **sick**:
radstorms, bad food, wounds from fights, and the sick passing it to the
healthy. A sick mind rests more, slows down, and when it can, buys
medicine — or dies untreated. The Commonwealth is *frail*, and the
settlement feels it: the radio reports the ill, the market sells
medicine, and a bad season can empty a bench.

> The test plan is one sentence: *a radstorm day makes settlers ill;
> the sick rest and buy medicine; untreated sickness can end a mind.*

---

## The engine's design question — answered

The engine handoff (2026-08-11) names disease as the one current
candidate for new core surface, locks the **hold-then-recover** shape
(the sim keeps health at a reduced amount while ill; after the recovery
time elapses, health climbs at a rate until healed), and asks the
adapter to decide: **new NeedType or fact-plus-tick rule?**

**Answer: fact-plus-tick rule, adapter-owned.** A `Health` component at
the edge, not a `NeedType::Health` in the core. Reasons:

1. **Health's curve is not a need's curve.** The five needs decay toward
   0 and urgency grows; health *holds* while ill then *recovers*. Forcing
   that into `NeedType` would add a need that behaves unlike the others
   and confuses `Decide`'s urgency model. The engine is right that this
   is not facts-only — it is a *component*, with real value, held and
   recovered by the adapter's tick.
2. **Causes are all game facts at the edge.** Radstorms (the weather
   fact already rides memory), food quality, wounds from Combat
   outcomes, proximity to the sick — every vector is an edge read the
   adapter already owns.
3. **The adapter already owns stateful components.** `CapPouch`,
   `Name`, `SpeciesTag` — each rides the co-save with a stable name.
   `Health` slots into that proven pattern: additive, no record bump
   (like `name`).
4. **Effects flow through existing needs.** While ill, the adapter
   multiplies Fatigue decay — a sick mind *tires faster and rests
   more*, and Rest already exists (the sleep cycle). Recovery rides
   the Rest path plus medicine. No engine surface, no new goals.

This honors the engine's discipline: *no new engine surface unless a
pattern or a finding names a real need.* Disease is real, but it needs
a component at the edge, not a new need type in the core. The engine
keeps `Decide` vocabulary-free.

---

## The design

### The component

```
Health (adapter-owned, co-save stable name "health")
    float Value;            // 0..1 — 1.0 is healthy, 0.0 is dead
    Sickness Illness;       // empty when well
        u32 Kind;           // which illness (a label, for the log/news)
        float Severity;     // 0..1 — how bad, drives decay multiplier
        u64 ContractedDay;  // world day it began
        float Remaining;    // seconds of illness left (the hold window)
```

Additive like `name` — a pre-health save restores with `Value = 1.0`
and no illness; the component is back-filled on restore.

### The hold-then-recover curve

- **Well:** `Value = 1.0`, no decay. Nothing to do.
- **Ill:** on contraction, `Value` drops to a reduced amount
  (`sim.illness.hold` — default 0.4) and **holds** there while
  `Remaining` counts down. While ill, the adapter multiplies the mind's
  Fatigue decay by `sim.illness.fatigueMult` (default 2×) — the sick
  mind tires faster, `Decide` produces Rest more often, and the mind
  slows. That is the visible cost: *a sick settler rests.*
- **Recovering:** when `Remaining` reaches zero (or medicine is taken),
  the hold ends and `Value` climbs at `sim.illness.recovery` (default
  0.05/s) until 1.0. The Fatigue multiplier eases off as health
  returns (linearly).
- **Death:** if severity is high and health bottoms out untreated
  (`Value <= 0`), the mind can die — the existing death path books it
  (`arcs: X died of sickness`, gossip, the world keeps its books).
  Children are more fragile (`sim.illness.childMult` — default 2×
  severity growth).

### Causes (edge reads, all already owned)

| Vector | The read | Tunable |
|---|---|---|
| **Radstorm** | A remembered WeatherRadstorm day — exposure rolls per mind that day | `sim.illness.radstormChance` |
| **Bad food / water** | The market's food quality (a future Trade good); dirty meals | `sim.illness.foodChance` |
| **Wounds** | A Combat outcome (0.7.3 Fights) — a fight can infect | `sim.illness.woundChance` |
| **Contagion** | Proximity to a sick mind — the settlement Groups already exist; the sick echo outward | `sim.illness.contagionChance` |

### Medicine — the second good

The economy stone made trade physical (caps for a meal). Medicine is
the **second purchasable**: a sick mind at the market buys
`sim.illness.medicinePrice` caps worth of medicine instead of (or
besides) food — the trade stone resolves the same stall-keeper, the
caps leave the pouch, and the hold ends immediately (recovery starts
early). A broke sick mind rests instead — honest: sickness without
caps means time, not treatment.

Species split: **humans and children get sick and buy medicine;
animals get sick but never buy** (the species split — a sick dog rests
by its owner and is fed by the settlement; `Aid`, never `Trade`).

### The player window

The radio/news report the illness (0.7.0's channel): `X is ill`,
`X recovered`, `X died of sickness`. A bad radstorm becomes a story the
player hears. Reuses News/captions — nothing new to build.

---

## The seams (what stays where)

| Piece | Where it lives |
|---|---|
| `Health` component (value, hold, recover) | Adapter edge, co-save `"health"` |
| Illness contraction (radstorm, food, wounds, contagion) | Adapter edge (weather fact, Combat outcome, Groups) |
| Fatigue multiplier while ill | Adapter mutates the mind's Needs (like RestRecovery/VaryNeeds) |
| Medicine purchase | Adapter edge (Trade with the stall-keeper, caps leave the pouch) |
| Recovery curve | Adapter tick (like RestRecovery) |
| Death at the bottom | Existing death path — nothing new |
| `Decide`, `Needs`, goals | Engine — untouched |

## Honest notes

- **Health is not a need, and that is the point.** It does not decay
  toward urgency; it holds and recovers. The five needs stay the
  drives; health is the *price* of being in the wastes. The engine's
  locked shape is honored exactly — reduced amount while ill, recovery
  rate after the hold — just owned at the edge, where the causes live.
- **The visible cost is rest, not a stat.** A sick settler tires
  faster and rests more — the same wander/rest machinery that already
  works. No new animations, no new commands; the behavior *emerges*
  from the Fatigue multiplier.
- **Contagion is Groups, reused.** The settlement echo (0.6.0 stone
  09) already knows who shares a settlement; sickness rides the same
  membership. No new engine channel.
- **Medicine needs a stocked market to be real** — the trade stone
  sells it; a settlement with no stall-keeper has no medicine, so its
  sick suffer. That ties illness to the economy honestly.
- **Death must be rare and earned** — severity growth is slow, hold
  windows are long, and medicine is cheap. Sickness is a *season*,
  not a meat grinder.

## INI keys (draft)

```
sim.illness.hold = 0.4          ; health while ill
sim.illness.fatigueMult = 2.0   ; how much faster the sick tire
sim.illness.recovery = 0.05     ; health per second after the hold
sim.illness.duration = 120      ; the hold window, sim seconds
sim.illness.radstormChance = 0.3
sim.illness.foodChance = 0.1
sim.illness.woundChance = 0.15
sim.illness.contagionChance = 0.05
sim.illness.medicinePrice = 25  ; caps; 0 = no medicine anywhere
sim.illness.severityRate = 0.01 ; severity growth per second untreated
sim.illness.childMult = 2.0     ; children fall sicker, faster
```

## Verification

- **Contraction:** a radstorm day (or forced INI chance) makes settlers
  ill — `X is ill` logs and news.
- **Hold:** while ill, the log shows the Fatigue multiplier and the
  mind Resting more (Rest intent, the wander/rest lines).
- **Recovery:** after the hold, `X recovered`; Health climbs; the mind
  walks normally again.
- **Medicine:** a sick mind with caps buys at the stall — caps leave
  the pouch, recovery starts early; a broke sick mind rests instead.
- **Death:** untreated severe sickness books a death, gossips, and
  never restores.
- **Co-save:** save mid-illness, reload — the sick mind is still sick,
  `Value` and `Remaining` restore; a pre-health save restores healthy.
