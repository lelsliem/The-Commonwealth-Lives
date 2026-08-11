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
