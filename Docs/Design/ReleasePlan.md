# Path to 1.0.0 — "The Release Plan"

**Status:** PLAN (2026-08-11) — the honest answer to "what makes this a
mod people want, what's missing, and what to cut." Written to be
disagreed with: every "keep" and every "cut" below is a decision the
author can overrule.

---

## The mod, on its own

The Living Commonwealth is not just the engine's proof — it is a mod in
its own right: **a settlement that visibly lives.** The player's people
get hungry and go to market, remember the weather, make friends and
enemies, feud and reconcile, get names, die, and are remembered. The
radio tells the player the stories. The co-save means it all survives
save/load. That's the product.

**The core insight for a release:** the mod is a *show*. The engine is
the stage machinery; what the player sees — people walking, talking,
arguing, trading, the radio narrating — is the product. Everything
serves the show.

## What players want (the release bar)

1. **The world visibly lives** — walking, talking, trading, rows, the
   occasional fight. This is the whole show.
2. **Their settlement is the stage** — their people, their benches,
   their stories; news centers on where the player actually is.
3. **The stories reach them** — the radio and HUD news are the channel;
   they must be throttled, readable, and never spammy.
4. **It doesn't break the game** — no crashes, no corrupted saves, no
   FPS tank at hundreds of settlers, no conflicts with their other mods.
5. **It stays out of the way** — no rename warfare, no popup flood, no
   settlers wandering off forever.

## What's missing (to add before 1.0.0)

**The show, in order of player value:**

