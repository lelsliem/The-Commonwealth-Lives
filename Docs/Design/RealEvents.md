# Real Events — "Trade With Anyone, Talk, and Fight"

**Milestone:** 0.7.1–0.8.0 (staged — see ReleasePlan.md)
**Status:** 0.7.1 Talk and 0.7.2 Rows BUILT (2026-08-11) —
`Dialogue.h` + `Say` on the trade/family/slight sites, and the verbal
altercation: rivals and enemies crossing at the same bench row
(`src/Rows.h`, pure) — Wronged memories both ways (engine −0.25),
gossip spread, the row can push a pair over the feud line. In-game
verification (2026-08-12) found and fixed two
real seams: the row scan now crosses the stall-keeper directly (she
stands at the bench, so she never enters the walker attendance book),
and the workshop's props are no longer minds (turrets and spotlights
hold the settler faction and were seeding as Human — Deacon was found
feuding with four missile turrets). 0.7.4 Trade with anyone built and
verified in-game (2026-08-12). Pillar 3 (fights) BUILT (0.7.5,
2026-08-12) — `src/Fights.h` (pure): the physical escalation at the
row and the shut-stall slight — enemy pairs, the temper line and the
chance coin (sim.fight.chance, sim.fight.temper), the engine's Combat
wrong on both sides (the feud deepens; the victim carries a threat and
may flee the aggressor), gossip, the fight pool's words, and the news
("come to blows"). In-game verification pending; the punch/shove
animation is the polish step after verification.
Written before any code; the author's vision, grounded in the seams
the 0.5.0–0.7.0 stones already cut.
**Related:** Trade.md (the stall-keeper), SettlementMarkets.md (per-
settlement benches), Identity.md (feuds — the conflict source),
Behaviour.md (species profiles), Life.md (the original sketches).

---

## The vision

Today the market is a **bench**: a hungry mind walks to its
settlement's workshop and trades with the stall-keeper who set up
there. The economy works, but the world is bigger than benches —
and people are more than walkers. Three ideas make the events real:

1. **Trade with anyone who sells** — traders on the roads, marketplace
   vendors, provisioners hauling goods between settlements, and
   sometimes **each other**.
2. **Talk** — minds that meet say something. Conversations are mostly
   dialect: greetings, gossip, a row. Visible in the world and in the
   log.
3. **Altercations** — when a relationship goes bad, minds argue. Mostly
   words; **sometimes it turns physical** — and physical needs
   animations, which is where scripted scenes (pex) come in.

> The test plan is one sentence: *a hungry settler buys from a trader
> on the road, two friends stop to talk, and a feud ends in a
> shoving match.*

---

## Pillar 1 — Trade with anyone who sells

The walk target stops being only the bench. The arrival resolution
already discriminates **market** (FormRef only) vs **person** (a
translated mind with a SpeciesTag) — pillar 1 widens the *person* side
into real merchants.

| Who sells | What they are | How the sim finds them |
|---|---|---|
| **Traders** | Vanilla merchants (Carla, Cricket, Lucas Miller, …) — actors with vendor containers | A vendor census at the edge: scan loaded actors for vendor containers / merchant factions |
| **Marketplaces** | Diamond City Market, Bunker Hill — cells full of vendors | Treat the cell as a market entity whose "keeper" is whichever vendor the walker reaches first |
| **Provisioners** | Settlers on supply lines, already translated minds | They already carry Trade-kind memories; they become *mobile* traders — the walker meets them on the road |
| **Anyone who sells** | Any actor with a vendor container | The same vendor census — a mind trades with whoever sells, not just "the market" |
| **Each other** | Two settlers who remember each other | Already emergent (Trade.md: roles flip when two minds who remember each other meet) — formalized: a surplus mind can be a food source |

**Engine surface (all already exists):** `AcquireFood` ambitions, Trade
memories, `ChooseTarget` resolving to a person, `Trade, Success`
outcomes, cap pouches at the edge. No new goal types needed for pillar
1 — it is a *who* problem, not a *what* problem.

