# 0.8.6 — The 0.8 Run: From Sickness to the Field

**Copy this into the GitHub release.**

---

## Title

**The Living Commonwealth v0.8.6 — The 0.8 Run: From Sickness to the Field**

## Tag

`0.8.6`

## Description

The 0.8 run is complete. The Commonwealth gets **sick** — and the
market becomes the cure. Medicine is a **stocked shelf** that can sell
out, and a **family cares for its own**. The dead are **buried** by the
settlement that mourned them. Settlers **talk to each other**
unprompted. The player **tunes the whole world in-game** through MCM —
no more INI hunting. Robots are finally **robots**. Every mind can
**earn caps**. And the whole sim proved it **scales in the field**:
~620 live minds on a real save, the sim's entire share roughly half a
frame, the game smooth throughout.

> A settler goes to market because they are hungry — no script.

---

## Changelog — 0.8.0 → 0.8.6c

### 0.8.0 — Illness & Medicine (2026-08-13)

- **Every settler carries health.** A radstorm day, a bad meal, a wound
  from a fight, or the sick passing it to the healthy — four ways to
  fall ill. A sick mind holds at reduced health while the sickness
  runs, tires faster and rests more, and **coughs** (the game's own
  coughing idle) so you can hear the outbreak. Untreated, a severe
  illness can end a mind — rare, earned, and remembered.
- **The market cures.** A sick settler with 25 caps buys medicine at
  the stall — the hold ends, recovery begins, the seller's pouch
  grows. Broke sick settlers rest instead and ride it out honestly.
  Verified in-game on a natural radstorm day: **73 medicine buys, 0
  deaths.**

### 0.8.1 — The illness field pass (2026-08-13)

- **Sickness takes the body.** The illness death now actually kills the
  game actor — the corpse appears (0.8.2 buries it later) and the dead
  stay dead, no ghost re-entry.
- **Children survive the cold.** Childhood illness retuned — a
  contagion-child runs the hold and recovers; death stays rare and
  earned.
