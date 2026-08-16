# Interactions — the in-world stone (0.8.7)

> The presentation rethink, in one line: **the sim's social life is told
> by the world itself, not by text on screen.** Two settlers who cross
> paths actually meet — they face each other, one voices a line to the
> other in the game's own voice, the other answers, they part. The player
> watches it happen. Text is demoted: the log keeps the record (the
> verify channel), the radio announces only the big events (per-category
> toggles, 0.8.7), and the curated pools become the *trigger map* — which
> moments exist — not the display.

---

## What's proven already

- **ProcessGreet is a stable audio seam.** The native greeting call
  (the same family as the kick's proven-safe PlayIdle) makes an actor
  voice a game line with real audio — stable across many sessions, no
  CTD. Tested so far with the target = the player ("lots of talk").
- **InteractPass already finds real crossing pairs.** It gates on
  genuinely-loaded actors (3D-present, so no streamed-out garbage
  positions), resting minds (never interrupting a walk), a per-mind
  jittered cadence, and a chance gate; then it picks the pool from the
  bond (family for spouses/sweethearts/friends, row for enemies/rivals,
  greet or gossip for everyone else). Today the tail is a `Say()` — text.
- **The physical acts already exist.** Market walks with real cap
  exchanges at stalls, kick fights with fall-and-flee, coughs, grief
  walks and burials. The sim's story is already partly watchable; the
  verbal layer is what still reads as text.

## What's untested — the probe that unlocks everything

- **ProcessGreet with target = the other NPC** (the A-greets-B
  exchange). We've only ever pointed it at the player.
- **Whether B answering on the next beat is a genuine two-way exchange**
  or double-talk — spacing and forceSub are the knobs.
- **Which (DIALOGUE_TYPE, DIALOGUE_SUBTYPE) combos voice which
  registers.** Greetings are proven (kMiscellaneous / kMisc_Hello).
  Rows, trades, and family talk may only be voiceable as the game's own
  barks — the probe maps the honest voiceable surface.

## The staged plan

### 0.8.7a — the A-greets-B probe
Extend the audio probe: A voices a greeting *at B* (ProcessGreet with
B as target), then B answers on the next tick (ProcessGreet with A as
target). Log both directions plus the game's chosen infos. One test
round answers: **two-way voiced exchange = the in-world seam.**
Double-talk or silence = adjust (space the two greets, forceSub on
one side, try other subtypes).

### 0.8.7b — the in-world greet/chat stone
InteractPass's `Say()` tail becomes the voiced exchange:
- When a pair crosses, A faces B and voices a line at B — **the game's
  words, the actor's voice, the game's own subtitles** (native, because
  it *is* native dialogue).
- The bond-based pool selection stays — it decides the *moment* (family
  talk, a quiet row, a greet) — but no longer the display text.
- New gates: skip actors busy in combat / dialogue / sleeping; one
  active exchange per area (a lock, so a crowd never all talks at
  once); per-pair cooldown so the same two don't re-greet every minute.
- The log records `X greeted Y (game line 0x…)` — the verify channel.

### 0.8.9-register — the bond names the register (built 2026-08-15)
The exchange asks the game for a register, not just a hello. The bond
names it before the game picks its words:

- **family** — a bonded household (spouse / sweetheart / friend). First
  asks the game for its general greeting (`kMisc_Greeting`), which may
  voice a different line where the voice has one; falls back to the
  proven hello if the game refuses outright (`family(hello)` in the
  log). A family exchange never goes silent.
- **flirt** — a compatible unpaired pair: two single, human adults,
  neither kin nor companion (the never-romance gate), warm to each
  other (shared disposition at/above the friend line). The moment is
  the flirt; the game voices its own words — the game has no distinct
  flirt INFO bank for NPC→NPC.
- **greet** — the crowd; the proven `kMisc_Hello`.

The register rides both exchange beats (the answer asks the same
register), and the log receipts name it: `X greeted Y — flirt
register, fired, played 0x…`. `sim.interact.register.family = 1`
turns the family subtype attempt off (0 = every exchange voices the
proven hello).

**Flirty Commonwealth evaluated, not integrated (2026-08-15):** the
mod's flirt greetings sit on the greeting topic (type 7 / `kMisc_Hello`
— the exact subtype our seam voices), so with it installed the game
can voice its lines for male speakers for free. But every line is
written for a female *player* target ("gal", "lady", "sister"), the
banks are male-only, and the content is explicit — so it stays an
optional player-side choice, never a dependency or content we build
on. The finding that matters: the greeting topic is a proven slot for
custom flirt lines if that ever becomes wanted.

### 0.8.8 — timings & weights (built 2026-08-15)
`sim.interact.*` keys, all in the INI and the MCM Interactions page:
- `cadence` (30 s) / `radius` (400 u) / `chance` (0.4) — as before.
- `pairCooldown` (60 s) — a SPECIFIC pair can't re-exchange within the
  window, so the same two settlers never greet twice a minute.
- `dailyCap` (0 = off) — how many interactions a mind may open per sim
  day before it goes quiet.
- `weight.greet / .gossip / .family / .row` — the pool pick is now a
  weighted roll: greet and gossip dominate the crowd, family is boosted
  ×10 for bonded pairs, a row stays the rare quiet line between
  friends — and a feud pair is a hard row whatever the weights say
  (their story is the physical escalation, not a warm hello).
So the show stays a show — a settlement that lives but never nags.
Field tuning pending.

### 0.8.9+ — the rest through the same seam
- **Rows**: the game voices what it can (the probe's subtype map); the
  physical escalation (kick / fall / flee) stays as it is.
- **Trades**: the market exchange gets a voiced line between buyer and
  keeper through the same seam.
- **Family / flirt**: a household pair voices at the shared meal; a
  compatible pair's exchange can nudge toward sweethearts.
- **LineCatalog** stays parked as the trigger map — it documents which
  game lines exist per register, so the seam knows what to ask for.

## The presentation contract

- **Interactions are watched, not read.** No HUD pop for a crossing
  exchange. The world is the display.
- **The game's own subtitles** appear during a voiced exchange — that's
  the native dialogue look, audio included.
- **The radio announces only the big events** — death, illness, birth,
  the bonds, fights, the market — each with its own off/on, subs, and
  audio toggle (already built).
- **The log is the verify channel**, never shown to players.

## Honest limits

- **The game chooses the words.** We choose the moment, the pair, the
  direction. An interaction is a short voiced exchange — the game's
  ambient life — not a scripted scene.
- **Not every register is voiceable by the greeting machinery.** The
  probe maps it; where the game has no voiced line, the moment stays
  physical with the log as the record.
- **Curated-line audio stays parked**: the only route that voices a
  *specific* INFO is the proven crasher (Say-via-VM, null-deref at the
  VM boundary). This stone doesn't reopen it.

## Decisions for review

1. **Proximity-only or anywhere?** Proposal: interactions run anywhere
   the sim ticks; the game only *shows* what's near the player. A
   settlement should already be mid-life when the player walks in, not
   start the moment they arrive.
2. **Frequency.** Proposal: a settler in company interacts roughly every
   few minutes of real time (tunable), so a settlement reads alive
   without chatter-noise.
3. **Any type to cut?** Flirt, rows, or family moments can be trimmed
   from the plan without touching the seam.
