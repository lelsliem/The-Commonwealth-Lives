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

### Stone 1 — The world keeps its books (lifecycle) — VERIFIED IN-GAME (2026-08-10)

- **Arrivals:** every loaded, sim-relevant actor that is not yet a mind
  becomes one on the tick's one-second census — new settlers, radio
  beacon recruits, and animals alike. The wake seed and the census share
  one `SeedMind` path.
- **Deaths:** the census reads the game's own markers (a killer handle,
  the corpse-cleanup timer, a deleted ref); a dead mind is removed — the
  walk session, the last-log key, the feeder line, and any stall it
  kept (the market re-derives its keeper) — and a **death fact**
  `{ the dead, Death, weight, day }` is pushed to every surviving mind
  (the core gained `InteractionKind::Death` — append-only ordinal).
- **Departures:** a known mind whose actor is alive but no longer in the
  settler faction is removed with a goodbye line. The bond-driven
  walk-out departure (the mind decides to leave) stays with Stone 2,
  where bonds define it — the bookkeeping is ready.
- **Verify (in-game, 2026-08-10):** a real kill books in two passes
  (`reads dead — first pass, not booked (confirming).` then `died — the
  world keeps its books.` a second later); the dead do not restore and
  never ghost-walk after save/reload; mid-session arrivals wake as new
  minds; and a fresh new game books **zero** false deaths while already-
  dead settlers from old saves are parked and never booked. The road
  there took three hardenings (ADR-0011): the 3D gate (streamed-in
  actors only), two-pass confirmation (a corpse reads dead twice, an
  artifact once), and the alive-first rule (a death is a transition — a
  mind must have read alive before it can be booked dead; the spawn
  burst after a big load reads the same actors dead on first sight for
  ~2 s, and only an alive reading un-parks them).

### Stone 2 — Bonds (relationships, good and bad) — VERIFIED IN-GAME (2026-08-11)

- Named bond states derived from the core's `Relationship`
  (Disposition + Trust), thresholds from the INI:
  friend +0.3, sweetheart +0.6, spouse +0.8, rival −0.3, enemy −0.6 —
  the adapter's own defaults when the config file names no lines.
- **Two channels, one derivation** (`Bonds.h`, pure + tested): the
  `RelationshipChangedEvent` on the adapter's `EventBus` (Request A —
  the core crosses a line mid-mutation; the adapter re-derives that pair
  instantly) and the 1-second `ReconcileBonds` pass (drift is quiet in
  the core — a bond cooling below its line is a dissolve, not an event,
  so only the pass sees it). Both feed `Bonds::ApplyPair`, so they can
  never disagree.
- **Mutual and sticky.** The pair's shared disposition is the *minimum*
  of the two directions — both must feel it (one-sided warmth is not
  yet a bond). Formation is immediate at the line; dissolution waits
  until the pair falls halfway back (friend +0.3 dissolves below +0.15)
  — a fresh bond must not vanish to the next drift tick. Same-family
  downgrades are sticky too; a family flip (friends turned rivals) is
  news.
- Bonds are **persisted** (co-save v5: form-id pair, kind, since-Day) — a
  spouse is still a spouse after reload; a v4 save loads with no bonds
  and the pass re-derives them from the restored relationships.
- Log lines: `settler X and settler Y became friends.` /
  `settler X is feuding with settler Y.` — with species labels
  (`child`, `animal`) so a dog bonding with its feeder speaks plainly.
  Formation, upgrade, family flip, and dissolution each get their line.
- **Verify:** watch two settlers socialize into a bond; reload; the bond
  stands (the `bonds: N bonds restored from the co-save.` line at
  restore, and the bond lines on formation).
- **Two in-game discoveries shaped the wiring (2026-08-11):** (1) the
  core's `Trade` outcome builds *trust*, never disposition — only Aid /
  Social warm feelings — so a buyer's disposition toward the keeper
  never grew from trading at all. The buyer's half is now a
  `Remember({keeper, Social})` alongside the trade: a meal at the bench
  is company, the courtship's raw material, and it publishes the
  crossing on the bus (instant bond log). (2) The core's drift default
  (0.05/s, half-life ~14 s) erased any warmth between meals — the
  adapter's world runs the same slow clock the INI sets
  (`sim.drift.rate = 0.0002`, half-life ~1 h, injected when the config
  names none). Four shared meals cross the friend line; meals every
  ~10 s (the fast demo rhythm) bond in under a minute.

### Stone 3 — Households — BUILT + TESTED (2026-08-11, 12/12 suites), in-game verification pending

- A household is the deepest bond made concrete (`Households.h`, pure +
  tested): the moment a pair's bond becomes **Spouse** (the +0.8 mutual
  line), their two pouches merge into **one shared wallet** (on the
  deterministic lower-id holder); when the marriage dissolves, the wallet
  splits (holder keeps the remainder, the other takes half). The reaction
  rides the bond-change handler — whichever channel crossed the line
  (event or 1-second pass) forms the household exactly once.
- **One bench:** a spouse arriving at the market where their partner keeps
  the stall is at the *family bench* — fed, no exchange (the household
  pouch does not pay itself). The keeper's spouse is the one customer who
  never pays.
- **The shared wallet round-trips** — `PouchOf` resolves a married
  member's pouch to the spouse's when they don't physically hold it, on
  both sides of the bench, and the invariant *one pouch per married pair,
  one per unmarried human* is enforced silently (`Enforce`) on restore
  and as defensive repair each pass. Households are **derived state** —
  the marriage rides the bond map (co-save v5) and the wallet rides the
  `CapPouch` component — so no record bump was needed (ADR-0013). A dead
  spouse's pouch passes to the widow(er), not the void.
- **Deferred (documented, not built):** walking to market *together* (no
  path coordination) — the shared-bench behavior is the observable half,
  and it waits for the companionship intents of a later stone. The same
  *bed* was the rest intent's work — delivered below as the sleep cycle.
- **Verify:** a married pair trades as one wallet — watch for
  `households: settler X and settler Y are now a household — one pouch,
  one bench.` then a trade line reading `(household; N left, M now)`;
  save, reload, the shared pouch is still one.

### Stone 3.5 — The sleep cycle — BUILT + TESTED (2026-08-11, 13/13 suites)

- The 24-hour market test exposed the gap: **a fed mind parks in Rest
  forever.** The engine's need loop only decays; nothing ever restored
  Fatigue. Only Hunger was restored (the meal), so the moment a mind ate,
  its drained Fatigue became the most urgent need → Rest → and Rest was
  a table slot that did nothing. Seven trades, seven different buyers,
  zero repeats — no pair ever shared a second meal, so no bond could
  form (the household test's dead end).
- The fix (`Behaviour.h` `RestRecovery`, wired in `Adapter::Tick` before
  `Update`): a mind whose last intent was Rest recovers Fatigue at
  `sim.rest.recovery` per second (default 0.2/s — a full nap in ~5 s).
  The next `Update` decides from the rested mind: hunger is most urgent
  again → `MoveTo` → walk to market → trade → meal → Rest → recover →
  repeat. The same pair finally meets repeatedly, and bonds can form.
- **The loop closes** (SleepCycleTest): a fed mind with drained Fatigue
  decides Rest; after a nap it decides MoveTo to the remembered market.
  Without the recovery it parks forever — the test pins the exact bug
  the 24h-market run exposed.
- **Verify:** relaunch the fast demo (24h market + `sim.hunger.decay =
  0.1`) — customers return to the same bench for second meals; the first
  `bonds: settler X and settler Y became friends.` lands within minutes;
  a marriage (and `households: … now a household`) follows.

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