- **An outbreak is one story, not a wall of sound.** A global cough
  gate (one cough per 4 s, on top of each mind's own) kills the
  cacophony; illness news is burst-paced (at most 4 new names per 10 s
  window) so the radio stays readable.
- **The co-save audit.** Illness and pregnancy now truly persist —
  a mid-hold sickness and an in-progress pregnancy restore exactly
  across save/load (the audit caught components that claimed to ride
  the record but never registered).

### 0.8.2 — The burial (2026-08-13, verified 2026-08-14)

- **The settlement lays its own to rest.** A corpse no longer stays in
  the cell forever: after the mourning window (`sim.death.burialDays`,
  default 3), the sweep disables the body, logs `the settlement laid X
  to rest`, and the settlement hears it. The burial book rides the
  co-save, so a death whose window expires while the game is away is
  still buried on the next load.

### 0.8.3 — The sick household (2026-08-14)

- **Medicine is a stocked shelf.** Each market's doses per day ride the
  co-save; a sold-out stall stays sold out across save/load until the
  next market day, and a sick mind at an empty shelf rests instead of
  buying.
- **A family cares for its own.** The trade block's dose logic is one
  shared path used twice: the sick dose themselves, then a well buyer
  buys for the household — the spouse first, then a sick child (who
  has no walk of its own). The shared wallet pays; the news reads
  `X bought medicine for Y — a family cares.`

### 0.8.4 — Random interactions (2026-08-14)

- **Settlers talk to each other.** A loaded human mind finds its
  nearest non-walking neighbour inside `sim.interact.radius` (400 u),
  rolls `sim.interact.chance`, and speaks unprompted — no hunger drive,
  no market. The pool follows the bond: family lines for family, a
  quiet row for rivals, greet/gossip for strangers. Speech is local:
  log + subtitle only when the player is within hearing
  (`sim.subtitle.radius`) — the radio never carries small talk.
- The trial passed in-game: 186 genuinely-near speakers, bond-aware
  pools, walking minds never interrupted. The author judged it
  *"added life"*.

### 0.8.5 — MCM + Settings Manager (2026-08-14)

- **The player tunes the world in-game.** A full MCM page (5 pages /
  53 controls: Life, Interactions, Relationships, Illness, Birth &
  Fights) binds every player-facing knob. A slider change lands in the
  sim within a second — hot-applied, no rebuild, survives a restart.
  No MCM installed? The INI alone still rules. MCM stays a soft
  dependency.

### 0.8.6a — The Audit (2026-08-14)

- **The honest pass.** Every cut candidate re-tested against what
  shipped: MCM was rescinded to KEEP, and the audit added the two real
  gaps to 0.8.6b — the earn-caps economy and log hygiene.

### 0.8.6b — Redefine & loose ends (2026-08-14)

- **Robots are robots.** A field find (Buddy the Mr. Handy caught the
  flu) became the species: robots talk but have no biological needs —
  no hunger, no fatigue, never ill, no pouch, no stipend, never walk
  to market, never romance. Befriends and feuds like anyone.
- **Everyone can earn caps.** A daily settlement stipend
  (`sim.economy.stipend`, default-off — the player opts in via the
  MCM "Daily stipend" slider) closes the broke-gap that made sickness
  a death sentence for the poor. The household shares the wage.
- **Who pays the wage — settled.** `sim.economy.stipend.source`
  (settlement minted | player — the bill comes out of the player's
  caps) and `sim.economy.stipend.requireOwned` (only player-owned
  settlements pay). Both field-failed, were reverted to the working
  minted stipend, then each was unbenched: player-pays fixed (the
  count's sign was the bug) and ownership fixed (see 0.8.6c).
- **MCM restructured** — a dedicated Economy page (Market, Medicine,
  Stipend); Illness is sickness mechanics only.

### 0.8.6c — Scale in the field (2026-08-14)

- **The hard gate, measured and passed.** The adapter feeds the
  engine's `TickReport`; field-verified on a 646-mind save — four
  consecutive reports at 618–619 live minds: worst frame 4.13–8.17 ms
  against the 16.6 ms budget, game smooth throughout. The sim's whole
  share is roughly half a frame.
- **The ownership read, unbenched for real.** `requireOwned` now reads
  the game's own `WorkshopPlayerOwnership` actor value (form 0x33C —
  the exact AV `SetOwnedByPlayer` writes), re-armed per world so the
  loaded save's restored state is read instead of the menu-world's
  defaults. Field-verified: **28 of 28 workshops owned**, matching the
  player's map.
- **Bug-hunt cleanup** — a cough idle null-guard (a crash waiting to
  happen), the quest-prop debug dump dropped from the hot path, and
  stale comments reconciled. Harness **27/27 suites green.**

---

## Requirements

- Fallout 4 **1.11.221 (Next-Gen)**
- **F4SE** (Next-Gen, 0.7.8)
- **Address Library**

## Install

1. Copy `F4SE/` from the archive into your `Fallout 4/Data/` folder — the
   plugin lands at `Data\F4SE\Plugins\TheLivingCommonwealth.dll`.
2. Copy `data/TheLivingCommonwealthAnims.esp` into your `Data/` folder
   and **enable it in your load order** (required for the fight kick;
   without it fights fall back to a stagger — nothing crashes).
3. The INI is created with sane defaults on first run — every number is
   tunable.

## Optional

- **MCM** — the in-game settings page (5 pages / 53 controls). Without
  it the INI alone rules.
- **Baby Sim - Babies That Grow Up** (Nexus 100934) — set
  `sim.birth.visible = 1` in the INI to pair grown children with real
  child actors.

## Note

If you load a 0.5/0.6/0.7/0.8-era save, it migrates forward cleanly.
The next package milestone is the **0.9.x run to the Nexus beta**
(dialog depth, timings & weights, animations, and the release).