**Adapter work:**
- A **vendor census** (like the workshop census) — vendors are edge
  knowledge: vendor containers, merchant keywords, provisioner
  assignments.
- Seed a per-mind "who sells" memory alongside the market memory —
  nearest vendor within radius out-scores the bench for a hungry walker.
- A **marketplace cell** resolves like a stall: the first vendor
  reached is the trader.
- **Provisioners as mobile markets** — their Trade memory is already
  there; the walker targets the provisioner's *position*, not a bench.
- The co-save learns nothing new here (vendors are game actors, not sim
  entities) — the vendor census re-derives per session, exactly like the
  workshop census.

**Log lines:**
```
LCE: settler 0001CA7D trades with trader Carla at market 00099D20 — fed, caps paid.
LCE: settler 0001A4D7 trades with provisioner 0005FD2F on the road — fed, caps paid.
```

---

## Pillar 2 — Conversations

The sim already *feels* social — `Socialize` ambitions, Social
interactions, warmth between minds. Pillar 2 makes talking **visible**:
when two minds interact socially, they say something.

- **Simple one-liners, not codex entries.** Short, punchy, wasteland
  dialect — lines people actually say. The author's rule of thumb:
  if it takes more than ten words, cut it. Example exchange:
  ```
  LCE: settler 0001A4D7 to 0001CA7D: "Market's open again."
  LCE: settler 0001CA7D to 0001A4D7: "Aye. Rads are free."
  ```
