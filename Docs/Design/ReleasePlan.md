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
| **0.7.3 — Fights** | Physical escalation via real combat (the game's own punch/shove anims); feedback into bonds and news | The feud's payoff — the escalation line lands and they go at it |
| **0.8.0 — Trade with anyone** | Vendor census: traders, marketplaces, provisioners as trade targets; caps change hands on the road | The world feels connected, not bench-to-bench; the player's provisioner is now part of the economy |

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

## The staged path to 1.0.0

```
0.7.0   shipped (names, feuds, radio)          ← we are here
0.7.1   Talk — pools speak, captions live
0.7.2   Rows — verbal altercations, gossip
0.7.3   Fights — real combat escalation
0.8.0   Trade with anyone — vendor census
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
