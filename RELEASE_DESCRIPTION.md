# 0.7.9 — Bugs & Polish

**Copy this into the GitHub release.**

---

## Title

**The Living Commonwealth v0.7.9 — The 0.7 Run Complete**

## Tag

`0.7.9`

## Description

The 0.7 run is complete and polished: the world talks, rows, names
everyone, trades with anyone, fights — with a real kick — and children
are born, grow, and can be seen. This release closes the run with a
full codebase audit (no bugs found, everything consistent) and the
clean hand to **0.8.0 Illness & Medicine**.

### What the 0.7 run brought

- **They talk** — settlers speak on trade, family meals, and slights
  (curated dialogue pools, one line per mind per day).
- **They row** — rivals and enemies have words at the bench; the
  settlement hears the shouting, and the wrongs feed the feud.
- **Names for everyone** — the workshop view reads *Mara Price*, not
  "Settler"; role titles gain the person ("Provisioner Daisy");
  the game's own names always win.
- **Trade with anyone** — a hungry walk resolves to a person who
  sells: traders, merchants, provisioners — not only the bench.
- **Fights, with a real kick** — the feud turns physical: the victim
  takes a genuine kick animation (the mod's own 380-byte ESP —
  the game's push-kick idle refuses to play outside combat), falls,
  gets up, answers, and slinks off. The fall tips in place; the beats
  wait for everyone to be on their feet. Fight lines read as
  bottom-of-screen subtitles when you're close enough to hear.
- **Babies** — bonded human couples conceive, carry a pregnancy
  (`sim.birth.gestation` sim-days), and the birth fires on the due
  day: named, fed, and bonded. After `sim.birth.childhood` days the
  child grows into a walking mind. Only humans conceive.
- **Visible children** — grown children can be paired with real game
  child actors (works with the Baby Sim mod; `sim.birth.visible`).
  The pairing is in code — no patch ESP.
- **Everything survives save/load** — the co-save carries names,
  bonds, households, pouches, pregnancies, children, fights' day-
  gates, the Rng stream, and the dead and their bequeathed memories.

### Tuning without recompiling

Every rhythm is in the INI next to the DLL: market hours, hunger /
fatigue / safety / social / comfort decay, bond thresholds, fight
chance / temper / push / stagger, birth chance / gestation / childhood,
wander radius, news cooldown, radio radius, name lists, dialogue pools.

Full change log: [CHANGELOG.md](https://github.com/lelsliem/The-Commonwealth-Lives/blob/main/CHANGELOG.md)

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

The 0.7.x run is shipped local-first: the next package milestone is
**0.8.0 — Illness & Medicine**. If you load a 0.6/0.7-era save, it
migrates forward cleanly.
