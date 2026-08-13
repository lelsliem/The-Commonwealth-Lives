# The Living Commonwealth — Release Notes

**Fallout 4 adapter for the Living Commonwealth Engine (LCE)** — an F4SE
plugin that makes the Commonwealth *live*.

> A settler goes to market because they are hungry — no script.

The settlers aren't on quest scripts. They are **hungry**, they
**remember** where to trade, they walk to **their own settlement's
market**, and the exchange is physical: caps change hands, the
stall-keeper's purse grows, trust is earned. The market has **hours** —
it closes at night and nobody walks to a closed bench — and the day's
**weather** is remembered. The game does nothing but show the result.

In-game verified end to end: hungry settlers decide `MoveTo`, walk to the
bench, arrive, trade, and the whole world survives save/load.

---

## 0.8.0 — Illness & Medicine (2026-08-13)

The Commonwealth gets sick — and the market becomes the cure.

Every settler now carries **health**. A radstorm day, a bad meal, a
wound from a fight, or the sick passing it to the healthy — four ways
to fall ill. A sick mind holds at a reduced health while the sickness
runs, tires faster and rests more, and **coughs** (the game's own
coughing idle) so you can hear the outbreak. Untreated, a severe
illness can end a mind — but death is rare and earned, not a meat
grinder.

**The market cures.** A sick settler with 25 caps buys medicine at the
stall — the hold ends, recovery begins, and the seller's pouch grows.
Broke sick settlers rest instead and ride out the illness honestly.
The whole loop is verified in-game on a natural radstorm day: 73
medicine buys, 0 deaths.

Everything is tunable from the INI (`sim.illness.*`): contraction
chances per vector, the hold window, recovery speed, severity growth,
the medicine price, the cough cadence.

## 0.7.9 — Bugs & Polish (2026-08-13)

The clean run into Illness. The full codebase audit — every INI default
vs the code, every comment, every doc — found **no bugs**. Everything
consistent, 23/23 tests green. Version 0.7.9.

The 0.7.x run is complete: talk, rows, names for everyone, trade with
anyone, fights, the real kick, babies, visible children, and polish.
Next: 0.8.0 Illness & Medicine.

## 0.7.8 — Visible Children (2026-08-13)

Born children can now be **seen**. When a sim-only child grows up
(`sim.birth.childhood` days), the adapter scans the game for child
actors — from the Baby Sim mod or any source — and pairs the child to
a real actor: it walks, trades, and bonds like any mind. The pairing is
entirely in code — no patch ESP, no load-order fragility. Without the
mod nothing changes (`sim.birth.visible`, default off): children stay
sim-only, as before. The Baby Sim mod itself is *usable now*; shipping
it as a requirement waits on the author's permission.

## 0.7.7 — Babies: The Birth Lifecycle (2026-08-13)

A child is now *born*, not just created. Bonded human couples conceive
(`sim.birth.chance`), carry a pregnancy through `sim.birth.gestation`
sim-days, and the birth fires on the due day — the settlement hears it,
the parents bond, the child is named and fed. After `sim.birth.childhood`
sim-days the child grows up: its needs normalize and it walks to market
like any mind. Only humans conceive (no robot babies), the day-261
crash is fixed, and the whole journey survives save/load.

## 0.7.6 — The Fight-Feel Pass: The Kick Is Real (2026-08-13)

Two presentation bugs closed. **The kick finally plays for real** — the
game's own push-kick idle refuses to play outside combat, so the mod
ships its own 380-byte ESP (unconditional clone of the proven recipe)
and the adapter plays it on the attacker: a real kick on the first
shove AND the retaliation. **The fall tips instead of sliding** — the
knock force drops to the tip-over zone so the victim falls in place;
the paired push is the shove. And both actors must be on their feet
before the next beat — no more double-collapses. The experiment adding
body-slam and push animations was reverted; the kick alone reads right.

---

## 0.7.0 — Identity & the Player Window (2026-08-11)

The world is alive even when the player isn't watching — settlers have
**names**, relationships can go **bad**, and the player **hears** what's
happening.

- **Names on the people themselves.** The workshop view, pip-boy and
  hover read *Mara Price*, not "Settler" — the name is written onto the
  actor (the same mechanism as the console's `SetDisplayName`) and
  persists in the save. The game's own names always win (Sturges stays
  Sturges). The author curates four name lists in the INI
  (`names.first.male/.female/.animal`, `names.last`); owned animals are
  named (the junkyard dog is *Bandit*), strays stay nameless, and pet
  names are unique per world. Every log line, bond, death, birth, and
  news item speaks in names.
