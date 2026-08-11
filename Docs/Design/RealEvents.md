# Real Events — "Trade With Anyone, Talk, and Fight"

**Milestone:** 0.8.0 (planned after 0.7.0 ships)
**Status:** PLAN (2026-08-11) — written before any code; the author's
vision, grounded in the seams the 0.5.0–0.7.0 stones already cut.
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

- **Mostly dialect** — words, not codex entries. In-game, the actors
  Say lines; in the log, the exchange reads as dialogue:
  ```
  LCE: settler 0001A4D7 to 0001CA7D: "Heard the market's open again."
  LCE: settler 0001CA7D to 0001A4D7: "Aye — and the rads are free."
  ```
- **Line pools in the INI** — the same pattern as the name pools:
  `dialogue.greet.*`, `dialogue.gossip.*`, `dialogue.row.*`, per
  species and per bond state (friends talk warmly, rivals talk cold).
  The author curates the words, like the names.
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

- **Mostly verbal.** A row is a conversation gone bad — a `dialogue.row`
  exchange, a Wronged outcome, disposition damage, gossip spread. Feuds
  are fought with words first: the settlement *hears* the shouting.
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
  outcome + gossip — on rival/enemy crossings and slights.
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
