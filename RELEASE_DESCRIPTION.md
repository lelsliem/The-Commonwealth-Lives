# 0.7.9 — The 0.7 Run: From Names to Babies

**Copy this into the GitHub release.**

---

## Title

**The Living Commonwealth v0.7.9 — The 0.7 Run: From Names to Babies**

## Tag

`0.7.9`

## Description

The 0.7 run is complete: settlers have **names**, they **talk**, they
**row**, they **trade with anyone**, they **fight** — with a real kick —
and their **children are born, grow, and can be seen**. The whole world
survives save/load. This release closes the run with a full codebase
audit (no bugs found) and the clean hand to **0.8.0 Illness & Medicine**.

> A settler goes to market because they are hungry — no script.

---

## Changelog — 0.7.0 → 0.7.9

### 0.7.0 — Identity & the Player Window (2026-08-11)

- **Names on the people themselves.** The workshop view, pip-boy and
  hover read *Mara Price*, not "Settler" — the name is written onto the
  actor and persists in the save. The game's own names always win
  (Sturges stays Sturges). Four curated name lists in the INI
  (`names.first.male/.female/.animal`, `names.last`); owned animals are
  named (the junkyard dog is *Bandit*), strays stay nameless, pet names
  are unique per world.
- **The feud is real.** A hungry arrival at a **closed market** is a
  slight — `the stall is shut — X went hungry and blames the keeper` —
  the settlement's echo agrees, rival and enemy bonds form, and feuds
  begin on their own: gossip through the settlement and mediation
  (`Titus Pratt cooled the feud between …`).
- **The player hears the world.** World events — bonds, feuds, births,
  deaths, market openings — become one-line news: throttled **HUD
  notifications** and a **settlement radio** that speaks the news as
  on-screen captions while you're near.

### 0.7.1 — Talk (2026-08-11)

- **Settlers speak.** Curated dialogue pools (the good greet/gossip/
  family, the bad trade/row, the ugly grief/fight/feud) say lines on
  trade, family meals, and slights. A seeded picker gives each mind one
  line per day — the same greeting all day, a new one tomorrow. The
  pools are INI data, so the words are yours without a recompile.

### 0.7.2 — Rows: the verbal altercation (2026-08-11)

- **Words before blows.** Rivals and enemies who cross paths at the
  same bench have words. Each remembers the other wronged them, the
  settlement hears the shouting, and the row can push a pair over the
  feud line. Physical escalation came later (0.7.5).

### 0.7.3 — Names for everyone (2026-08-12)

- **Every unnamed mind gets a name.** Role titles gain the person:
  "Provisioner Daisy", "Guard Cole". Two provisioners are no longer
  interchangeable — they're distinguishable in memory and in trade.
  Real names still win.

### 0.7.4 — Trade with anyone who sells (2026-08-12)

- **The hungry walk resolves to a person, not only the bench.** The
  vendor census admits traders, merchants, and provisioners as minds;
  a seller out-scores the bench while fresh. A vendor never shops at
  their own stall; the bench stays the fallback.

### 0.7.5 — Fights: the physical escalation (2026-08-12)

- **The feud turns physical.** Temper + chance book a fight — once per
  day, co-saved — the victim is shoved, and the exchange runs on beats:
  push → fall → get-up → retaliation → slink off. Fight lines ride the
  game's own subtitle queue as bottom-of-screen subtitles only when
  you're close enough to hear (`sim.subtitle.radius`). Species and kin
  splits keep the brawl human and families safe.

### 0.7.6 — The fight-feel pass: the kick is real (2026-08-13)

- **The kick finally plays for real.** The game's own push-kick idle
  refuses to play outside combat, so the mod ships its own 380-byte ESP
  (an unconditional clone of the proven recipe, byte-verified) — a real
  kick on the first shove AND the retaliation.
- **The fall tips instead of sliding** — the knock force drops to the
  tip-over zone, so the victim falls in place. Both actors must be on
  their feet before the next beat — no more double-collapses.

### 0.7.7 — Babies: the birth lifecycle (2026-08-13)

- **A child is now born, not just created.** Bonded human couples
  conceive (`sim.birth.chance`), carry a pregnancy through
  `sim.birth.gestation` sim-days, and the birth fires on the due day —
  the settlement hears it, the parents bond, the child is named and
  fed. After `sim.birth.childhood` days the child grows up and walks to
  market like any mind. Only humans conceive (no robot babies), and the
  whole journey survives save/load.

### 0.7.8 — Visible children (2026-08-13)

- **Grown children can be seen.** The adapter scans the game for child
  actors — from the Baby Sim mod or any source — and pairs a grown
  sim-only child to a real actor: it walks, trades, and bonds like any
  mind. All in code — no patch ESP. Without the mod, children stay
  sim-only (`sim.birth.visible`, default off).

### 0.7.9 — Bugs & polish (2026-08-13)

- **The clean hand to Illness.** Full codebase audit — every INI
  default vs the code, every comment, every doc — **no bugs found**.
  Everything consistent, 23/23 tests green.

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

- **Baby Sim - Babies That Grow Up** (Nexus 100934) — set
  `sim.birth.visible = 1` in the INI to pair grown children with real
  child actors.

## Note

If you load a 0.5/0.6/0.7-era save, it migrates forward cleanly. The
next package milestone is **0.8.0 — Illness & Medicine**.
