# 0.7.0 — Identity & the Player Window

**Copy this into the GitHub release.**

---

## Title

**The Living Commonwealth v0.7.0 — Identity & the Player Window**

## Tag

`0.7.0`

## Description

The world is alive even when the player isn't watching. Settlers have
**names**, relationships can go **bad**, and the player **hears** what's
happening.

**Names on the people themselves** — the workshop view, pip-boy and hover
read *Mara Price*, not "Settler", written onto the actor and persisted in
the save. The game's own names always win (Sturges stays Sturges). The
author curates the name pools in the INI; owned animals are named (the
junkyard dog is *Bandit*), strays stay nameless, pet names are unique per
world. Every log line, bond, death, birth, and news item speaks in names.

**The feud is real** — a hungry arrival at a closed market is a slight:
`the stall is shut — X went hungry and blames the keeper`. Rival and
enemy bonds form, gossip spreads it through the settlement, and mediation
pulls the settlement apart again: *"Titus Pratt cooled the feud between
…"*. The engine's `sim.hunger.desperate` gate means a critically hungry
mind walks to a shut market anyway — slights, feuds, and famine stories
all happen on their own.

**The player hears the world** — world events become one-line news:
throttled HUD notifications ("X and Y became friends") and a settlement
radio (build it in the workshop) that speaks the news as on-screen
captions while you're near.

**Everything survives save/load** — co-save record v6: names, bonds,
stall-keepers, cap pouches, the Rng stream, memory world-days, the dead
and their bequeathed memories, and sim-only children.

**Tuning without recompiling** — every rhythm is in the INI: market
hours, hunger/fatigue/safety/social/comfort decay, bond thresholds, the
slight's temper line, news cooldown, radio radius and caption cadence,
name lists.

Full change log: [CHANGELOG.md](https://github.com/lelsliem/The-Commonwealth-Lives/blob/main/CHANGELOG.md)

---

## Requirements

- Fallout 4 **1.11.221 (Next-Gen)**
- **F4SE** (Next-Gen, 0.7.8)
- **Address Library**

## Install

1. Copy `F4SE/` from the archive into your `Fallout 4/Data/` folder — the
   plugin lands at `Data\F4SE\Plugins\TheLivingCommonwealth.dll`.
2. The INI is created with sane defaults on first run — every number is
   tunable.