- **Line pools in the INI** — the same pattern as the name pools,
  comma-separated lists the author curates, a seeded picker (like
  names) per mind so everyone doesn't say the same line. The starter
  sets are already drafted in the INI (2026-08-11): `dialogue.greet`
  (the good — "Mornin'", "Still breathing — good"), `dialogue.gossip`
  ("Heard about the market?", "There's trouble brewing"),
  `dialogue.row` (the bad — the author's ramp plus "You looking at
  me?", "Keep walking"), `dialogue.trade` ("Caps first", "Don't
  short me"), `dialogue.family` ("Dinner's ready", "Home's where
  the bench is"), `dialogue.grief` (the ugly — "Who feeds us now?",
  "The house is too quiet"), `dialogue.fight` ("Come on then!",
  "Put 'em up"), and `dialogue.feud` ("I'll remember this", "Don't
  turn your back").
- **Altercation lines escalate in order.** The author's starter set is
  the verbal→physical ramp in five lines: the complaint ("you ripped
  me off"), the dare ("you do that again, i dare you"), the taunt
  ("go on, one more time"), the break point ("i've had it with you"),
  and the escalation ("want some? let's go"). A row uses the early
  lines; a fight is earned when the escalation line lands.
- **Who talks:** two loaded minds near each other with a Social need;
  the walker and the trader after a trade; a couple at the bench; a
  family at dinner. The sim's existing Social interactions are the
  trigger — this pillar only adds the speech.
- **On-screen:** the same caption channel the radio uses — a
  conversation line pops as a subtitle while the player is near.
  (Reuses News/captions; nothing new to build for display.)

**Engine surface:** `Socialize` ambitions and Social interactions
already exist — talking is a presentation layer on an interaction that
already happens. No engine change.

**Adapter work:**
- Speech triggers in the interaction paths (arrival, trade, rest,
  family meal).
- INI line pools + a picker (seeded, like names).
- Say the line in-game (voice-less is fine — the caption carries it;
  real audio stays post-0.9.0 per the radio decision).

---

## Pillar 3 — Altercations

The feud machinery (Identity.md) already produces **slights → rival →
enemy**. Pillar 3 gives a feud *scenes*:

- **Mostly verbal (✅ built, 0.7.2 Rows).** A row is a conversation
  gone bad — a `dialogue.row` exchange (the author's five-line ramp),
  a Wronged outcome (engine Wronged, −0.25 each way), disposition
  damage, gossip spread. Feuds are fought with words first: the
  settlement *hears* the shouting. Rivals and enemies who cross paths
  at the same bench row (`Rows.h`); the shut-stall slight keeps its
  −0.1 channel (the executed let-down), the row is the unprompted
  wrong.
- **Sometimes physical.** When dispositions are deep enough (enemy) and
  tempers flare, an altercation escalates — a shove, a punch. The
  engine already has `Combat` as an InteractionKind; the adapter maps
  the escalation to real in-game combat or a scripted scene.
- **Animations.** Physical scenes need the game's animation system:
  either real combat (the game does the animating) or a scripted
  confrontation. This is where **pex files** come in if a scripted
  scene is the better look — a small Papyrus scene driver (compiled
  .pex, shipped alongside the DLL) that the adapter calls to play the
  confrontation: actors face, anims play, the fight resolves.
  Papyrus is the game's own scripting language — it is the natural
  tool for "play an animation sequence" that C++ does not own.

**Engine surface:** `Combat` and `Wronged` kinds exist; the feud
thresholds and the conflict source (Identity.md) are in and verified.
The escalation rule (verbal → physical) is adapter-side, tunable:
`sim.altercation.physical.chance` etc.

**Adapter work:**
- Verbal altercation: a row exchange (pillar 2's line pool) + Wronged
  outcome + gossip — on rival/enemy crossings and slights. ✅ built
  (0.7.2): the crossing row at the bench, once per pair per day.
- Physical escalation: a chance roll at enemy level; on success, real
  combat or a pex-driven confrontation scene.
- Consequences: the fight feeds back into bonds (a punch hurts trust)
  and news (the radio tells the settlement).

**Log lines:**
```
LCE: settler 0001A4D7 and 0001CA7D argue at the market — words first.
LCE: settler 0001A4D7 and 0001CA7D come to blows — the feud turns physical.
```

---

## The seams (what stays where)

| Piece | Where it lives |
|---|---|
| Trade with a vendor = Trade outcome | Engine (already there) |
| Row = Social/Wronged outcome | Engine (already there) |
| Fight = Combat outcome | Engine (already there) |
| Vendor census (who sells) | Adapter edge (like the workshop census) |
| Marketplace cell resolution | Adapter edge (like the stall-keeper map) |
| Line pools + picker | Adapter, INI (like the name pools) |
| Speech/captions on screen | Adapter (reuses the radio's caption channel) |
| Animation scenes / pex driver | Adapter ships the .pex; F4SE calls it |
| Escalation tuning | Adapter INI (`sim.altercation.*`) |

## Honest notes

- **Vendors are game actors, not sim minds** — a trader the sim buys
  from is *not* a translated mind (unless they're a settler/provisioner
  who happens to sell). The trade still reports `Trade, Success`; the
  vendor's pouch is game state, not co-save state. Only minds are
  citizens — vendors are scenery with merchandise. This is consistent
  with the species split: the sim never makes a vendor *decide*, it
  just buys from them.
- **pex needs the Creation Kit** — writing a Papyrus scene driver is an
  author-side asset task (compile .psc → .pex with the CK), like the
  audio radio and MCM before it. The adapter can call existing game
  scenes (combat, idle anims) with zero pex; the pex only adds
  bespoke *scripted* confrontations.
- **Verbal-first is a rule, not a nicety** — fights must be rare and
  earned: the feud line must already be crossed and tempers must roll.
  Otherwise every rival pair becomes a brawl and the world turns into
  a tavern.
- **Dialogue lines are content** — the pools need the author's words
  (like the name lists). Ships with a small curated set; grows like
  the names.

## Verification

- **Trade:** a settler's hungry walk resolves to a trader/provisioner
  (log shows the person, not the bench); caps paid; fed line fires.
- **Talk:** two minds with a Social need exchange lines; the caption
  shows on screen; the log reads as dialogue.
- **Altercation:** a rival pair argues (log + caption); an enemy pair
  sometimes escalates to blows (Combat outcome; animation plays);
  the fight lands in the news and cools the relationship further.
- **Co-save:** relationships survive save/load (already proven); the
  vendor census re-derives; dialogue pools are INI, not state.