| Stage | What | Why players want it |
|---|---|---|
| **0.7.1 — Talk** | The drafted dialogue pools (greet/gossip/family) speak on the social interactions the sim already makes; captions via the radio channel | Two settlers chatting at the bench is the first "it's alive" moment |
| **0.7.2 — Rows** | Verbal altercations: `dialogue.row` lines, Wronged outcomes, gossip spread | Feuds stop being log lines and become *heard* shouting |
| **0.7.3 — Names for everyone** | The memory prerequisite for trade: every unnamed sim-relevant mind gets an individual name. The role placeholders ("Provisioner", "Guard", "Minuteman") join "Settler"/"Workshop Worker" as generic, and a role-named mind is named "<role> <first>" ("Provisioner Daisy"). Prefer each actor's existing in-game name — never overwrite a real name with a pool draw | Two provisioners stop being interchangeable; a Trade memory can name its seller |
| **0.7.4 — Trade with anyone** | Vendor census: traders, marketplaces, provisioners as trade targets; caps change hands on the road (the core already resolves Trade to a person — no engine change, a who problem) | The world feels connected, not bench-to-bench; the player's provisioner is now part of the economy |
| **0.7.5 — Fights** | Physical escalation via real combat (the game's own punch/shove anims); feedback into bonds and news | The feud's payoff — the escalation line lands and they go at it |
| **0.7.6 — Fight-feel bug pass** | The known presentation bugs from the 0.7.5 field tests: the ghost-push slide (the fall's knock impulse dragging the victim after the kick), the both-fall look when the retaliation lands | The brawl reads as a real scuffle, not two collapses |
| **0.7.7 — Babies** | The sim's birth lifecycle made whole: a child born to a bonded pair, counted at wake, named, growing — the sim-only half (visible actors wait on 0.7.8) | Birth becomes a journey, not a log line |
| **0.7.8 — Baby & kid items** | Bottles, cribs, visible child actors via the external baby mod as a requirement — usable now, editable once permission lands | Children are *seen*, not just counted |
| **0.7.9 — Bugs & polish** | Every stone's field feedback folded in, docs reconciled, perf sanity at scale | The clean run into Illness |
| **0.8.0 — Illness & Medicine** | A `Health` component (adapter-owned, hold-then-recover — the engine's locked shape); radstorm/food/wound/contagion vectors; the sick rest and buy medicine; untreated sickness can die | The wasteland has a price beyond hunger — settlements feel seasons, the radio reports the ill, and medicine makes the economy real |

**The reliability gate (0.9.0, hard):**

- **Performance at scale, verified in-game** — the engine proved 5000
  minds round-trip and year-long soaks; the adapter must prove a normal
  save with hundreds of settlers ticks inside a frame budget. A release
  that chugs at 600 minds is dead on arrival. This is the *hard gate*
  for 1.0.0 — nothing ships before it.
- **The trust story** — remove-the-DLL behavior documented (the co-save
  rides in the save; the save still loads without the mod), clean
  migration, no stale-record corruption. The save/load proof is the
  mod's reputation.
- **Player docs** — a real README: what it does, install, tune, known
  limits, compatibility notes (Sim Settlements and other renaming mods,
  place-anywhere, etc.). The mod page copy.
- **News polish** — prioritize the player's settlement; keep the
  throttle sane; the radio stays the storyteller.

## What's NOT needed or wanted (cut — be honest)

These are the hard calls. The author can overrule any of them.

- **Cut: the "hands" pillar (build / move items / destroy) from 1.0.0.**
  The original 0.8.0 sketch (agency — settlers place objects, haul
  stock, clear clutter) is the riskiest and least-wanted piece: FO4's
  workshop placement path is crash-prone territory, it collides with
  the workshop/Sim Settlements ecosystem, and — the decisive point —
  **the player wants people, not construction**. The show is social:
  trade, talk, rows, fights. Build/move/destroy is a post-1.0 "hands
  in the world" expansion, if ever. This is the biggest cut and the
  most defensible one.
- **Cut: pex-driven scripted confrontation scenes.** Real combat covers
  fights with the game's own animations; a bespoke scene driver is CK
  asset work with no player-visible payoff over real combat. If a
  scripted shove ever matters, revisit post-1.0.
- **Cut: MCM from 1.0.0.** Honest as always: the INI already delivers
  every tuning knob; the MCM page needs the MCM mod + the Creation Kit
  (author-asset work, not sim work) and buys convenience, not show.
  Post-1.0, optional.
- **Cut: audio radio from 1.0.0.** Captions work and are verified; real
  voice needs assets. Deferred, as already scheduled — the radio is the
  channel, audio is dressing.
- **Cut: visible child actors.** Sim-only children stay (they exist,
  are fed, inherit memories — in the co-save), but spawning real child
  actors is a race/marker/idle can of worms with no show value.
  Post-1.0 if ever.
- **Already cut, don't revisit:** provisioner first-names (renaming
  guards/raiders isn't the point), pet-name collisions (unique per
  world), naming generic NPCs the player never owns.

## Life/death visibility — the author's two questions (2026-08-11)

Asked before the 0.7.1 test: *when one dies the body never moves —
wouldn't it be buried or disappear another way? and a birth child never
gets spawned — the whole life/death cycle?*

1. **The body must not linger forever.** The sim's death path is
   complete in the books (death fact, grief, legacy) but the game
   corpse stays in the settlement cell forever (no cell reset there).
   **Planned stone:** the burial — after the mourning window
   (`sim.death.burialDays`), the adapter disables the corpse ref and
   logs `the settlement laid X to rest` (+ a news line). Body state is
   game state — the adapter owns it, no engine change. Lands with the
   life/death polish, before 0.9.0's gate.
2. **A child is a mind, not a body — honestly, and with a reachable
   path.** Born children are sim-only by design: no FormRef, no actor.
   Making a child visible means a Creation Kit child template (race,
   markers, no-combat) — an author-asset stone, genuinely post-1.0.
   **The asset side already exists: Baby Sim - Babies That Grow Up
   (Nexus 100934)** — 10 babies, 20+ child variants, cribs, aging
   baby → child → young adult, grown children become settlers. It is
   player-driven (babies are crafted at a chemslab, placed, grown by
   the player), so our side is a *bridge*: the sim's birth event
   summons a Baby Sim child, names it, and ages it on our schedule.
   Two gates: the author's permission (assets require it; the author
   welcomes notification) and a hard mod dependency. Until then the
   visible story is the household: parents walk, eat, trade, grieve;
   the child is fed, bonded, named, and counted in the co-save. The
   sim-only child is the design, not a missing feature — noted so the
   cut is a decision, not an accident.

## The staged path to 1.0.0

```
0.7.0-0.7.5   shipped local (names, talk, rows, trade, fights)  ← we are here
0.7.6   Fight-feel bug pass — the scuffle reads as a scuffle
0.7.7   Babies — the sim's birth lifecycle made whole
0.7.8   Baby & kid items — bottles, cribs, visible children (external mod)
0.7.9   Bugs & polish — every stone's field feedback folded in
0.8.0   Illness & Medicine — health holds, recovers, or ends
0.9.0   The release gate — scale verified, docs, news polish
1.0.0   Freeze, ship
```

Each stage lands and is verified in-game before the next — the standing
rule. Every stage is a *small* stone; nothing in this plan is a giant
leap, and the only hard gate (scale) is measurable in a log.

## The honest one-liner

**1.0.0 is when the show is complete — people visibly living — and the
stage holds.** The engine proved the stage; the adapter proves the show.
Everything above either serves the show, hardens the stage, or is cut.