- **The feud is real.** The world has a conflict source: a hungry
  arrival at a **closed market** is a slight — `the stall is shut — X
  went hungry and blames the keeper` — the settlement's echo agrees,
  rival and enemy bonds form, and the 0.6.0 feud arc finally begins on
  its own: `X is feuding with Y`, gossip through the settlement, and
  mediation (`Titus Pratt cooled the feud between …`). The engine's
  `sim.hunger.desperate` gate (INI-tunable) means a critically hungry
  mind will walk to a shut market anyway — slights, feuds, and famine
  stories are all possible.
- **The player hears the world.** World events — bonds, feuds, births,
  deaths, market openings — become one-line news: throttled **HUD
  notifications** ("X and Y became friends") and a **settlement radio**
  (build it in the workshop) that speaks the news as on-screen captions
  while you're near. Real audio is planned after 0.9.0.
- **The whole world round-trips the co-save** (record **v6**): names,
  bond maps, stall-keepers, cap pouches, the Rng stream, memory world-
  days, and the legacy section (a death bequeaths its memories and
  leaves its name behind; a child inherits the parents' memories of the
  people).
- **Tuning without recompiling** — every rhythm is in the INI: market
  hours, hunger/fatigue/safety/social/comfort decay, bond thresholds,
  the slight's temper line, news cooldown, radio radius and caption
  cadence, name lists.

---

## 0.6.0 — Life & Emergent Quests (2026-08-11)

Settlers are born, live, and die; they make friends and enemies; and
quests happen because life happens — no scripts.

- **The world keeps its books.** Arrivals wake mid-session, deaths and
  departures leave it, and every survivor remembers who is gone
  (`gossip: 643 minds remember settler X is gone`). A real kill books
  confirming; the dead never restore and never ghost-walk.
- **Bonds from how people treat each other.** Friends, sweethearts, and
  spouses emerge from shared meals and survive save/load — the buyer's
  half of a trade warms the keeper, and feelings cool slowly (the
  living drift clock), so bonds accumulate.
- **Households.** A married couple shares one pouch, one stall, one
  bench, one bed — the family bench feeds the spouse for free.
- **The sleep cycle.** A fed mind rests, recovers its Fatigue
  (`sim.rest.recovery`), and walks again — so the same settlers keep
  meeting and their bonds deepen. Settlers **mill around** between
  meals (Rest/Explore command a bounded wander to a real nearby
  reference, furniture preferred).
- **Grief is real.** A widowed settler drains its Social seeking
  company — `arcs: settler X grieves for Y`.
- **Children.** A spouse household's sim-only child — fed, bonded, and
  living in the co-save like any mind (`sim.birth.enabled`).

---

## 0.5.0-beta — The Living World (2026-08-10)

### What's in this release

- **0.1 — The heartbeat.** The plugin loads and breathes.
- **0.2 — The translation.** Settler-faction actors become minds.
- **0.3 — The intent executor.** The sim ticks every frame and settlers walk.
- **0.4 — The co-save.** The world rides inside the save file (record v4:
  entities, the seeded RNG stream, who runs each market's stall, and each
  memory's world day — the timestamp survives save/load).
- **0.5 — The living world.**
  - Species split: children and animals don't barter — a dog gets fed and
    gives nothing in return.
  - World facts: the market's hours push to memory — closed at night,
    nobody walks to a closed bench.
  - Weather memory events: settlers remember the day's sky.
  - Per-settlement markets: a persistent-cell census (FO4 never fills the
    REFR form array) gives every settlement its own bench and its own
    stall-keeper.
  - The trade stone: a stall-keeper per market; the buyer remembers the
    merchant.
  - The economy stone: cap pouches that round-trip through the co-save.
  - Per-mind decay desync (`VaryNeeds`) plus the engine's per-tick decay
    jitter — a seeded RNG, persisted in the co-save.
  - Hardening: `DeleteGame` no longer kills a running world, and the walk
    probe reads the actor's data position instead of a lying 3D transform.

### Install

1. Install **F4SE** (Next-Gen, 0.7.8) and **Address Library**.
2. Copy `F4SE/` from this archive into your `Fallout 4/Data/` folder —
   the plugin lands at `Data\F4SE\Plugins\TheLivingCommonwealth.dll`.
3. The INI (`Data\F4SE\Plugins\TheLivingCommonwealth.ini`) is created
   with sane defaults on first run — every number in it is tunable.

### Notes for a beta

- Requires **Fallout 4 1.11.221 (Next-Gen)**.
- The name lists are the author's curated pools — swap them freely in the
  INI.
- The F4SE serialization UID is a placeholder; it only matters if the
  mod later reaches the F4SE plugin registry.

### Links

- Repo: <https://github.com/lelsliem/The-Commonwealth-Lives>
- The engine: <https://github.com/lelsliem/Living-Commonwealth-Engine-LCE->
