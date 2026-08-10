# Life & Emergent Quests — 0.6.0 "The Commonwealth Remembers"

**Status:** plan (2026-08-10) — written before any 0.6.0 code; stones build
in order below. The 0.5.0 boundary contract is the foundation; what the
adapter asks of the core is itemized at the bottom and handed over to the
engine in the engine's `Docs/AdapterProject.md`.

## The vision

A settler isn't a mind with a hunger bar. A settler is **born**, **lives**,
**makes friends and enemies**, and **dies** — and the settlement remembers.
Quests happen because life happens: a feud starts over a failed trade, a
widow grieves and then decides between vengeance and comfort, a couple
falls in love at the same bench, a hungry town turns on itself. No quest
scripts. The quest is the behaviour — visible in the world and in the log.

> The test plan is one sentence: *the Commonwealth remembers.*

Kill a settler, save, reload: the survivor still grieves, the feud still
stands, the gossip is still town news, and the child still knows its
parents.

## What life entails (the pillars)

| Pillar | The question | In the sim |
|--------|--------------|------------|
| **Born** | Where do minds come from? | Arrivals (new workshop settlers become minds), then births (a couple has a child — experimental, INI-gated). |
| **Live** | What is a day? | Already real: wake, hunger, market, trade, rest, social. 0.6.0 adds work rhythms and household life. |
| **Die** | What ends a mind? | Combat death and departure. The mind is destroyed; the memory of them lives on in others. |
| **Relate** | How do minds feel? | Bonds — named states (friend, sweetheart, spouse, rival, enemy) built on the core's Disposition/Trust, persisted in the co-save. |
| **Quest** | Why do things happen? | Arcs — adapter-side state machines that steer intents when state (needs, bonds, memory, weather, market) crosses a line. |

## The adapter's 0.6.0 stones

Each stone is a buildable unit with an in-game verification. They build in
order; each one leaves the world strictly more alive.

### Stone 1 — The world keeps its books (lifecycle)

- **Arrivals:** new workshop settlers become minds mid-session (radio
  beacon recruits, new settlers) — not just at world wake.
- **Deaths:** hook the actor death event → destroy the entity → write the
  death fact to the settlement's minds (`{death, who, day}`) → grief.
- **Departures:** a settler whose bonds are all negative and needs unmet
  walks out of the settlement and despawns — a removal with a goodbye.
- **Verify:** kill a settler in-game — the death is town news, no ghost
  walks, no resurrected minds on reload.

### Stone 2 — Bonds (relationships, good and bad)

- Named bond states derived from the core's `Relationship`
  (Disposition + Trust) plus interaction history, thresholds from the INI:
  friend +0.3, sweetheart +0.6, spouse +0.8 (mutual), rival −0.3,
  enemy −0.6.
- Bonds are **persisted** (co-save v5: form-id pair, kind, since-Day) — a
  spouse is still a spouse after reload.
- Log lines: `X and Y became friends.` / `X is feuding with Y.`
- **Verify:** watch two settlers socialize into a bond; reload; the bond
  stands.

### Stone 3 — Households

- Couples share: one pouch, one stall preference, the same bench, the same
  bed (rest intent pairs).
- The couple walks to market together; the shared pouch round-trips.
- **Verify:** a married pair trades as one wallet.

### Stone 4 — Gossip

- Word spreads: a bond event, a death, a feud writes a fact to every mind
  within a gossip radius (INI) of the settlement — the settlement knows
  its own news; strangers and fresh arrivals don't.
- **Verify:** kill a settler — a bystander across town mentions the death;
  a newly arrived settler doesn't know.

### Stone 5 — Emergent arcs (the quests)

Arcs are adapter-side state machines (stage + participants + memory keys)
that steer intents. Five arcs ship:

1. **The Feud** — repeated failed trades / slights drop disposition below
   −0.6; the pair refuses to trade or socialize with each other; gossip
   spreads it; a neutral settler may try to mediate.
2. **The Grief** — a spouse or child dies; the survivor's comfort
   plummets, they stop trading for a day, then fork on the evidence: hunt
   the killer if they trust the killer was at fault, else seek comfort in
   new bonds.
3. **The Courtship** — repeated socializing at the same place raises
   disposition; +0.6 → sweetheart; mutual +0.8 → spouse (hands off to the
   household stone).
4. **The Departure** — all bonds negative and needs unmet → the settler
   leaves; the settlement remembers them (a goodbye fact, then the mind
   despawns).
5. **The Famine** — the market can't feed the settlement (shortfall from
   the sim); hunger spikes, dispositions fray, some settlers consider
   leaving, and the market stays open late while the famine lasts.

- **Verify:** the logs show each arc's lifecycle — start, play, resolve —
  with no scripts: every arc is needs, bonds, and memory driving intents.

### Stone 6 — Birth (experimental, INI-gated)

- A spouse pair can have a child: a child actor spawns (HumanChildRace), a
  mind is born at Day 0.
- The child is fed by the settlement (the existing species rule), bonds
  with its parents, and — in a later stone — grows up.
- INI gate: `sim.births = off` in the beta; on is a stress test.
- **Verify:** the log records a birth; the child mind exists, protected,
  fed, and bonded.

### Co-save v5

Bonds (form-id pairs + kind + since-Day), households, arcs in progress,
gossip day-stamps, the birth registry. Version-gated exactly like v3/v4 —
old saves load, new sections are additive.

### Tuning (INI additions)

Bond thresholds (friend / sweetheart / spouse / rival / enemy), gossip
radius, grief duration, famine threshold, population cap, births on/off.
Every new number is a `sim.*` or `bond.*` key the adapter reads — same
file, same rules (unknown keys ignored, broken lines keep defaults).

## What the adapter wants from the engine

**The honest headline: the 0.5.0 boundary contract suffices for nearly all
of this.** The adapter builds lifecycle, bonds, arcs, gossip, and births
on the existing surface — `CreateEntity` / `DestroyEntity` / `Remember` /
`Update`, the `Relationships`, `Goals`, `Memory` components, the `Outcome`
channel, observation events, `WorldTime`, and the seeded `Rng`.

One stone would align the sim with the core's push-don't-poll philosophy:

### Request A — `RelationshipChanged` observation event (core stone 08 candidate)

The core already owns disposition/trust dynamics (drift, outcome shifts).
Let the core emit an observation event when a relationship crosses a
configurable threshold — so bond formation is an **event** the adapter
(and, through gossip, other minds) react to, instead of the adapter
polling `Relationships` every tick. The event carries:

```
subject (EntityId) · other (EntityId) · disposition · trust
threshold crossed (name from tuning) · world day
```

Thresholds come from tuning (`sim.bond.threshold.*`), so the core stays
world-agnostic — it knows nothing about "marriage", only that a
relationship crossed a line the world configured.

### Request B — optional, deferred: `GoalType` growth

If the core wants arcs first-class (e.g. `FindPartner`, `Avenge`, `Lead`)
rather than the adapter chaining the four generic intents
(`AcquireFood`, `ReachSafety`, `Socialize`, `Prosper`), add them. **Not
required for 0.6.0** — the adapter chains the existing four.

### What the adapter will NOT ask for

- No population logic, aging rules, or quest grammar in the core — those
  are world-specific and the adapter's job by design.
- No game knowledge of any kind — the core stays a stateless, world-free
  tick, exactly as the boundary contract promises.

## The 0.6.0 test plan

1. Two settlers become friends; one slights the other; the feud arc
   starts; gossip spreads it; a neutral settler mediates.
2. A couple courtships at the bench, marries, and their pouch is shared —
   save, reload, the bond and pouch survive.
3. Kill the spouse: the survivor grieves (comfort plummets, trading
   stops), then forks — vengeance or comfort — on what they trust.
4. Let the settlement starve: the famine arc opens the market late,
   dispositions fray, someone leaves.
5. A birth (with the gate on): a child mind is born, protected, fed,
   bonded to its parents, and its Day starts at 0.

Everything observable in the log; every behaviour driven by needs, bonds,
memory, and the seeded RNG — never by a script.

## Beyond 0.6.0 — the author's ideas (2026-08-10)

Three pillars the author wants after the life core is real. They live
here as sketches; each becomes a designed stone when its turn comes.

### Identity — settlers get names

The sim talks about people by formid hex. Names make it human: logs,
gossip, bonds, and the radio all name people — "Marcy and Jun became
friends", "a child was born to the Lees", "Sturges is feuding with the
provisioner".

- FO4's generic settlers are mostly named "Settler" in-game, so the
  adapter assigns **procedural Commonwealth names** at mind-seed time
  from a name list (INI-selectable), persisted in the co-save (a name
  registry keyed by form id) — a settler is "Sturges" in Sanctuary and
  still "Sturges" after reload and after a fast-travel despawn.
- **Verify:** the log greets the world by name; save, reload, same names;
  a name collision never happens (the registry dedupes).

### Agency — settlers act on the world: build, move items, destroy

Today settlers walk and trade. This pillar gives them **hands**: their
intents produce real world changes, not just movement.

- **Move items** — a hauler transfers stock between containers
  (workshop → stall, stall → workshop) using the game's item APIs; the
  stall actually sells what was stocked.
- **Build** — a builder places objects at the settlement through the
  workshop placement path (position, rotation, resources); the bench
  grows a stall, a fence, a chair the sim built.
- **Destroy** — a destroyer clears clutter (disable refs); decay, raids,
  and spite take things apart.
- **Engine ask (tentative):** the core's four `GoalType`s don't cover
  labour. The adapter can map `Prosper` → labor intents with the
  existing surface (no core change needed), but if the core wants
  labour first-class — `Construct`, `Haul`, `Demolish` — that's a
  growth of Request B.
- **Verify:** a stocked stall sells stocked goods; a builder's fence
  survives reload; a destroyer's clearing stays cleared.

### The Player Window — a radio, and MCM

The world is alive even when the player isn't watching. Give the player
ears and hands.

- **The radio** — a news channel that synthesizes world events into
  plain speech for the player, in three steps:
  1. **The news feed (first):** the adapter turns events (bonds, births,
     deaths, feuds, famine, market openings) into one-line news, pushed
     as in-game notifications and written to a news log the player can
     open — "A feud has begun in Sanctuary", "A child was born at
     Tenpines".
  2. **The radio object:** a transceiver in settlements (an in-game
     radio marker) plays the news as on-screen captions while the
     player is near — the settlement tells its own story.
  3. **Real audio (deferred):** voice lines need assets; the radio
     becomes a real station only when the author ships or licenses
     audio. Not a blocker for 1 and 2.
- **MCM + Settings Manager** — a Mod Configuration Menu page exposing
  the tuning keys (hunger rhythm, market hours, bond thresholds, births
  gate, gossip radius, population cap) so players tune the world
  in-game; the INI stays the source of truth, MCM reads and writes it
  (or an override file), with a restore-defaults button. Requires the
  MCM mod as a soft dependency and a small Papyrus surface on the
  adapter side.
- **Verify:** news appears for a feud without the player nearby; MCM
  changes survive reload; defaults restore cleanly.
